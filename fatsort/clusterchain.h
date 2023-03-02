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
	This file contains/describes the cluster chain ADO with its structures and
	functions. Cluster chain ADOs hold a linked list of cluster numbers.
	Together all clusters in a cluster chain hold the date of a file or a
	directory in a FAT filesystem.
*/

#ifndef __clusterchain_h__
#define __clusterchain_h__

#include <stdint.h>

struct sClusterChain {
/*
	this structure contains cluster chains
*/
	uint32_t cluster;
	struct sClusterChain *next;
};

// create new cluster chain
struct sClusterChain *newClusterChain(void);

// allocate memory and insert cluster into cluster chain
int32_t insertCluster(struct sClusterChain *chain, uint32_t cluster);

// free cluster chain
void freeClusterChain(struct sClusterChain *chain);


#endif // __clusterchain_h__
