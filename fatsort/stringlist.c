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
#include "stringlist.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include "errors.h"
#include "mallocv.h"

struct sStringList *newStringList() {
/*
	create a new string list
*/
	struct sStringList *stringList;

	// create the dummy head element
	stringList = malloc(sizeof(struct sStringList));
	if (stringList == NULL) {
		stderror();
		return NULL;
	}
	stringList->str = NULL;
	stringList->next = NULL;

	return stringList;
}

int32_t addStringToStringList(struct sStringList *stringList, const char *str) {
/*
	insert new string into string list
*/
	assert(stringList != NULL);
	assert(stringList->str == NULL);
	assert(str != NULL);

	int32_t len;

	// find end of list
	while (stringList->next != NULL) {
		stringList = stringList->next;
	}

	// allocate memory for new entry
	stringList->next=malloc(sizeof(struct sStringList));
	if (stringList->next == NULL) {
		stderror();
		return -1;
	}
	stringList->next->next = NULL;

	len=strlen(str);

	// allocate memory for string
	stringList->next->str=malloc(len+1);
	if (stringList->next->str == NULL) {
		stderror();
		return -1;
	}

	memcpy(stringList->next->str, str, len+1);

	return 0;

}

int32_t matchesStringList(struct sStringList *stringList, const char *str) {
/*
	evaluates whether str is contained in stringList
*/

	assert(stringList != NULL);
	assert(stringList->str == NULL);
	assert(str != NULL);

	int32_t ret=0; // not in list

	stringList=stringList->next;
	while (stringList != NULL) {
		if (strncmp(stringList->str, str, strlen(stringList->str)) == 0) {
			// contains a top level string of str
			ret=RETURN_SUB_MATCH;
		}
		if (strcmp(stringList->str, str) == 0) {
			// contains str exactly, so return immediately
			return RETURN_EXACT_MATCH;
		}
		stringList = stringList->next;
	}

	return ret;
}

void freeStringList(struct sStringList *stringList) {
/*
	free directory list
*/

	assert(stringList != NULL);

	struct sStringList *tmp;

	while (stringList != NULL) {
		if (stringList->str) free(stringList->str);
		tmp=stringList;
		stringList=stringList->next;
		free(tmp);
	}

}
