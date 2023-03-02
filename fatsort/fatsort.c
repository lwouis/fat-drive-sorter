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
	This file contains the main function of fatsort.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <locale.h>
#include <time.h>
#include <inttypes.h>

// project includes
#include "endianness.h"
#include "sig.h"
#include "FAT_fs.h"
#include "options.h"
#include "errors.h"
#include "sort.h"
#include "clusterchain.h"
#include "misc.h"
#include "mallocv.h"

// program information
#define INFO_PROGRAM		"fatsort"
#define INFO_VERSION		"1.6.4"
#define INFO_AUTHOR		"Written by Boris Leidner.\n"
#define INFO_COPYRIGHT		"Copyright (C) 2004-2021 Boris Leidner.\n"
#define INFO_LICENSE		"License GPLv2: GNU GPL version 2 (see LICENSE.txt)\n" \
				"This is free software: you are free to change and redistribute it.\n" \
				"There is NO WARRANTY, to the extent permitted by law.\n"
#define INFO_DESCRIPTION	"FATSort sorts directory structures of FAT file systems (FAT12, FAT16, FAT32 and exFAT). " \
				"Many MP3 hardware players don't sort files automatically " \
				"but play them in the  order they were transferred to the " \
				"device. FATSort can help here.\n"

#define INFO_USAGE		"Usage: fatsort [OPTIONS] DEVICE\n" \
				"\n" \
				"Sort options:\n\n" \
				"\t-a\tUse ASCIIbetical order for sorting\n\n" \
				"\t-c\tIgnore case of file names (default for most locales)\n\n" \
				"\t-I PFX\tIgnore file name PFX\n\n" \
				"\t-n\tNatural order sorting\n\n" \
				"\t-o FLAG\tSort order of files where FLAG is one of\n\n" \
				"\t\t\td : directories first (default)\n\n" \
				"\t\t\tf : files first\n\n" \
				"\t\t\ta : files and directories are not differentiated\n\n" \
				"\t-r\tSort in reverse order\n\n" \
				"\t-R\tSort in random order\n\n" \
				"\t\tThis options makes all other sort options obsolete.\n\n" \
				"\t-t\tSort by last modification date and time\n\n" \
				"The following options can be specified multiple times\n" \
				"to select which directories shall be sorted:\n\n" \
				"\t-d DIR\tSort directory DIR only\n\n" \
				"\t-D DIR\tSort directory DIR and all subdirectories\n\n" \
				"\t-x DIR\tDon't sort directory DIR\n\n" \
				"\t-X DIR\tDon't sort directory DIR and its subdirectories\n\n" \
				"The following options can be specified multiple times\n" \
				"to select which directories shall be sorted using\n" \
				"POSIX.2 extended regular expressions:\n\n" \
				"\t-e RE\tOnly sort directories that match regular expression RE\n\n" \
				"\t-E RE\tDon't sort directories that match regular expression RE\n\n" \
				"However, options -e and -E may not be used simultaneously with\n" \
				"options -d, -D, -x and -X.\n\n" \
				"\t-L LOC\tUse the locale LOC instead of the locale from the environment variables\n\n" \
				"More options:\n\n" \
				"\t-l\tPrint current order of files only\n\n" \
				"\t-i\tPrint file system information only\n\n" \
				"\t-f\tForce sorting even if file system is mounted\n\n" \
				"\t-h, --help\n\n" \
				"\t\tPrint some help\n\n" \
				"\t-v, --version\n\n" \
				"\t\tPrint version information\n\n" \
				"\t-q\tBe quiet\n\n" \
				"DEVICE must be a FAT12, FAT16, FAT32 or exFAT file system.\n\n" \
				"WARNING: THE FILESYSTEM MUST BE CONSISTENT (NO FILESYSTEM ERRORS).\n" \
				"PLEASE BACKUP YOUR DATA BEFORE USING FATSORT. RISK OF CORRUPT FILESYSTEM!\n" \
				"FATSORT USER ASSUMES ALL RISK. FATSORT WILL NOT BE HELD LIABLE FOR DATA LOSS!\n" \
								"\n" \
				"Examples:\n" \
				"\tfatsort /dev/sda\t\tSort /dev/sda.\n" \
				"\tfatsort -n /dev/sdb1\t\tSort /dev/sdb1 with natural order.\n" \
				"\n" \
				"Report bugs to <fatsort@formenos.de>.\n"

#define INFO_OPTION_VERSION	INFO_PROGRAM " " INFO_VERSION "\n" \
				"\n" \
				INFO_COPYRIGHT \
				INFO_LICENSE \
				"\n" \
				INFO_AUTHOR

#define INFO_OPTION_HELP	INFO_DESCRIPTION \
				"\n" \
				INFO_USAGE


int32_t printFSInfo(char *filename) {
/*
	print file system information
*/

	assert(filename != NULL);

	uint32_t value, clen;
	uint32_t usedClusters, badClusters;
	uint32_t i;
	struct sClusterChain *chain;

	struct sFileSystem fs;

	printf("\t- File system information -\n");

	if (openFileSystem(filename, FS_MODE_RO, &fs)) {
		myerror("Failed to open file system!");
		return -1;
	}

	if (fs.FATType != 64) { // VFAT
		usedClusters=0;
		badClusters=0;
		for (i=2; i<fs.clusters+2; i++) {
			getFATEntry(&fs, i, &value);
			if ((value & 0x0FFFFFFF) != 0) usedClusters++;
			if (fs.FATType == FATTYPE_FAT32) {
				if ((value & 0x0FFFFFFF) == 0x0FFFFFF7) badClusters++;
			} else if (fs.FATType == FATTYPE_FAT16) {
				if (value == 0x0000FFF7) badClusters++;
			} else if (fs.FATType == FATTYPE_FAT12) {
				if (value == 0x00000FF7) badClusters++;
			} else {
				myerror("Unsupported FAT type!");
				return -1;
			}
		}
	} else { // exFAT
		usedClusters=fs.allocatedClusters;
		badClusters=0;
		for (i=2; i<fs.clusters+2; i++) {
			getFATEntry(&fs, i, &value);
			if (value == 0xFFFFFFF7) badClusters++;
		}
	}

	// feature: print volume label
	printf("Device:\t\t\t\t\t%s\n", fs.path);
	printf("Type:\t\t\t\t\t");
	if (fs.FATType == 12) {
		printf("FAT12");
	} else if (fs.FATType == 16) {
		printf("FAT16");
	} else if (fs.FATType == 32) {
		printf("FAT32");
	} else if (fs.FATType == 64) {
		printf("exFAT %u.%u", fs.bs.xxFATxx.exFAT.version.major, fs.bs.xxFATxx.exFAT.version.minor);
	}
	printf("\n");
	printf("Sector size:\t\t\t\t%d bytes\n", (int) fs.sectorSize);
	printf("FAT size:\t\t\t\t%d sectors (%d bytes)\n", (int) fs.FATSize, (int) (fs.FATSize * fs.sectorSize));
	printf("Number of FATs:\t\t\t\t%d %s\n", fs.FATCount, (checkFATs(&fs) ? "- WARNING: FATs are different!" : ""));
	printf("Cluster size:\t\t\t\t%d bytes\n", (int) fs.clusterSize);
	printf("Max. cluster chain length:\t\t%u clusters\n", (int) fs.maxClusterChainLength);
	printf("Data clusters (total / used / bad):\t%d / %d / %d\n", (int) fs.clusters, (int) usedClusters, (int) badClusters);
	printf("FS size:\t\t\t\t%.2f MiBytes\n", (float) fs.FSSize / (1024.0*1024));

	if (fs.FATType == FATTYPE_FAT32) {
		if (getFATEntry(&fs, SwapInt32(fs.bs.xxFATxx.FAT12_16_32.FATxx.FAT32.BS_RootClus), &value) == -1) {
			myerror("Failed to get FAT entry!");
			closeFileSystem(&fs);
			return -1;
		}
		printf("FAT32 root first cluster:\t\t0x%x\nFirst cluster data offset:\t\t0x%lx\nFirst cluster FAT entry:\t\t0x%x\n",
			(unsigned int) SwapInt32(fs.bs.xxFATxx.FAT12_16_32.FATxx.FAT32.BS_RootClus),
			(unsigned long) getClusterOffset(&fs, SwapInt32(fs.bs.xxFATxx.FAT12_16_32.FATxx.FAT32.BS_RootClus)), (unsigned int) value);
	} else if (fs.FATType == FATTYPE_FAT12) {
		printf("FAT12 root directory Entries:\t\t%u\n", SwapInt16(fs.bs.xxFATxx.FAT12_16_32.BS_RootEntCnt));
	} else if (fs.FATType == FATTYPE_FAT16) {
		printf("FAT16 root directory Entries:\t\t%u\n", SwapInt16(fs.bs.xxFATxx.FAT12_16_32.BS_RootEntCnt));
	} else if (fs.FATType == FATTYPE_EXFAT) {
		printf("Root directory cluster:\t\t\t%u (at 0x%"PRIx64")\n",
				SwapInt32(fs.bs.xxFATxx.exFAT.rootdir_cluster),
				(uint64_t) getClusterOffset(&fs, SwapInt32(fs.bs.xxFATxx.exFAT.rootdir_cluster)));
		printf("Allocation Bitmap cluster:\t\t%u (at 0x%"PRIx64")\n",
						fs.allocBitmapFirstCluster,
						(uint64_t) getClusterOffset(&fs, fs.allocBitmapFirstCluster));
	}

	if (OPT_MORE_INFO) {
		printf("\n\t- FAT -\n");
		printf("Cluster \tFAT entry\tChain length\n");
		for (i=0; i<fs.clusters+2; i++) {
			getFATEntry(&fs, i, &value);

			clen=0;
			if ((value & 0x0FFFFFFF ) != 0) {
				if ((chain=newClusterChain()) == NULL) {
					myerror("Failed to generate new ClusterChain!");
					return -1;
				}
				clen=getClusterChain(&fs, i, chain);
				freeClusterChain(chain);
			}

			printf("%08x\t%08x\t%u\n", i, value, clen);

		}

	}

	closeFileSystem(&fs);

	return 0;

}

int main(int argc, char *argv[]) {
/*
	parse arguments and options and start sorting
*/

	char *locale;

	// initialize rng
	srand(time(0));

	// initialize blocked signals
	init_signal_handling();
	char *filename;

	if (parse_options(argc, argv) == -1) {
		myerror("Failed to parse options!");
		return -1;
	}

	// use locale from environment or option
	locale=setlocale(LC_ALL, OPT_LOCALE);
	if (locale == NULL) {
		myerror("Could not set locale!");
		return -1;
	} else if (strncmp(locale, "C", 1) == 0){
		myerror("WARNING: The C locale does not support all multibyte characters!");
	}

	if (OPT_HELP) {
		printf(INFO_OPTION_HELP);
		return 0;
	} else if (OPT_VERSION) {
		printf(INFO_OPTION_VERSION);
		return 0;
	} else if (optind < argc -1) {
		myerror("Too many arguments!");
		myerror("Use -h for more help.");
		return -1;
	} else if (optind == argc) {
		myerror("Device must be given!");
		myerror("Use -h for more help.");
		return -1;
	}

	filename=argv[optind];

	if (OPT_INFO) {
		//infomsg(INFO_HEADER "\n\n");
		if (printFSInfo(filename) == -1) {
			myerror("Failed to print file system information");
			return -1;
		}
	} else {
		//infomsg(INFO_HEADER "\n\n");
		if (sortFileSystem(filename) == -1) {
			myerror("Failed to sort file system!");
			return -1;
		}
	}

	freeOptions();

	// report mallocv debugging information
	REPORT_MEMORY_LEAKS

	return 0;
}
