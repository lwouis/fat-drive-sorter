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

#ifndef __errors_h__
#define __errors_h__

#include <stdarg.h>
#include <string.h>
#include <errno.h>

// macros
#if DEBUG >= 1
#define DEBUGMSG(msg...) errormsg("DEBUG", msg);
#else
#define DEBUGMSG(msg...)
#endif
#define myerror(msg...) errormsg(__func__, msg);
#define stderror() errormsg(__func__, "%s!", strerror(errno));

// error messages with function name and argument list
void errormsg(const char *func, const char *str, ...);

#endif // __errors_h__
