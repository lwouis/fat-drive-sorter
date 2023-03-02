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
	This file contains/describes functions for sorting of FAT filesystems.
*/

#ifndef __sort_h__
#define __sort_h__

#include <stdlib.h>
#include <stdint.h>
#include "FAT_fs.h"
#include "clusterchain.h"

// sorts FAT file system
int32_t sortFileSystem(char *filename);

// sorts the root directory of a FAT12 or FAT16 file system
int32_t sortFat1xRootDirectory(struct sFileSystem *fs);

// sorts directory entries in a cluster
int32_t sortClusterChain(struct sFileSystem *fs, uint32_t cluster, const char (*path)[MAX_PATH_LEN+1]);

// returns cluster chain for a given start cluster
int32_t getClusterChain(struct sFileSystem *fs, uint32_t startCluster, struct sClusterChain *chain);

// sorts exFAT directory entries in a cluster
int32_t sortExFATClusterChain(struct sFileSystem *fs, uint32_t cluster, uint32_t len, uint16_t isContigous, const char (*path)[MAX_PATH_LEN+1]);

#endif // __sort_h__
