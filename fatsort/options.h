/*
	FATSort, utility for sorting FAT directory structures
	Copyright (C) 2018 Boris Leidner <fatsort(at)formenos.de>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
	This file contains/describes functions that parse command line options.
*/

#ifndef __options_h__
#define __options_h__

#include "FAT_fs.h"
#include "stringlist.h"
#include "regexlist.h"

extern uint32_t OPT_VERSION, OPT_HELP, OPT_INFO, OPT_QUIET, OPT_IGNORE_CASE,
		OPT_ORDER, OPT_LIST, OPT_REVERSE, OPT_FORCE, OPT_NATURAL_SORT,
		OPT_RECURSIVE, OPT_RANDOM, OPT_MORE_INFO, OPT_MODIFICATION,
		OPT_ASCII, OPT_REGEX;
extern struct sStringList *OPT_INCL_DIRS, *OPT_EXCL_DIRS, *OPT_INCL_DIRS_REC, *OPT_EXCL_DIRS_REC, *OPT_IGNORE_PREFIXES_LIST;
extern struct sRegExList *OPT_REGEX_INCL, *OPT_REGEX_EXCL;

extern char *OPT_LOCALE;

// parses command line options
int32_t parse_options(int argc, char *argv[]);

// evaluate whether str matches the include an exclude dir path lists or not
int32_t matchesDirPathLists(struct sStringList *includes,
				struct sStringList *includes_recursion,
				struct sStringList *excludes,
				struct sStringList *excludes_recursion,
				const char (*str)[MAX_PATH_LEN+1]);

// free options
void freeOptions();

#endif // __options_h__
