/*
	FATDefrag, utility for defragmentation of FAT file systems
	Copyright (C) 2013 Boris Leidner <fatdefrag(at)formenos.de>

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
	This file contains/describes functions that are used to read, write, check,
	and use FAT filesystems.
*/

#include "FAT_fs.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/param.h>
#include <iconv.h>
#include <string.h>

#include "errors.h"
#include "endianness.h"
#include "deviceio.h"
#include "mallocv.h"

// used to check if device is mounted
#if defined(__LINUX__)
#include <mntent.h>
#elif defined (__BSD__) || defined (__OSX__)
#include <sys/ucred.h>
#include <sys/mount.h>
#endif

int32_t check_mounted(char *filename) {
/*
	check if filesystem is already mounted
*/

#if defined(__LINUX__)
	FILE *fd;
	struct mntent *mnt;
	int32_t ret = 0;
	char rp_filename[MAXPATHLEN+1], rp_mnt_fsname[MAXPATHLEN+1];

	if ((fd = setmntent("/proc/self/mounts", "r")) == NULL) {
		stderror();
		return -1;
	}

	// get real path
	if (realpath(filename, rp_filename) == NULL) {
		myerror("Unable to get realpath of filename!");
		return -1;
	}

	while ((mnt = getmntent(fd)) != NULL) {
		if (realpath(mnt->mnt_fsname, rp_mnt_fsname) != NULL) {
			if (strcmp(rp_mnt_fsname, rp_filename) == 0) {
				ret = 1;
				break;
			}
		}
	}

	if (endmntent(fd) != 1) {
		myerror("Closing mtab failed!");
		return -1;
	}

	return ret;

#elif defined(__BSD__) || defined(__OSX__)
	struct statfs *mntbuf;
	int i, mntsize;
	int32_t ret = 0;
	char rp_filename[MAXPATHLEN], rp_mnt_fsname[MAXPATHLEN+1];

	// get real path
	if (realpath(filename, rp_filename) == NULL) {
		myerror("Unable to get realpath of filename!");
		return -1;
	}

	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);

	if (mntsize == 0) {
		stderror();
		return -1;
	}

	for (i = mntsize - 1; i >= 0; i--) {
		realpath(mntbuf[i].f_mntfromname, rp_mnt_fsname);
		if (strcmp(rp_mnt_fsname, rp_filename) == 0) {
			ret = 1;
			break;
		}
	}

	return ret;
#else
	// ok, we don't know how to check this on an unknown platform
	myerror("Don't know how to check whether filesystem is mounted! Use option '-f' to sort nonetheless.");

	return -1;
#endif
}

#define _53_ZEROS \
		"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" \
		"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" \
		"\0\0\0"

int32_t check_bootsector(struct sBootSector *bs) {
/*
	lazy check if this is really a FAT bootsector
*/

	assert(bs != NULL);

	if (!((bs->BS_JmpBoot[0] == 0xeb) &&
	     (bs->BS_JmpBoot[2] == 0x90)) &&
		!(bs->BS_JmpBoot[0] == 0xe9)) {
		// boot sector does not begin with specific instruction
 		myerror("Boot sector does not begin with jump instruction!");
		return -1;
	} else if (SwapInt16(bs->BS_EndOfBS) != 0xaa55) {
		// end of boot sector marker is missing
		myerror("End of boot sector marker is missing!");
		return -1;
	}

	// exFAT
	if (!memcmp(bs->BS_OEMName, "EXFAT   ", 8)) {
		// exFAT file system!
		if (memcmp(bs->xxFATxx.exFAT.__unused1, _53_ZEROS, 53)) {
			// some bytes are not zero
			myerror("Unused bytes after OEM Name must be zero in exFAT Volume Boot Record!");
			return -1;
		} else if ((bs->xxFATxx.exFAT.sector_bits < 9) || (bs->xxFATxx.exFAT.sector_bits > 12)) {
			myerror("exFAT sector size must be between 512 and 4096 bytes but is %u!",
				1 << bs->xxFATxx.exFAT.sector_bits);
			return -1;
		} else if (bs->xxFATxx.exFAT.sector_bits + bs->xxFATxx.exFAT.spc_bits > 25) {
			myerror("Maximum allowed cluster size is 32 MiB but we have %u bytes per sector and %u bytes per cluster!",
				1 << bs->xxFATxx.exFAT.sector_bits, (1 << bs->xxFATxx.exFAT.spc_bits) * 512);
			return -1;
		} else if (bs->xxFATxx.exFAT.fat_count != 1) {
			myerror("FAT count %u is not supported!", bs->xxFATxx.exFAT.fat_count);
			return -1;
		} else if ((bs->xxFATxx.exFAT.version.major != 0x01) || (bs->xxFATxx.exFAT.version.minor != 0x00)) {
			myerror("exFAT version %u.%u is not supported",
				bs->xxFATxx.exFAT.version.major,
				bs->xxFATxx.exFAT.version.minor);
			return -1;
		} else if (SwapInt32(bs->xxFATxx.exFAT.fat_sector_start) >= SwapInt64(bs->xxFATxx.exFAT.sector_count)) {
			myerror("FAT start sector (%u) >= sector count (%u)!",
				SwapInt32(bs->xxFATxx.exFAT.fat_sector_start),
				SwapInt64(bs->xxFATxx.exFAT.sector_count));
			return -1;
		} else if (SwapInt32(bs->xxFATxx.exFAT.cluster_count) >= 0xFFFFFFF6) {
			myerror("Cluster count (%u) is too big!",
				SwapInt32(bs->xxFATxx.exFAT.cluster_count),
				SwapInt64(bs->xxFATxx.exFAT.cluster_count));
			return -1;
		} else if (SwapInt32(bs->xxFATxx.exFAT.rootdir_cluster) > SwapInt32(bs->xxFATxx.exFAT.cluster_count)+1) {
			myerror("Root directory cluster number (%u) >= cluster count (%u)!",
				SwapInt32(bs->xxFATxx.exFAT.rootdir_cluster),
				SwapInt32(bs->xxFATxx.exFAT.cluster_count));
			return -1;
		} else if (SwapInt64(bs->xxFATxx.exFAT.sector_count) <= SwapInt32(bs->xxFATxx.exFAT.cluster_sector_start)) {
			myerror("Cluster heap starts at sector (%u) but total sector count in volume is %u!",
				SwapInt32(bs->xxFATxx.exFAT.cluster_sector_start),
				SwapInt64(bs->xxFATxx.exFAT.sector_count));
			return -1;
		}
		// future checks:
		// volume size >= cluster offset + cluster space
		// FAT size large enough for cluster space
		// FAT offset, FAT size and cluster offset consistent

	// VFAT
	} else if (SwapInt16(bs->xxFATxx.FAT12_16_32.BS_BytesPerSec) == 0) {
		myerror("Sectors have a size of zero!");
		return -1;
	} else if (SwapInt16(bs->xxFATxx.FAT12_16_32.BS_BytesPerSec) % 512 != 0) {
		myerror("Sector size is not a multiple of 512 (%u)!",
			SwapInt16(bs->xxFATxx.FAT12_16_32.BS_BytesPerSec));
		return -1;
	} else if (bs->xxFATxx.FAT12_16_32.BS_SecPerClus == 0) {
		myerror("Cluster size is zero!");
		return -1;
	} else if (bs->xxFATxx.FAT12_16_32.BS_SecPerClus * SwapInt16(bs->xxFATxx.FAT12_16_32.BS_BytesPerSec) > MAX_CLUSTER_SIZE) {
		myerror("Cluster size is larger than %u kB!", MAX_CLUSTER_SIZE / 1024);
		return -1;
	} else if (SwapInt16(bs->xxFATxx.FAT12_16_32.BS_RsvdSecCnt) == 0) {
		myerror("Reserved sector count is zero!");
		return -1;
	} else if (bs->xxFATxx.FAT12_16_32.BS_NumFATs == 0) {
		myerror("Number of FATs is zero!");
		return -1;
	}

	return 0;
}

int32_t read_bootsector(DEVICE *device, struct sBootSector *bs) {
/*
	reads bootsector
*/

	int64_t ret;

	assert(device != NULL);
	assert(bs != NULL);

	// seek to beginning of fs
	if (device_seekset(device, 0) == -1) {
		stderror();
		return -1;
	}

  ret=device_read(device, bs, 1, sizeof(struct sBootSector));
	if (ret == -1) {
		myerror("Failed to read from device!");
		return -1;
	} else if (ret < (int64_t) sizeof(struct sBootSector)) {
		myerror("Boot sector is too short!");
		return -1;
	}

	if (check_bootsector(bs)) {
		myerror("This is not a FAT boot sector or sector is damaged!");
		return -1;
	}

	return 0;
}

int32_t checkVbrCecksum(struct sFileSystem *fs) {
/*
 * 	Check VBR checksum
 */
	assert(fs != NULL);

	uint16_t i, j;
	uint32_t checksum=0;
	uint8_t sector[fs->sectorSize];
	uint32_t *checksums;

	// calculate checksum over first 11 sectors and compare it to the values in sector 12
	for (j=0; j<12; j++) {

		// read sector j
		if (device_seekset(fs->device, j * fs->sectorSize) == -1) {
			stderror();
			return -1;
		}

		if (device_read(fs->device, sector, fs->sectorSize, 1) < 1) {
			stderror();
			return -1;
		}
	
		if (j == 11) { // this sector contains the checksum
			checksums = (uint32_t*) sector;
			for (i=0; i<fs->sectorSize / 4; i++) {
				if (SwapInt32(checksums[i]) != checksum) {
					myerror("Failed to verify VBR checksum (calculated=%08X, found=%08X)!", checksum, SwapInt32(checksums[i]));
					return -1;
				}
			}
			return 0;
		}

		for (i=0; i<fs->sectorSize; i++) {
			if ((j!=0) || ((i != 106) && (i != 107) && (i != 112))) {
				checksum = ((checksum << 31) | (checksum >> 1)) + (uint32_t) sector[i];
			}
		}
	}

	return 0;
}

int32_t writeBootSector(struct sFileSystem *fs) {
/*
	write boot sector
*/

	// seek to beginning of fs
	if (device_seekset(fs->device, 0) == -1) {
		stderror();
		return -1;
	}

	// write boot sector
	if (device_write(fs->device, &(fs->bs), sizeof(struct sBootSector), 1) < 1) {
		stderror();
		return -1;
	}

	//  update backup boot sector for FAT32 file systems
	if (fs->FATType == FATTYPE_FAT32) {
		// seek to beginning of backup boot sector
		if (device_seekset(fs->device, SwapInt16(fs->bs.xxFATxx.FAT12_16_32.FATxx.FAT32.BS_BkBootSec) * fs->sectorSize) == -1) {
			stderror();
			return -1;
		}

		// write backup boot sector
		if (device_write(fs->device, &(fs->bs), sizeof(struct sBootSector), 1) < 1) {
			stderror();
			return -1;
		}
	}

	return 0;
}

int32_t readFSInfo(struct sFileSystem *fs, struct sFSInfo *fsInfo) {
/*
	reads FSInfo structure
*/

	assert(fs != NULL);
	assert(fsInfo != NULL);

	// seek to beginning of FSInfo structure

	if (device_seekset(fs->device, SwapInt16(fs->bs.xxFATxx.FAT12_16_32.FATxx.FAT32.BS_FSInfo) * fs->sectorSize) == -1) {

		stderror();
		return -1;
	}

	if (device_read(fs->device, fsInfo, sizeof(struct sFSInfo), 1) < 1) {
		stderror();
		return -1;
	}

	return 0;
}

int32_t writeFSInfo(struct sFileSystem *fs, struct sFSInfo *fsInfo) {
/*
	write FSInfo structure
*/
	assert(fs != NULL);
	assert(fsInfo != NULL);

	// seek to beginning of FSInfo structure

	if (device_seekset(fs->device, SwapInt16(fs->bs.xxFATxx.FAT12_16_32.FATxx.FAT32.BS_FSInfo) * fs->sectorSize) == -1) {

		stderror();
		return -1;
	}

	// write fsInfo
	if (device_write(fs->device, fsInfo, sizeof(struct sFSInfo), 1) < 1) {
		stderror();
		return -1;
	}

	return 0;
}

int32_t getCountOfClusters(struct sBootSector *bs, uint32_t *clusters) {
/*
	calculates count of clusters
*/

	assert(bs != NULL);
	assert(clusters != NULL);

	uint32_t RootDirSectors, FATSz, TotSec, DataSec;
	int32_t retvalue;


	RootDirSectors = ((SwapInt16(bs->xxFATxx.FAT12_16_32.BS_RootEntCnt) * DIR_ENTRY_SIZE) + (SwapInt16(bs->xxFATxx.FAT12_16_32.BS_BytesPerSec) - 1));
	RootDirSectors = RootDirSectors / SwapInt16(bs->xxFATxx.FAT12_16_32.BS_BytesPerSec);

	if (bs->xxFATxx.FAT12_16_32.BS_FATSz16 != 0) {
		FATSz = SwapInt16(bs->xxFATxx.FAT12_16_32.BS_FATSz16);
	} else {
		FATSz = SwapInt32(bs->xxFATxx.FAT12_16_32.FATxx.FAT32.BS_FATSz32);
	}
	if (SwapInt16(bs->xxFATxx.FAT12_16_32.BS_TotSec16) != 0) {
		TotSec = SwapInt16(bs->xxFATxx.FAT12_16_32.BS_TotSec16);
	} else {
		TotSec = SwapInt32(bs->xxFATxx.FAT12_16_32.BS_TotSec32);
	}
	DataSec = TotSec - (SwapInt16(bs->xxFATxx.FAT12_16_32.BS_RsvdSecCnt) + (bs->xxFATxx.FAT12_16_32.BS_NumFATs * FATSz) + RootDirSectors);

	retvalue = DataSec / bs->xxFATxx.FAT12_16_32.BS_SecPerClus;
	if (retvalue <= 0) {
		myerror("Failed to calculate count of clusters!");
		return 0;
	}
	*clusters=retvalue;
	return 1;
}

int32_t getFATType(struct sBootSector *bs) {
/*
	retrieves FAT type from bootsector
*/

	assert(bs != NULL);

	uint32_t CountOfClusters;

	if (!getCountOfClusters(bs, &CountOfClusters)) {
		myerror("Failed to get count of clusters!");
		return -1;
	} else if (CountOfClusters < 4096) { // FAT12!
		return FATTYPE_FAT12;
	} else if (CountOfClusters < 65525) { // FAT16!
		return FATTYPE_FAT16;
	} else { // FAT32!
		return FATTYPE_FAT32;
	}
}


uint16_t isFreeCluster(const uint32_t data) {
/*
	checks whether data marks a free cluster
*/

	    return (data & 0x0FFFFFFF) == 0;
}


uint16_t isEOC(struct sFileSystem *fs, const uint32_t data) {
/*
	checks whether data marks the end of a cluster chain
*/

	assert(fs != NULL);

	if(fs->FATType == FATTYPE_FAT12) {
	    if(data >= 0x0FF8)
		return 1;
	} else if(fs->FATType == FATTYPE_FAT16) {
	    if(data >= 0xFFF8)
		return 1;
	} else if (fs->FATType == FATTYPE_FAT32) {
	    if((data & 0x0FFFFFFF) >= 0x0FFFFFF8)
		return 1;
	}

	return 0;
}

uint16_t isBadCluster(struct sFileSystem *fs, const uint32_t data) {
/*
	checks whether data marks a bad cluster
*/
	assert(fs != NULL);

	if(fs->FATType == FATTYPE_FAT12) {
	    if(data == 0xFF7)
		return 1;
	} else if(fs->FATType == FATTYPE_FAT16) {
	    if(data == 0xFFF7)
		return 1;
	} else if (fs->FATType == FATTYPE_FAT32) {
	    if ((data & 0x0FFFFFFF) == 0x0FFFFFF7)
		return 1;
	}

	return 0;
}


void *readFAT(struct sFileSystem *fs, uint16_t nr) {
/*
	reads a FAT from file system fs
*/

	assert(fs != NULL);
	assert(nr < fs->bs.xxFATxx.FAT12_16_32.BS_NumFATs);

	uint32_t FATSizeInBytes;
	off_t BSOffset;

	void *FAT;

	FATSizeInBytes = fs->FATSize * fs->sectorSize;

	if ((FAT=malloc(FATSizeInBytes))==NULL) {
		stderror();
		return NULL;
	}
	BSOffset = (off_t)SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RsvdSecCnt) * SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_BytesPerSec);
	if (device_seekset(fs->device, BSOffset + nr * FATSizeInBytes) == -1) {
		myerror("Seek error!");
		free(FAT);
		return NULL;
	}
	if (device_read(fs->device, FAT, FATSizeInBytes, 1) < 1) {
		myerror("Failed to read from file!");
		free(FAT);
		return NULL;
	}

	return FAT;

}

int32_t writeFAT(struct sFileSystem *fs, void *fat) {
/*
	write FAT to file system
*/

	assert(fs != NULL);
	assert(fat != NULL);

	uint32_t FATSizeInBytes, nr;
	off_t BSOffset;

	FATSizeInBytes = fs->FATSize * fs->sectorSize;

	BSOffset = (off_t)SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RsvdSecCnt) * SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_BytesPerSec);

	// write all FATs!
	for(nr=0; nr< fs->bs.xxFATxx.FAT12_16_32.BS_NumFATs; nr++) {
		if (device_seekset(fs->device, BSOffset + nr * FATSizeInBytes) == -1) {
			myerror("Seek error!");
			return -1;
		}
		if (device_write(fs->device, fat, FATSizeInBytes, 1) < 1) {
			myerror("Failed to read from file!");
			return -1;
		}
	}

	return 0;
}

int32_t checkFATs(struct sFileSystem *fs) {
/*
	checks whether all FATs have the same content
*/

	assert(fs != NULL);

	uint32_t FATSizeInBytes;
	int32_t result=0;
	int32_t i;

	off_t BSOffset;

	char *FAT1, *FATx;

	// if there is just one FAT, we don't have to check anything
	if (fs->FATCount < 2) {
		return 0;
	}

	FATSizeInBytes = fs->FATSize * fs->sectorSize;

	if ((FAT1=malloc(FATSizeInBytes))==NULL) {
		stderror();
		return -1;
	}
	if ((FATx=malloc(FATSizeInBytes))==NULL) {
		stderror();
		free(FAT1);
		return -1;
	}
	BSOffset = (off_t)SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RsvdSecCnt) * SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_BytesPerSec);
	if (device_seekset(fs->device, BSOffset) == -1) {
		myerror("Seek error!");
		free(FAT1);
		free(FATx);
		return -1;
	}
	if (device_read(fs->device, FAT1, FATSizeInBytes, 1) < 1) {
		myerror("Failed to read from file!");
		free(FAT1);
		free(FATx);
		return -1;
	}

	for(i=1; i < fs->FATCount; i++) {
		if (device_seekset(fs->device, BSOffset+FATSizeInBytes) == -1) {
			myerror("Seek error!");
			free(FAT1);
			free(FATx);
			return -1;
		}
		if (device_read(fs->device, FATx, FATSizeInBytes, 1) < 1) {
			myerror("Failed to read from file!");
			free(FAT1);
			free(FATx);
			return -1;
		}

		//printf("FAT1: %08lx FATx: %08lx\n", FAT1[0], FATx[0]);

		result = memcmp(FAT1, FATx, FATSizeInBytes) != 0;
		if (result) break; // FATs don't match

	}

	free(FAT1);
	free(FATx);

	return result;
}

int32_t getFATEntry(struct sFileSystem *fs, uint32_t cluster, uint32_t *data) {
/*
	retrieves FAT entry for a cluster number
*/

	assert(fs != NULL);
	assert(data != NULL);

	off_t FATOffset, BSOffset;

	*data=0;

	switch(fs->FATType) {
	case FATTYPE_FAT32:
		FATOffset = (off_t)cluster * 4;
		BSOffset = (off_t)SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RsvdSecCnt) * SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_BytesPerSec) + FATOffset;
		if (device_seekset(fs->device, BSOffset) == -1) {
			myerror("Seek error!");
			return -1;
		}
		if (device_read(fs->device, data, 4, 1) < 1) {
			myerror("Failed to read from file!");
			return -1;
		}
		*data=SwapInt32(*data);
		*data = *data & 0x0fffffff;
		break;
	case FATTYPE_FAT16:
		FATOffset = (off_t)cluster * 2;
		BSOffset = (off_t) SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RsvdSecCnt) * SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_BytesPerSec) + FATOffset;
		if (device_seekset(fs->device, BSOffset) == -1) {
			myerror("Seek error!");
			return -1;
		}
		if (device_read(fs->device, data, 2, 1)<1) {
			myerror("Failed to read from file!");
			return -1;
		}
		*data=SwapInt32(*data);
		break;
	case FATTYPE_FAT12:
		FATOffset = (off_t) cluster + (cluster / 2);
		BSOffset = (off_t) SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RsvdSecCnt) * SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_BytesPerSec) + FATOffset;
		if (device_seekset(fs->device, BSOffset) == -1) {
			myerror("Seek error!");
			return -1;
		}
		if (device_read(fs->device, data, 2, 1)<1) {
			myerror("Failed to read from file!");
			return -1;
		}

		*data=SwapInt32(*data);

		if (cluster & 1)  {
			*data = *data >> 4;	/* cluster number is odd */
		} else {
			*data = *data & 0x0FFF;	/* cluster number is even */
		}
		break;
	case FATTYPE_EXFAT:
		FATOffset = (off_t)cluster * 4;
		BSOffset = (off_t)SwapInt32(fs->bs.xxFATxx.exFAT.fat_sector_start) * fs->sectorSize + FATOffset;
		if (device_seekset(fs->device, BSOffset) == -1) {
			myerror("Seek error!");
			return -1;
		}
		if (device_read(fs->device, data, 4, 1) < 1) {
			myerror("Failed to read from file!");
			return -1;
		}
		*data=SwapInt32(*data);
		break;
	default:
		myerror("Failed to get FAT type!");
		return -1;
	}

	return 0;

}

off_t getClusterOffset(struct sFileSystem *fs, uint32_t cluster) {
/*
	returns the offset of a specific cluster in the
	data region of the file system
*/

	assert(fs != NULL);
	assert(cluster > 1);

	return (off_t)(cluster - 2) * fs->clusterSize + fs->firstDataSector * fs->sectorSize;
}

void *readCluster(struct sFileSystem *fs, uint32_t cluster) {
/*
	read cluster from file system
*/
	void *dummy;

	if (device_seekset(fs->device, getClusterOffset(fs, cluster)) != 0) {
		stderror();
		return NULL;
	}

	if ((dummy = malloc(fs->clusterSize)) == NULL) {
		stderror();
		return NULL;
	}

	if ((device_read(fs->device, dummy, fs->clusterSize, 1)<1)) {
		myerror("Failed to read cluster!");
		return NULL;
	}

	return dummy;
}

int32_t writeCluster(struct sFileSystem *fs, uint32_t cluster, void *data) {
/*
	write cluster to file systen
*/
	if (device_seekset(fs->device, getClusterOffset(fs, cluster)) != 0) {
		stderror();
		return -1;
	}

	if (device_write(fs->device, data, fs->clusterSize, 1)<1) {
		stderror();
		return -1;
	}

	return 0;
}

int32_t parseEntry(struct sFileSystem *fs, union sDirEntry *de) {
/*
	parses one directory entry
*/

	assert(fs != NULL);
	assert(de != NULL);

	if ((device_read(fs->device, de, DIR_ENTRY_SIZE, 1)<1)) {
		myerror("Failed to read from file!");
		return -1;
	}

	if (de->ShortDirEntry.DIR_Name[0] == DE_FOLLOWING_FREE ) return 0; // no more entries

	// long dir entry
	if ((de->LongDirEntry.LDIR_Attr & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME) return 2;

	return 1; // short dir entry
}


int32_t parseExFATEntry(struct sFileSystem *fs, struct sExFATDirEntry *de) {
/*
	parses one exFAT directory entry
*/

	assert(fs != NULL);
	assert(de != NULL);

	if ((device_read(fs->device, de, DIR_ENTRY_SIZE, 1)<1)) {
		myerror("Failed to read from file!");
		return -1;
	}

	return de->type;

}


uint8_t calculateChecksum (char *sname) {
	uint8_t len;
	uint8_t sum;


	sum = 0;
	for (len=11; len!=0; len--) {
		sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *sname++;
	}
	return sum;
}

int32_t getAllocationTableOffset(struct sFileSystem *fs) {

	struct sClusterChain *chain, *tmp;
	struct sExFATDirEntry de;
	uint32_t i;

	if ((chain=newClusterChain()) == NULL) {
		myerror("Failed to create new cluster chain!");
		return -1;
	}

	if (getClusterChain(fs, SwapInt32(fs->bs.xxFATxx.exFAT.rootdir_cluster), chain) == -1) {
		myerror("Failed to get cluster chain!");
		return -1;
	}

	tmp=chain->next;
	while (tmp != NULL) {
		device_seekset(fs->device, getClusterOffset(fs, tmp->cluster));

		for(i=0; i < fs->clusterSize / DIR_ENTRY_SIZE; i++){
			if ((device_read(fs->device, &de, DIR_ENTRY_SIZE, 1)<1)) {
				myerror("Failed to read from file!");
				freeClusterChain(chain);
				return -1;
			}

			if (EXFAT_ISTYPE(de, EXFAT_ENTRY_ALLOC_BITMAP) &&
				EXFAT_HASFLAG(de, EXFAT_FLAG_INUSE)) {
				fs->allocBitmapFirstCluster=SwapInt32(de.entry.AllocationBitmapDirEntry.firstCluster);
				fs->allocBitmapSize=SwapInt64(de.entry.AllocationBitmapDirEntry.dataLen);
				freeClusterChain(chain);
				return 0; // success

			} else if (de.type == DE_FOLLOWING_FREE) {
				freeClusterChain(chain);
				myerror("Failed to find Allocation Bitmap Entry!");
				return -1;
			}
		}

		tmp=tmp->next;
	}

	freeClusterChain(chain);

	// no allocation table entry found
	myerror("Failed to find Allocation Bitmap Entry!");

	return -1;
}

int32_t isClusterAllocated(struct sFileSystem *fs, uint32_t cluster) {
/*
	lookup in Allocation Bitmap Table whether cluster is allocated
*/

	off_t offset;
	uint8_t byte;

	assert(cluster > 2);
	cluster-=2;
	assert(cluster < fs->clusters);

	offset=getClusterOffset(fs, fs->allocBitmapFirstCluster) + (cluster) / 8;

	device_seekset(fs->device, offset);

	if ((device_read(fs->device, &byte, 1, 1)<1)) {
		myerror("Failed to read from file!");
		return -1;
	}

	return byte & (1 << (cluster % 8));

}

int32_t countAllocatedClusters(struct sFileSystem *fs) {
/*
 * count allocated clusters and update fs
 */
	off_t offset;
	uint32_t data[fs->clusterSize / 4];
	uint64_t i;
	uint32_t count=0, v;
	struct sClusterChain *chain, *tmp;


	if ((chain=newClusterChain()) == NULL) {
		myerror("Failed to create new cluster chain!");
		return -1;
	}

	if (getClusterChain(fs, fs->allocBitmapFirstCluster, chain) == -1) {
		myerror("Failed to get cluster chain!");
		return -1;
	}

	tmp=chain->next;
	while (tmp != NULL) {

		offset=getClusterOffset(fs, tmp->cluster);

		device_seekset(fs->device, offset);

		if ((device_read(fs->device, &data, fs->clusterSize, 1)<1)) {
			myerror("Failed to read from file!");
			return -1;
		}

		// count bits double word wise
		for (i=0; i<fs->clusterSize / 4; i++) {
			v=SwapInt32(data[i]);
			for(;v;count++) {
				v&=v-1;
			}
		}

		tmp=tmp->next;
	}

	freeClusterChain(chain);

	fs->allocatedClusters=count;
	return 0;

}


int32_t getClusterChain(struct sFileSystem *fs, uint32_t startCluster, struct sClusterChain *chain) {
/*
	retrieves an array of all clusters in a cluster chain
	starting with startCluster
*/

	assert(fs != NULL);
	assert(chain != NULL);

	uint32_t cluster;
	uint32_t data,i=0;

	cluster=startCluster;

	switch(fs->FATType) {
	case FATTYPE_FAT12:
		do {
			if (i == fs->maxClusterChainLength) {
				myerror("Cluster chain is too long!");
				return -1;
			}
			if (cluster >= fs->clusters+2) {
				myerror("Cluster %08x does not exist!", data);
				return -1;
			}
			if (insertCluster(chain, cluster) == -1) {
				myerror("Failed to insert cluster!");
				return -1;
			}
			i++;
			if (getFATEntry(fs, cluster, &data)) {
				myerror("Failed to get FAT entry!");
				return -1;
			}
			if (data == 0) {
				myerror("Cluster %08x is marked as unused!", cluster);
				return -1;
			}
			cluster=data;
		} while (cluster < 0x0ff8);	// end of cluster
		break;
	case FATTYPE_FAT16:
		do {
			if (i == fs->maxClusterChainLength) {
				myerror("Cluster chain is too long!");
				return -1;
			}
			if (cluster >= fs->clusters+2) {
				myerror("Cluster %08x does not exist!", data);
				return -1;
			}
			if (insertCluster(chain, cluster) == -1) {
				myerror("Failed to insert cluster!");
				return -1;
			}
			i++;
			if (getFATEntry(fs, cluster, &data)) {
				myerror("Failed to get FAT entry!");
				return -1;
			}
			if (data == 0) {
				myerror("Cluster %08x is marked as unused!", cluster);
				return -1;
			}
			cluster=data;
		} while (cluster < 0xfff8);	// end of cluster chain
		break;
	case FATTYPE_FAT32:
		do {
			if (i == fs->maxClusterChainLength) {
				myerror("Cluster chain is too long!");
				return -1;
			}
			if ((cluster & 0x0fffffff) >= fs->clusters+2) {
				myerror("Cluster %08x does not exist!", data);
				return -1;
			}
			if (insertCluster(chain, cluster) == -1) {
				myerror("Failed to insert cluster!");
				return -1;
			}
			i++;
			if (getFATEntry(fs, cluster, &data)) {
				myerror("Failed to get FAT entry");
				return -1;
			}
			if ((data & 0x0fffffff) == 0) {
				myerror("Cluster %08x is marked as unused!", cluster);
				return -1;
			}
			cluster=data;
		} while (((cluster & 0x0fffffff) != 0x0ff8fff8) &&
			 ((cluster & 0x0fffffff) < 0x0ffffff8));	// end of cluster chain
		break;
	case FATTYPE_EXFAT:
		do {
			if (i == fs->maxClusterChainLength) {
				myerror("Cluster chain is too long!");
				return -1;
			}
			if (cluster >= fs->clusters+2) {
				myerror("Cluster %08x does not exist!", data);
				return -1;
			}
			if (insertCluster(chain, cluster) == -1) {
				myerror("Failed to insert cluster!");
				return -1;
			}
			i++;
			if (getFATEntry(fs, cluster, &data)) {
				myerror("Failed to get FAT entry");
				return -1;
			}
			if (data == 0) {
				myerror("Cluster %08x is marked as unused!", cluster);
				return -1;
			}
			cluster=data;
		} while (cluster < 0xfffffff8);	// end of cluster chain
		break;
	case -1:
	default:
		myerror("Failed to get FAT type!");
		return -1;
	}

	return i;
}


int32_t openFileSystem(char *path, uint32_t mode, struct sFileSystem *fs) {
/*
	opens file system and assemlbes file system information into data structure
*/
	assert(path != NULL);
	assert(fs != NULL);

	int32_t ret;

	switch(mode) {
		case FS_MODE_RO:


		case FS_MODE_RW:
			break;
		case FS_MODE_RO_EXCL:
		case FS_MODE_RW_EXCL:
			// this check is only done for user convenience
			// open would fail too if device is mounted, but without specific error message
			ret=check_mounted(path);
			switch (ret) {
				case 0: break;  // filesystem not mounted
				case 1:		// filesystem mounted
					myerror("Filesystem is mounted. Please unmount!");
					return -1;
				case -1:	// unable to check
				default:
					myerror("Could not check whether filesystem is mounted!");
					return -1;
			}

			break;
		default:
			myerror("Mode not supported!");
			return -1;
	}

	if ((fs->device=device_open(path)) == NULL) {
		stderror();
		return -1;
	}

	// read boot sector
	if (read_bootsector(fs->device, &(fs->bs))) {
		myerror("Failed to read boot sector!");
		device_close(fs->device);
		return -1;
	}

	strncpy(fs->path, path, MAX_PATH_LEN);
	fs->path[MAX_PATH_LEN] = '\0';

	if (!memcmp(fs->bs.BS_OEMName, "EXFAT   ", 8)) { // exFAT!
		// calculations are far easier for exFAT than for FATxx
		fs->FATType = FATTYPE_EXFAT;
		fs->sectorSize = (1 << fs->bs.xxFATxx.exFAT.sector_bits);
		fs->FATSize = SwapInt32(fs->bs.xxFATxx.exFAT.fat_sector_count);
		fs->clusters = SwapInt32(fs->bs.xxFATxx.exFAT.cluster_count);
		fs->clusterSize = (1 << fs->bs.xxFATxx.exFAT.spc_bits) * fs->sectorSize;
		fs->FSSize = SwapInt64(fs->bs.xxFATxx.exFAT.sector_count) * fs->sectorSize;
		fs->FATCount = fs->bs.xxFATxx.exFAT.fat_count;
		fs->maxClusterChainLength = 0xffffffff; // basically not limited
		fs->firstDataSector= SwapInt32(fs->bs.xxFATxx.exFAT.cluster_sector_start);

		if (checkVbrCecksum(fs)) {
			myerror("Volume Boot Record checksum verification failed!");
			return -1;
		}

		if (getAllocationTableOffset(fs)) {
			myerror("Failed to get Allocation Table Offset!");
			return -1;
		}
		if (countAllocatedClusters(fs)) {
			myerror("Failed to count allocated clusters!");
			return -1;
		}
		if ((fs->allocBitmapFirstCluster < 2) || (fs->allocBitmapFirstCluster > fs->clusters+1)) {
			myerror("First cluster of Allocation Bitmap Table is invalid (%u)!", fs->allocBitmapFirstCluster);
			return -1;
		}

		if (EXFAT_HASVOLUMEFLAG(fs->bs.xxFATxx.exFAT.volumeFlags, EXFAT_VOLUME_FLAG_VOLUME_DIRTY)) {
			myerror("Volume is marked as dirty. Please run fschk!");
			return -1;
		}

	} else { // we are expecting FATxx now

		fs->FATCount = fs->bs.xxFATxx.FAT12_16_32.BS_NumFATs;

		if (SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_TotSec16) != 0) {
			fs->totalSectors = SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_TotSec16);
		} else {
			fs->totalSectors = SwapInt32(fs->bs.xxFATxx.FAT12_16_32.BS_TotSec32);
		}

		if (fs->totalSectors == 0) {
			myerror("Count of total sectors must not be zero!");
			device_close(fs->device);
			return -1;
		}


		fs->FATType = getFATType(&(fs->bs));
		if (fs->FATType == -1) {
			myerror("Failed to get FAT type!");
			device_close(fs->device);
			return -1;
		}


		if ((fs->FATType == FATTYPE_FAT32) && (fs->bs.xxFATxx.FAT12_16_32.FATxx.FAT32.BS_FATSz32 == 0)) {
			myerror("32-bit count of FAT sectors must not be zero for FAT32!");
			device_close(fs->device);
			return -1;
		} else 	if (((fs->FATType == FATTYPE_FAT12) || (fs->FATType == FATTYPE_FAT16)) && (fs->bs.xxFATxx.FAT12_16_32.BS_FATSz16 == 0)) {
			myerror("16-bit count of FAT sectors must not be zero for FAT1x!");
			device_close(fs->device);
			return -1;
		}

		if (fs->bs.xxFATxx.FAT12_16_32.BS_FATSz16 != 0) {
			fs->FATSize = SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_FATSz16);
		} else {
			fs->FATSize = SwapInt32(fs->bs.xxFATxx.FAT12_16_32.FATxx.FAT32.BS_FATSz32);
		}

		// check whether count of root dir entries is ok for given FAT type
		if (((fs->FATType == FATTYPE_FAT16) || (fs->FATType == FATTYPE_FAT12)) && (SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RootEntCnt) == 0)) {
			myerror("Count of root directory entries must not be zero for FAT1x!");
			device_close(fs->device);
			return -1;
		} else 	if ((fs->FATType == FATTYPE_FAT32) && (SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RootEntCnt) != 0)) {
			myerror("Count of root directory entries must be zero for FAT32 (%u)!", SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RootEntCnt));
			device_close(fs->device);
			return -1;
		}

		if (!getCountOfClusters(&(fs->bs), &fs->clusters)) {
			myerror("Failed to get count of clusters!");
			device_close(fs->device);
			return -1;
		}

		if (fs->clusters > 268435445) {
			myerror("Count of clusters should be less than 268435446, but is %d!", fs->clusters);
			device_close(fs->device);
			return -1;
		}

		fs->sectorSize=SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_BytesPerSec);

		fs->clusterSize=fs->bs.xxFATxx.FAT12_16_32.BS_SecPerClus * SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_BytesPerSec);

		fs->FSSize = (uint64_t) fs->clusters * fs->clusterSize;

		fs->maxClusterChainLength = (uint32_t) MAX_FILE_LEN / fs->clusterSize;

		uint32_t rootDirSectors;

		fs->maxClusterChainLength = (uint32_t) MAX_FILE_LEN / fs->clusterSize;

		rootDirSectors = ((SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RootEntCnt) * DIR_ENTRY_SIZE) +
				  (SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_BytesPerSec) - 1)) / SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_BytesPerSec);
		fs->firstDataSector = (SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RsvdSecCnt) +
				      (fs->FATCount * fs->FATSize) + rootDirSectors);
	}

	fs->maxDirEntriesPerCluster = fs->clusterSize / DIR_ENTRY_SIZE;


#ifndef __WIN32__
	// convert utf 16 le to local charset
	fs->cd = iconv_open("", "UTF-16LE");
	if (fs->cd == (iconv_t)-1) {
			myerror("iconv_open failed!");
	return -1;
	}
#endif	
		
	return 0;
}

int32_t syncFileSystem(struct sFileSystem *fs) {
/*
	sync file system
*/
	if (device_sync(fs->device) != 0) {
		myerror("Could not sync device!");
		return -1;
	}

	return 0;
}

int32_t closeFileSystem(struct sFileSystem *fs) {
/*
	closes file system
*/
	assert(fs != NULL);

	device_close(fs->device);
#ifndef __WIN32__
	iconv_close(fs->cd);
#endif

	return 0;
}
