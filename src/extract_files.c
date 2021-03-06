/*
 *  tarix - a GNU/POSIX tar indexer
 *  Copyright (C) 2006 Matthew "Cheetah" Gabeler-Lee
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <fnmatch.h>

#include "config.h"

#include "debug.h"
#include "index_parser.h"
#include "lineloop.h"
#include "portability.h"
#include "tar.h"
#include "tarix.h"
#include "tstream.h"

struct extract_files_state
{
  int gotheader;
  struct index_parser_state ipstate;
#ifndef WITHOUT_DMSG
  int debug_messages;
#endif
  /* curpos always tracks block offsets */
  off64_t curpos;
  int zlib_level;
  t_streamp tsp;
  int outfd;
  /* flags to pass to fnmatch, if 0, don't use fnmatch */
  int glob_flags;
  /* if set, then matched items are excluded */
  int exclude_mode;
  int exact_match;
  const struct files_list_state *files_list;
};

int extract_files_lineloop_processor(char *line, void *data)
{
  struct extract_files_state *state = (struct extract_files_state*)data;
  const struct files_list_state* files_list = state->files_list;
  
  size_t i;
#ifndef WITHOUT_DMSG
  /* for the DMSG macro */
  int debug_messages = state->debug_messages;
#endif
  off64_t destoff;
  
  if (!state->gotheader)
  {
    if (init_index_parser(&state->ipstate, line) != 0)
      return 1;
    state->ipstate.allocate_filename = 0;
    state->gotheader = 1;
    return 0;
  }

  int extract = 0;
  int parse_result;
  struct index_entry entry;
  memset(&entry, 0, sizeof(entry));
  
  parse_result = parse_index_line(&state->ipstate, line, &entry);
  if (parse_result < 0)
    /* error */
    return 1;
  if (parse_result > 0)
    /* comment line */
    return 0;

  /* take action on the line */
  for (i = 0; i < files_list->argc; ++i)
  {
    if (state->glob_flags)
    {
      /* use fnmatch to test, instead of a simple compare */
      int mr = fnmatch(files_list->argv[i], entry.filename, state->glob_flags);
      if (mr == 0)
      {
        extract = 1;
        break;
      }
      if (mr != FNM_NOMATCH)
      {
        /* error in fnmatch */
        perror("glob match error");
        return 1;
      }
    }
    /* does the item match an extract arg? */
    else if (state->exact_match)
    {
      if (strcmp(files_list->argv[i], entry.filename) == 0)
      {
        extract = 1;
        break;
      }
    }
    /* does the start of the item match an extract arg? */
    else if (strncmp(files_list->argv[i], entry.filename, files_list->arglens[i]) == 0)
    {
      extract = 1;
      break;
    }
  }
  
  if (state->exclude_mode)
    extract = !extract;
  
  if (!extract)
    return 0;
    
  char passbuf[TARBLKSZ];
  
  DMSG("extracting %s\n", entry.filename);
  /* seek to the record start and then pass the record through */
  /* don't actually seek if we're already there */
  if (state->curpos != entry.blocknum)
  {
    destoff = state->zlib_level ? entry.offset : entry.blocknum * TARBLKSZ;
    DMSG("seeking to %lld\n", (long long)destoff);
    if (ts_seek(state->tsp, destoff) != 0)
    {
      fprintf(stderr, "seek error\n");
      return 1;
    }
    state->curpos = entry.blocknum;
  }
  DMSG("reading %ld records\n", entry.blocklength);
  for (unsigned long bnum = 0; bnum < entry.blocklength; ++bnum)
  {
    int n;
    if ((n = ts_read(state->tsp, passbuf, TARBLKSZ)) < TARBLKSZ)
    {
      if (n >= 0)
        perror("partial tarfile read");
      else
        ptserror("read tarfile", n, state->tsp);
      return 2;
    }
    DMSG("read a rec, now at %lld, %ld left\n",
      (long long)state->curpos, entry.blocklength - bnum - 1);
    ++state->curpos;
    if ((n = write(state->outfd, passbuf, TARBLKSZ)) < TARBLKSZ)
    {
      perror((n > 0) ? "partial tarfile write" : "write tarfile");
      return 2;
    }
    DMSG("wrote rec\n");
  }
  
  return 0;
}

int extract_files(const char *indexfile, const char *tarfile,
  const char *outfile, int use_mt, int zlib_level, int debug_messages,
  int glob_flags, int exclude_mode, int exact_match,
  const struct files_list_state *files_list)
{
  int index, tar, outfd;
  struct extract_files_state state;
  
  memset(&state, 0, sizeof(state));
  
  /* the basic idea:
   * read the index an entry at a time
   * scan the index entry against each arg and pass through the file if
   * it matches
   */
  
  /* open the index file */
  if ((index = open(indexfile, O_RDONLY)) < 0) {
    perror("open indexfile");
    return 1;
  }
  /* maybe warn user about no largefile on stdin? */
  if (tarfile == NULL) {
    /* stdin */
    tar = 0;
  } else {
    if ((tar = open(tarfile, O_RDONLY|P_O_LARGEFILE)) < 0) {
      perror("open tarfile");
      return 1;
    }
  }
  if (outfile == NULL) {
    /* stdout */
    outfd = 1;
  } else {
    if ((outfd = open(outfile, O_CREAT|O_TRUNC|O_WRONLY, 0666)) < 0) {
      perror("open outfile");
      return 1;
    }
  }
  
  /* tstream handles base offset */
  state.tsp = init_trs(NULL, tar, use_mt, TARBLKSZ, zlib_level);
  if (state.tsp->zlib_err != Z_OK) {
    fprintf(stderr, "zlib init error: %d\n", state.tsp->zlib_err);
    return 1;
  }
  
#ifndef WITHOUT_DMSG
  state.debug_messages = debug_messages;
#endif
  state.zlib_level = zlib_level;
  state.glob_flags = glob_flags;
  state.exclude_mode = exclude_mode;
  state.exact_match = exact_match;
  state.files_list = files_list;
  state.outfd = outfd;
  
  lineloop(index, extract_files_lineloop_processor, (void*)&state);
  
  return 0;
}
