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

#ifndef __FAT_fs_h__
#define __FAT_fs_h__

#ifdef __WIN32__
#define fsync(i) 0
#endif

// FS open mode bits
#define FS_MODE_RO 1
#define FS_MODE_RO_EXCL 2
#define FS_MODE_RW 3
#define FS_MODE_RW_EXCL 4

// FAT types
#define FATTYPE_FAT12 12
#define FATTYPE_FAT16 16
#define FATTYPE_FAT32 32
#define FATTYPE_EXFAT 64

// file attributes
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)
#define ATTR_LONG_NAME_MASK (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID | ATTR_DIRECTORY | ATTR_ARCHIVE)

// constants for the LDIR structure
#define DE_FREE 0xe5
#define DE_FOLLOWING_FREE 0x00
#define LAST_LONG_ENTRY 0x40

#define DIR_ENTRY_SIZE 32

// maximum path len on FAT file systems (above specification)
#define MAX_PATH_LEN 512

// maximum filename length on exFAT
#define MAX_EXFAT_FILENAME_LEN 255

// maximum file len
// (specification: file < 4GB which is
// maximum clusters in chain * cluster size)
#define MAX_FILE_LEN 0xFFFFFFFF
#define MAX_DIR_ENTRIES 65536
#define MAX_CLUSTER_SIZE 65536

#include <stdio.h>
#include <stdint.h>
#include <iconv.h>

#include "deviceio.h"
#include "endianness.h"
#include "clusterchain.h"

#ifdef __WIN32__
#define ATTR_PACKED __attribute__ ((gcc_struct, __packed__))
#else
#define ATTR_PACKED __attribute__((packed))
#endif

// Directory entry structures
// Structure for long directory names
struct sLongDirEntry {
	uint8_t LDIR_Ord;		// Order of entry in sequence
	char LDIR_Name1[10];		// Chars 1-5 of long name
	uint8_t LDIR_Attr;		// Attributes (ATTR_LONG_NAME must be set)
	uint8_t LDIR_Type;		// Type
	uint8_t LDIR_Checksum;		// Short name checksum
	char LDIR_Name2[12];		// Chars 6-11 of long name
	uint16_t LDIR_FstClusLO;	// Zero
	char LDIR_Name3[4];		// Chars 12-13 of long name
} ATTR_PACKED;

// Structure for old short directory names
struct sShortDirEntry {
	char DIR_Name[11];		// Short name
	uint8_t DIR_Atrr;		// File attributes
	uint8_t DIR_NTRes;		// Reserved for NT
	uint8_t DIR_CrtTimeTenth;	// Time of creation in ms
	uint16_t DIR_CrtTime;		// Time of creation
	uint16_t DIR_CrtDate;		// Date of creation
	uint16_t DIR_LstAccDate;	// Last access date
	uint16_t DIR_FstClusHI;	// Hiword of first cluster
	uint16_t DIR_WrtTime;		// Time of last write
	uint16_t DIR_WrtDate;		// Date of last write
	uint16_t DIR_FstClusLO;	// Loword of first cluster
	uint32_t DIR_FileSize;		// file size in bytes
} ATTR_PACKED;

union sDirEntry {
	struct sShortDirEntry ShortDirEntry;
	struct sLongDirEntry LongDirEntry;
} ATTR_PACKED;


// exFAT dir entry structures

// exFAT directory entry type flags
#define EXFAT_FLAG_INUSE 0x80
#define EXFAT_FLAG_SECONDARY 0x40
#define EXFAT_FLAG_BENIGN 0x20

// exFAT directory entry types
#define EXFAT_ENTRY_TYPE_MASK		0x7f
#define EXFAT_ENTRY_VOLUME_LABEL	0x03
#define EXFAT_ENTRY_ALLOC_BITMAP	0x01
#define EXFAT_ENTRY_UPCASE_TABLE	0x02
#define EXFAT_ENTRY_VOLUME_GUID		(0x00 | EXFAT_FLAG_SECONDARY | EXFAT_FLAG_BENIGN)
#define EXFAT_ENTRY_TEXFAT_PADDING	(0x01 | EXFAT_FLAG_SECONDARY | EXFAT_FLAG_BENIGN)
#define EXFAT_ENTRY_WINCE_AC_TABLE	(0x02 | EXFAT_FLAG_SECONDARY | EXFAT_FLAG_BENIGN)
#define EXFAT_ENTRY_FILE		0x05
#define EXFAT_ENTRY_STREAM_EXTENSION	(0x00 | EXFAT_FLAG_SECONDARY)
#define EXFAT_ENTRY_FILE_NAME_EXTENSION	(0x01 | EXFAT_FLAG_SECONDARY)
#define EXFAT_ENTRY_EMPTY		0x00
#define EXFAT_ISTYPE(_entry, _type) ((_entry.type & EXFAT_ENTRY_TYPE_MASK) == _type)
#define EXFAT_HASFLAG(_entry, _flag) (_entry.type & _flag)

// exFAT general secondary flags
#define EXFAT_GSFLAG_ALLOC_POSSIBLE	0x01
#define EXFAT_GSFLAG_FAT_INVALID	0x02

// exFAT file attributes
#define EXFAT_ATTR_RO		0x01
#define EXFAT_ATTR_HIDDEN	0x02
#define EXFAT_ATTR_SYS		0x04
#define EXFAT_ATTR_VOL		0x08
#define EXFAT_ATTR_DIR		0x10
#define EXFAT_ATTR_ARCH		0x20
#define EXFAT_HASATTR(_de, _attr) (SwapInt16(_de.attr) & _attr)

// exFAT FAT Allocation Bitmap flag
#define EXFAT_FLAG_BITMAP	0x01


struct sExFATAllocationBitmapDirEntry {
	uint8_t bitmapFlags;
	char reserved[18];
	uint32_t firstCluster;
	uint64_t dataLen;
} ATTR_PACKED;

struct sExFATFileDirEntry {
	uint8_t count;		// secondary count
	uint16_t chksum;	// set checksum
	uint16_t attr;		// file attributes
	uint16_t reserved1;
	uint32_t createTime;	// create timestamp
	uint32_t lastModTime;	// last modified timestamp
	uint32_t lastAccTime;	// last accessed timestamp
	uint8_t createTimeMs;	// create timestamp (ms)
	uint8_t lastModTimeMs;	// last modified timestamp (ms)
	uint8_t lastAccTimeMs;	// last accessed timestamp (ms)
	uint8_t createTimeTZ;	// create timestamp timezone difference
	uint8_t lastModTimeTZ;	// last modified timestamp timezone difference
	uint8_t lastAccTimeTZ;	// last accessed timestamp timezone difference
	uint8_t reserved2[7];
} ATTR_PACKED;

struct sExFATStreamExtDirEntry {
	uint8_t genSecFlags;	// general secondary flags
	uint8_t reserved1;
	uint8_t nameLen;		// name length
	uint16_t nameHash;	// name hash
	uint16_t reserved2;
	uint64_t validDataLen;	// valid data length
	uint32_t reserved3;
	uint32_t firstCluster;	// first cluster
	uint64_t dataLen;	// data length
} ATTR_PACKED;

struct sExFATFileNameExtDirEntry {
	uint8_t genSecFlags;	// general secondary flags
	char filename[30];	// file name (part)
} ATTR_PACKED;

struct sExFATDirEntry {
	uint8_t type;		// entry type
	union {
		uint8_t data[31];
		struct sExFATFileDirEntry fileDirEntry;
		struct sExFATStreamExtDirEntry streamExtDirEntry;
		struct sExFATFileNameExtDirEntry FileNameExtDirEntry;
		struct sExFATAllocationBitmapDirEntry AllocationBitmapDirEntry;
		// other dir entry structures are not relevant
	} entry;
} ATTR_PACKED;

// Bootsector structures
// FAT12 and FAT16
struct sFAT12_16 {
	uint8_t BS_DrvNum;		// Physical drive number
	uint8_t BS_Reserved;		// Current head
	uint8_t BS_BootSig;		// Signature
	uint32_t BS_VolID;		// Volume ID
	char BS_VolLab[11];		// Volume Label
	char BS_FilSysType[8];		// FAT file system type (e.g. FAT, FAT12, FAT16, FAT32)
	uint8_t unused[448];		// unused space in bootsector
} ATTR_PACKED;

// FAT32
struct sFAT32 {
	uint32_t BS_FATSz32;		// Sectors per FAT
	uint16_t BS_ExtFlags;		// Flags
	uint16_t BS_FSVer;		// Version
	uint32_t BS_RootClus;		// Root Directory Cluster
	uint16_t BS_FSInfo;		// Sector of FSInfo structure
	uint16_t BS_BkBootSec;		// Sector number of the boot sector copy in reserved sectors
	char BS_Reserved[12];		// for future expansion
	char BS_DrvNum;			// see fat12/16
	char BS_Reserved1;		// see fat12/16
	char BS_BootSig;		// ...
	uint32_t BS_VolID;
	char BS_VolLab[11];
	char BS_FilSysType[8];
	uint8_t unused[420];		// unused space in bootsector
} ATTR_PACKED;

// common fields for FAT12, FAT16 and FAT32
union sFATxx {
	struct sFAT12_16 FAT12_16;
	struct sFAT32 FAT32;
} ATTR_PACKED;

// common fields for FAT12, FAT16 and FAT32
struct sFAT12_16_32 {
	uint16_t BS_BytesPerSec;	// Bytes per sector
	uint8_t BS_SecPerClus;		// Sectors per cluster
	uint16_t BS_RsvdSecCnt;	// Reserved sector count (including boot sector)
	uint8_t BS_NumFATs;		// Number of file allocation tables
	uint16_t BS_RootEntCnt;	// Number of root directory entries
	uint16_t BS_TotSec16;		// Total sectors (bits 0-15)
	uint8_t BS_Media;		// Media descriptor
	uint16_t BS_FATSz16;		// Sectors per file allocation table
	uint16_t BS_SecPerTrk;		// Sectors per track
	uint16_t BS_NumHeads;		// Number of heads
	uint32_t BS_HiddSec;		// Hidden sectors
	uint32_t BS_TotSec32;		// Total sectors (bits 16-47)
	union sFATxx FATxx;
} ATTR_PACKED;

// exFAT volume flags
#define EXFAT_VOLUME_FLAG_ACTIVE_FAT		0x01
#define EXFAT_VOLUME_FLAG_VOLUME_DIRTY		0x02
#define EXFAT_VOLUME_FLAG_MEDIA_FAILURE	0x04
#define EXFAT_VOLUME_FLAG_CLEAR_TO_ZERO	0x08
#define EXFAT_HASVOLUMEFLAG(_volumeflags, _flag) (SwapInt16(_volumeflags) & _flag)

// exFAT
// structure partly taken from Andrew Nayenkos exFAT implementation
struct sexFAT {
	uint8_t  __unused1[53];                  /* 0x0B always 0 */
	uint64_t sector_start;                    /* 0x40 partition first sector */
	uint64_t sector_count;                    /* 0x48 partition sectors count */
	uint32_t fat_sector_start;                /* 0x50 FAT first sector */
	uint32_t fat_sector_count;                /* 0x54 FAT sectors count */
	uint32_t cluster_sector_start;    /* 0x58 first cluster sector */
	uint32_t cluster_count;                   /* 0x5C total clusters count */
	uint32_t rootdir_cluster;                 /* 0x60 first cluster of the root dir */
	uint32_t volume_serial;                   /* 0x64 volume serial number */
	struct                                    /* 0x68 FS version */
	{
		uint8_t  minor;
		uint8_t  major;
	}
	version;
	uint16_t volumeFlags;   /* 0x6A volume state flags */
	uint8_t sector_bits;                    /* 0x6C sector size as (1 << n) */
	uint8_t spc_bits;                               /* 0x6D sectors per cluster as (1 << n) */
	uint8_t fat_count;                              /* 0x6E always 1 */
	uint8_t drive_no;                               /* 0x6F always 0x80 */
	uint8_t allocated_percent;              /* 0x70 percentage of allocated space */
	uint8_t __unused2[397];                 /* 0x71 always 0 */
} ATTR_PACKED;

union sxxFATxx {
	struct sexFAT exFAT;
	struct sFAT12_16_32 FAT12_16_32;
} ATTR_PACKED;

// First sector = boot sector
// common fields for exFAT, FAT12, FAT16 and FAT32
struct sBootSector {
	uint8_t BS_JmpBoot[3];		// Jump instruction (to skip over header on boot)
	char BS_OEMName[8];		// OEM Name (padded with spaces)
	union sxxFATxx xxFATxx;		// exFAT or FATxx
	uint16_t BS_EndOfBS;		// marks end of bootsector
} ATTR_PACKED;

// FAT32 FSInfo structure
struct sFSInfo {
	uint32_t FSI_LeadSig;
	uint8_t FSI_Reserved1[480];
	uint32_t FSI_StrucSig;
	uint32_t FSI_Free_Count;
	uint32_t FSI_Nxt_Free;
	uint8_t FSI_Reserved2[12];
	uint32_t FSI_TrailSig;
} ATTR_PACKED;

// holds information about the file system
struct sFileSystem {
	DEVICE *device;
	uint32_t mode;
	char path[MAX_PATH_LEN+1];
	struct sBootSector bs;
	int32_t FATType;
	uint32_t clusters;
	uint16_t sectorSize;
	uint32_t totalSectors;
	uint32_t clusterSize;
	uint32_t FATSize;
	uint64_t FSSize;
	uint32_t maxDirEntriesPerCluster;
	uint32_t maxClusterChainLength;
	uint32_t firstDataSector;
	uint8_t  FATCount;
	uint32_t allocBitmapFirstCluster;
	uint64_t allocBitmapSize;
	uint32_t allocatedClusters;
	iconv_t cd;
};

// functions

// opens file system and calculates file system information
int32_t openFileSystem(char *path, uint32_t mode, struct sFileSystem *fs);

// update boot sector
int32_t writeBootSector(struct sFileSystem *fs);

// sync file system
int32_t syncFileSystem(struct sFileSystem *fs);

// closes file system
int32_t closeFileSystem(struct sFileSystem *fs);

// lazy check if this is really a FAT bootsector
int32_t check_bootsector(struct sBootSector *bs);

// retrieves FAT entry for a cluster number
int32_t getFATEntry(struct sFileSystem *fs, uint32_t cluster, uint32_t *data);

// read FAT from file system
void *readFAT(struct sFileSystem *fs, uint16_t nr);

// write FAT to file system
int32_t writeFAT(struct sFileSystem *fs, void *fat);

// read cluster from file systen
void *readCluster(struct sFileSystem *fs, uint32_t cluster);

// write cluster to file systen
int32_t writeCluster(struct sFileSystem *fs, uint32_t cluster, void *data);

// checks whether data marks a free cluster
uint16_t isFreeCluster(const uint32_t data);

// checks whether data marks the end of a cluster chain
uint16_t isEOC(struct sFileSystem *fs, const uint32_t data);

// checks whether data marks a bad cluster
uint16_t isBadCluster(struct sFileSystem *fs, const uint32_t data);

// returns the offset of a specific cluster in the data region of the file system
off_t getClusterOffset(struct sFileSystem *fs, uint32_t cluster);

// parses one directory entry
int32_t parseEntry(struct sFileSystem *fs, union sDirEntry *de);

// parses one exFAT directory entry
int32_t parseExFATEntry(struct sFileSystem *fs, struct sExFATDirEntry *de);

// calculate checksum for short dir entry name
uint8_t calculateChecksum (char *sname);

// checks whether all FATs have the same content
int32_t checkFATs(struct sFileSystem *fs);

// reads FSInfo structure
int32_t readFSInfo(struct sFileSystem *fs, struct sFSInfo *fsInfo);

// write FSInfo structure
int32_t writeFSInfo(struct sFileSystem *fs, struct sFSInfo *fsInfo);

// get cluster chain
int32_t getClusterChain(struct sFileSystem *fs, uint32_t startCluster, struct sClusterChain *chain);

// return if cluster is allocated, -1 on error
int32_t isClusterAllocated(struct sFileSystem *fs, uint32_t cluster);

// return number of allocated clusters according to allocation bitmap
int32_t countAllocatedClusters(struct sFileSystem *fs);

#endif // __FAT_fs_h__
