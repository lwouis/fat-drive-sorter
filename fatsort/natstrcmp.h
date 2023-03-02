/*
	FATSort, utility for sorting FAT directory structures
	Copyright (C) 2008 Boris Leidner <fatsort(at)formenos.de>

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
	This file contains/describes functions for natural order sorting.
*/

#ifndef __natstrcmp_h__
#define __natstrcmp_h__

#include <stdint.h>

// natural order comparison
int32_t natstrcmp(const char *str1, const char *str2);

// natural order comparison ignoring case
int32_t natstrcasecmp(const char *str1, const char *str2);

#endif // __natstrcmp_h__
