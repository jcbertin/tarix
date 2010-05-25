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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tarix.h"
#include "index_parser.h"

int init_index_parser(struct index_parser_state *state, char *header) {
  if (sscanf(header, "TARIX INDEX v%d GENERATED BY ", &state->version) != 1) {
    fprintf(stderr, "Index header not recognized\n");
    return 1;
  }
  if (state->version < 0 || state->version > TARIX_FORMAT_VERSION) {
    fprintf(stderr, "Index version %d not supported\n", state->version);
    return 1;
  }
  state->last_num = -1;
  return 0;
}

int parse_index_line(struct index_parser_state *state, char *line, struct index_entry *entry) {
  /* games with longlong to avoid warnings on 64bit */
  long long lltmp;
  int fnpos, ssret, sscount;
  
  entry->version = state->version;
  entry->num = ++state->last_num;
  if (entry->filename_allocated)
  {
    free(entry->filename);
    entry->filename = NULL;
    entry->filename_allocated = 0;
  }
  if (line[0] == '#')
  {
    entry->recordtype = '#';
    entry->blocknum = -1;
    entry->blocklength = -1;
    entry->offset = -1;
    return 0;
  }
  
  switch (state->version) {
    case 0:
      sscount = 2;
      ssret = sscanf(line, "%ld %ld %n", &entry->blocknum, &entry->blocklength, &fnpos);
      break;
    case 1:
      sscount = 3;
      ssret = sscanf(line, "%ld %lld %ld %n", &entry->blocknum, &lltmp,
        &entry->blocklength, &fnpos);
      entry->offset = lltmp;
      break;
    case 2:
      sscount = 4;
      ssret = sscanf(line, "%c %ld %lld %ld %n", &entry->recordtype, &entry->blocknum,
        &lltmp, &entry->blocklength, &fnpos);
      entry->offset = lltmp;
      break;
    default:
      fprintf(stderr, "Index version %d not supported\n", state->version);
      return 1;
  }
  
  if (ssret != sscount) {
    fprintf(stderr, "index format error: v%d expects %d, got %d\n",
      state->version, sscount, ssret);
    return 1;
  }
  
  if (state->allocate_filename)
  {
    entry->filename = strdup(line + fnpos);
    entry->filename_allocated = 1;
  }
  else
  {
    entry->filename = line + fnpos;
    entry->filename_allocated = 0;
  }
  
  return 0;
}
