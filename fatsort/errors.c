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
	This file contains/describes functions for error handling and messaging.
*/

#include <stdio.h>
#include <stdarg.h>
#include "errors.h"
#include "mallocv.h"

void errormsg(const char *func, const char *str, ...) {
/*
	 error messages with function name and argument list
*/
	char msg[129];
	va_list argptr;

	va_start(argptr,str);
	vsnprintf(msg, 128, str, argptr);
	fprintf(stderr, "%s: %s\n", func, msg);
	va_end(argptr);

}
