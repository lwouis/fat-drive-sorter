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
	This file contains platform-independent device i/o functions
	for UNIX/Linux/Windows
*/

#ifndef __deviceio_h__
#define __deviceio_h__

#include <stdint.h>

#if defined __LINUX__ || defined __BSD__ || defined __OSX__ 

#define DIRECTORY_SEPARATOR '/'

typedef struct {
  int fd;
} DEVICE;

#elif defined __WIN32__

#define DIRECTORY_SEPARATOR '\\'

#include <windows.h>

#define SECTOR_NONE 0xffffffffffffffff

typedef struct {
  HANDLE h;
  int isDrive;
  uint16_t sectorSize;
  uint64_t currentOffset;
  char *buffer;
  uint64_t bufferedSector;
  int unwrittenChanges;
} DEVICE;

#else
  #error Unsupported OS!
#endif

// opens a device
DEVICE *device_open(const char *path);

// performs a seek inside an open device
int64_t device_seekset(DEVICE *device, int64_t offset);

// reads data from a device
int64_t device_read(DEVICE *device, void *data, uint64_t size, uint64_t n);

// writes data to a device
int64_t device_write(DEVICE *device, const void *data, uint64_t size, uint64_t n);

// ensures that all pending data writes are performed
int device_sync(DEVICE *device);

// closes a device
int device_close(DEVICE *device);

#endif	// __deviceio_h__
