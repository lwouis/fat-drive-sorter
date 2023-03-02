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
#include "regexlist.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include "errors.h"
#include "mallocv.h"

struct sRegExList *newRegExList() {
/*
	create a new string list
*/
	struct sRegExList *regExList;

	// create the dummy head element
	regExList = malloc(sizeof(struct sRegExList));
	if (regExList == NULL) {
		stderror();
		return NULL;
	}
	regExList->regex = NULL;
	regExList->next = NULL;

	return regExList;
}

int32_t addRegExToRegExList(struct sRegExList *regExList, const char *regExStr) {
/*
	insert new regular expression into directory path list
*/
	assert(regExList != NULL);
	assert(regExList->regex == NULL);
	assert(regExStr != NULL);

	int32_t ret;
	char errbuf[128];

	// find end of list
	while (regExList->next != NULL) {
		regExList = regExList->next;
	}

	// allocate memory for new entry
	regExList->next=malloc(sizeof(struct sRegExList));
	if (regExList->next == NULL) {
		stderror();
		return -1;
	}
	regExList->next->next = NULL;

	// allocate memory for regex
	regExList->next->regex=malloc(sizeof(regex_t));
	if (regExList->next->regex == NULL) {
		stderror();
		return -1;
	}

	// compilete regex
	ret=regcomp(regExList->next->regex, regExStr, REG_EXTENDED | REG_NOSUB);
	if (ret) {
		regerror(ret, regExList->next->regex, errbuf, 128);
		myerror("Failed to compile regular expression \"%s\": %s!", regExStr, errbuf);
		return -1;
	}

	return 0;

}

int32_t matchesRegExList(struct sRegExList *regExList, const char *str) {
/*
	evaluates whether str matches regular expressions in regExList
*/

	assert(regExList != NULL);
	assert(regExList->regex == NULL);
	assert(str != NULL);

	regmatch_t pmatch[0];

	regExList=regExList->next;
	while (regExList != NULL) {

		// return on first match with success
		if (!regexec(regExList->regex, str, 1, pmatch, 0)) {
			return RETURN_MATCH;
		}

		regExList = regExList->next;
	}

	return RETURN_NO_MATCH;
}

void freeRegExList(struct sRegExList *regExList) {
/*
	free regExList
*/

	assert(regExList != NULL);

	struct sRegExList *tmp;

	while (regExList != NULL) {
		if (regExList->regex) {
				regfree(regExList->regex);
				free(regExList->regex);
		}
		tmp=regExList;
		regExList=regExList->next;
		free(tmp);
	}

}
