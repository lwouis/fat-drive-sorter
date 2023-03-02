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
	These functions are used to convert endianness of integers.
*/

#include "endianness.h"
#include "mallocv.h"

// Endian swap functions
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

uint16_t SwapInt16(uint16_t value) {
/*
	swaps endianness of a 16 bit integer
*/
	union {
		uint16_t ivalue;
		char cvalue[2];
	} u;

	u.ivalue=value;

	char tmp;
	tmp=u.cvalue[0];
	u.cvalue[0]=u.cvalue[1];
	u.cvalue[1]=tmp;

	return u.ivalue;
}

uint32_t SwapInt32(uint32_t value) {
/*
	swaps endianness of a 32 bit integer
*/
	union {
		uint32_t ivalue;
		char cvalue[4];
	} u;

	u.ivalue=value;

	char tmp;
	tmp=u.cvalue[0];
	u.cvalue[0]=u.cvalue[3];
	u.cvalue[3]=tmp;
	tmp=u.cvalue[1];
	u.cvalue[1]=u.cvalue[2];
	u.cvalue[2]=tmp;

	return u.ivalue;
}

uint64_t SwapInt64(uint64_t value) {
/*
	swaps endianness of a 64 bit integer
*/
	union {
		uint64_t ivalue;
		char cvalue[8];
	} u;

	u.ivalue=value;

	char tmp;
	tmp=u.cvalue[0]; u.cvalue[0]=u.cvalue[7]; u.cvalue[7]=tmp;
	tmp=u.cvalue[1]; u.cvalue[1]=u.cvalue[6]; u.cvalue[6]=tmp;
	tmp=u.cvalue[2]; u.cvalue[2]=u.cvalue[5]; u.cvalue[5]=tmp;
	tmp=u.cvalue[3]; u.cvalue[3]=u.cvalue[4]; u.cvalue[4]=tmp;

	return u.ivalue;
}
#endif
