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
	This file contains/describes functions to manage string lists.
*/

#ifndef __stringlist_h__
#define __stringlist_h__

#include <stdint.h>
#include "FAT_fs.h"

struct sStringList {
	char *str;
	struct sStringList *next;
};

// defines return values for function matchesStringList
#define RETURN_NO_MATCH 0
#define RETURN_EXACT_MATCH 1
#define RETURN_SUB_MATCH 2

// create a new string list
struct sStringList *newStringList();

// insert new directory path into directory path list
int32_t addStringToStringList(struct sStringList *stringList, const char *str);

// evaluates whether str is contained in strList
int32_t matchesStringList(struct sStringList *stringList, const char *str);

// free string list
void freeStringList(struct sStringList *stringList);

#endif //__stringlist_h__
