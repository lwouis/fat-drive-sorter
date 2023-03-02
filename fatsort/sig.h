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
	This file contains/describes functions for signal handling.
*/

#ifndef __sig_h__
#define __sig_h__

#ifndef __WIN32__

// initialize signal handling for critical sections
void init_signal_handling(void);

// blocks signals for critical section
void start_critical_section(void);

// unblocks signals after critical section
void end_critical_section(void);


#else
#define init_signal_handling()
#define start_critical_section()
#define end_critical_section()
#endif

#endif // __sig_h__
