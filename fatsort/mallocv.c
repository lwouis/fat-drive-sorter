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
	This file contains/describes debug versions for malloc and free
*/
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include "mallocv.h"
#include "errors.h"

#undef malloc
#undef realloc
#undef free

int64_t mem_used=0;

struct sList {
	void *ptr;
	size_t size;
	char*file;
	uint32_t line;
	struct sList *next;
};

struct sList list={0};

void add(struct sList *list, void *ptr, size_t size, char *filename, uint32_t line) {

	assert(list != NULL);
	assert(ptr != NULL);
	

	while(list->next != NULL) {
		list = list->next;
	}
	
	list->next = malloc(sizeof(struct sList));
	if (list->next == NULL) {
		stderror();
		exit(1);
	}
	
	list->next->ptr=ptr;
	list->next->size=size;
	
	size_t flen=strlen(filename);

	list->next->file=malloc(flen+1);
	if (list->next->file == NULL) {
		stderror();
		exit(1);
	}
	strncpy(list->next->file, filename, flen);
	list->next->file[flen] = '\0';
	
	list->next->line=line;
	
	list->next->next=NULL;
	
}

size_t del(struct sList *list, void *ptr) {
	
	assert(list != NULL);
	assert(ptr != NULL);
	
	size_t size;
	struct sList *tmp;
	
	while(list->next != NULL) {
		if (list->next->ptr == ptr) {
			size=list->next->size;
			//free(list->next->ptr);
			free(list->next->file);
			tmp=list->next->next;
			free(list->next);
			list->next=tmp;
			return size;
		}
		list=list->next;
	}
	return 0;
}

void *mallocv(char *filename, uint32_t line, size_t size) {
	
	void* ptr = malloc(size);

	if (size && (ptr == NULL)) {
		stderror();
		exit(1);		
	}
	
	add(&list, ptr, size, filename, line);
	
	mem_used += size;
	
#if DEBUG >= 3
	myerror("%d bytes allocated in %s:%u, ptr=%x (total memory %d)!", size, filename, line, ptr, mem_used);
#endif
	
	return ptr;
}

void *reallocv(char *filename, uint32_t line, void *ptr, size_t size) {
	
	
	size_t delSize=0;

	if (ptr) {
		delSize=del(&list, ptr);
		if (!delSize) {
#if DEBUG >= 4
			myerror("No memory allocated for ptr %x freed (realloc) at %s:%u!", ptr, filename, line);
#endif
		}
		mem_used -= delSize;
	}

	void* newPtr = realloc(ptr, size);

	if (size && (newPtr == NULL)) {
		stderror();
		exit(1);		
	}

	if (newPtr)
		add(&list, newPtr, size, filename, line);
	
	mem_used += size;
	
#if DEBUG >= 3
	myerror("%d bytes reallocated in %s:%u, old ptr=%x, new ptr=%x (total memory %d)!", size-delSize, filename, line, ptr, newPtr, mem_used);
#endif

	return newPtr;
}

void freev(char *filename, uint32_t line, void *ptr) {
	
	size_t size=del(&list, ptr);
	if (size) {
		mem_used-=size;
		free(ptr);
#if DEBUG >= 3		
		myerror("%d bytes freed in %s:%u (total memory %d)!", size, filename, line, mem_used);
#endif
	} else {
#if DEBUG >= 4
		myerror("No memory allocated for ptr %x freed at %s:%u!", ptr, filename, line);
#endif
	}
}

void reportLeaks() {
	struct sList *tmp=&list;
	while(tmp->next != NULL) {
		myerror("%d bytes allocated for ptr %x at %s:%u were not freed!",
			tmp->next->size, tmp->next->ptr, tmp->next->file, tmp->next->line);
		tmp = tmp->next;
	}
	myerror("Total bytes not freed: %d", mem_used);
}

