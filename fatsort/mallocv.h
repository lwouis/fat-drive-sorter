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

#ifndef __mallocv_h__
#define __mallocv_h__

#include <stdlib.h>
#include <stdint.h>

#if DEBUG >= 2
#define malloc(size) mallocv(__FILE__, __LINE__, size)
#define realloc(ptr, size) reallocv(__FILE__, __LINE__, ptr, size)
#define free(ptr) freev(__FILE__, __LINE__, ptr)
#define REPORT_MEMORY_LEAKS reportLeaks();
void *mallocv(char *filename, uint32_t line, size_t size);
void *reallocv(char *filename, uint32_t line, void *ptr, size_t size);
void freev(char *filename, uint32_t line, void *ptr);
void reportLeaks();
#else
#define REPORT_MEMORY_LEAKS
#endif

#endif // __mallocv_h__
