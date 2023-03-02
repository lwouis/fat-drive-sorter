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
	This file contains/describes functions to manage lists of regular expressions.
*/

#ifndef __regexlist_h__
#define __regexlist_h__

#ifndef __WIN32__
// feature: implement regex for windows
#include <regex.h>
#else
#define regcomp
#define regfree
#define regexec
#define regex_t int
#define regmatch_t int
#define regerror
#define REG_EXTENDED 1
#define REG_NOSUB 1
#endif

#include <stdint.h>
#include "FAT_fs.h"

struct sRegExList {
	regex_t *regex;
	struct sRegExList *next;
};

// defines return values for function matchesRegExList
#define RETURN_NO_MATCH 0
#define RETURN_MATCH 1

// create a new string list
struct sRegExList *newRegExList();

// insert new regular expression into directory path list
int32_t addRegExToRegExList(struct sRegExList *regExList, const char *regExStr);

// evaluates whether str matches regular expressions in regExList
int32_t matchesRegExList(struct sRegExList *regExList, const char *str);

// free regExList
void freeRegExList(struct sRegExList *regExList);

#endif //__regexlist_h__
