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

#include "sig.h"

#include <stdlib.h>
#include <signal.h>
#include "mallocv.h"

#ifndef __WIN32__

sigset_t blocked_signals_set;

void init_signal_handling(void) {
/*
	initialize signal handling for critical sections
*/
	sigfillset(&blocked_signals_set);
}

void start_critical_section(void) {
/*
	blocks signals for critical section
*/
	sigprocmask(SIG_BLOCK, &blocked_signals_set, NULL);
}

void end_critical_section(void) {
/*
	unblocks signals after critical section
*/
	sigprocmask(SIG_UNBLOCK, &blocked_signals_set, NULL);
}

#endif

