/*
 *  tarix - a GNU/POSIX tar indexer
 *  Copyright (C) 2002 Matthew "Cheetah" Gabeler-Lee
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
                     
#include "tarix.h"

int show_help()
{
	fprintf(stderr,
		"Usage: tarix [-hix] [-f index_file] [-t tarfile]\n"
		"  -h   Show this help\n"
		"  -i   Explicitly create index, don't pass tar data to stdout\n"
		"  -x   Use index to extract tar file\n"
		"  -f   Set index file to use\n"
		"  -t   Set tar file to use\n"
		"  -m   Use mt (magnetic tape) IOCTLs for seeking instead of lseek\n"
		"\n"
		"The default action is to create an index and pass the tar data through\n"
		"to stdout so that tarix can be used with tar's --use-compress-program\n"
		"option.\n"
		"\n"
		"If the -i option is used, the tar file will not be written to stdout.\n"
		"If -t is not specified, stdin is used for reading the tar file\n"
		"If -f is not specified, the environment variable TARIX_OUTFILE will\n"
		"determine the ouput location of the index file.  If that is not set,\n"
		"out.tarix will be used\n");
	return 0;
}

enum tarix_action
{
	CREATE_INDEX,
	SHOW_HELP,
	EXTRACT_FILES
};

int main(int argc, char *argv[])
{
	int action = CREATE_INDEX;
	int opt;
	char *indexfile = NULL;
	char *tarfile = NULL;
	int pass_through = 1;
	int use_mt = 0;
	
	/* parse opts, do right thing */
	while ((opt = getopt(argc, argv, "himf:t:x")) != -1)
	{
		switch (opt)
		{
			case 'h':
				action = SHOW_HELP;
				break;
			case 'i':
				action = CREATE_INDEX;
				pass_through = 0;
				break;
			case 'm':
				use_mt = 1;
				break;
			case 'f':
				if (indexfile)
					free(indexfile);
				indexfile = (char*)malloc(strlen(optarg) + 1);
				strcpy(indexfile, optarg);
				break;
			case 't':
				if (tarfile)
					free(tarfile);
				tarfile = (char*)malloc(strlen(optarg) + 1);
				strcpy(tarfile, optarg);
				break;
			case 'x':
				action = EXTRACT_FILES;
				break;
			case ':':
				fprintf(stderr, "Missing arg to '%c' option\n", optopt);
				show_help();
				return 1;
				break;
			case '?':
				fprintf(stderr, "Unrecognized option '%c'\n", optopt);
				show_help();
				return 1;
				break;
			default:
				fprintf(stderr, "EEK! getopt returned an unrecognized value\n");
				return 1;
				break;
		}
	}
	
	if (!indexfile)
		indexfile = getenv("TARIX_OUTFILE");
	if (!indexfile)
		indexfile = TARIX_DEF_OUTFILE;

	switch (action)
	{
		case CREATE_INDEX:
			return create_index(indexfile, tarfile, pass_through);
		case SHOW_HELP:
			return show_help();
		case EXTRACT_FILES:
			return extract_files(indexfile, tarfile, use_mt, argc, argv, optind);
			return 1;
		default:
			fprintf(stderr, "EEK! unknown action!\n");
			return 1;
	}
	return -1;
}
