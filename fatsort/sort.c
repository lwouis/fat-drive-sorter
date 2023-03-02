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

#include "sort.h"

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <sys/param.h>
#include "entrylist.h"
#include "errors.h"
#include "options.h"
#include "endianness.h"
#include "clusterchain.h"
#include "sig.h"
#include "misc.h"
#include "deviceio.h"
#include "stringlist.h"
#include "mallocv.h"

char *getCharSet(void) {
/*
	find out character set
*/
	char *locale,*charset;
	
	locale=setlocale(LC_CTYPE, NULL);

	if (locale != NULL) {
		charset = strchr(locale, '.');
		if (charset != NULL) { 
			return charset+1;
		}
	}
	
	return NULL;
}

int32_t parseLongFilenamePart(struct sLongDirEntry *lde, char *str, iconv_t cd) {
/*
	retrieves a part of a long filename from a
	directory entry
	(thanks to M$ for this ugly hack...)
*/

	assert(lde != NULL);
	assert(str != NULL);

	size_t incount;
	size_t outcount;
	char utf16str[28];

	str[0]='\0';

	memcpy(utf16str, (&lde->LDIR_Ord+1), 10);
	memcpy(utf16str+10, (&lde->LDIR_Ord+14), 12);
	memcpy(utf16str+22, (&lde->LDIR_Ord+28), 4);
	memset(utf16str+26, 0, 2);

	incount=26;
	outcount=MAX_PATH_LEN;

	int i;
	for (i=0;i<12; i++) {
		if ((utf16str[i*2] == '\0') && (utf16str[i*2+1] == '\0')) {
			incount=i*2;
			break;
		}
	}


	//printf("Incount: %u\n", incount);

#ifdef __WIN32__

	if (WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)utf16str, (int) incount, str, (int) outcount, NULL, NULL) == 0) {
		stderror();
		return -1;
	}

#else // Linux et al

	char *outptr = &(str[0]);
	char *inptr = &(utf16str[0]);
	size_t ret;
	
	while (incount != 0) {
		if ((ret=iconv(cd, &inptr, &incount, &outptr, &outcount)) == (size_t)-1) {
			if (errno == EILSEQ) {
				outptr[0]='?';
				outptr++;
				incount+=2;
				outcount++;
				inptr+=2;
			} else {
				stderror();
				myerror("WARNING: iconv failed!");
				break;
			}
		}
	}
  	outptr[0]='\0';

#endif

	return 0;
}

void parseShortFilename(struct sShortDirEntry *sde, char *str) {
/*
	parses short name of a file
*/

	assert(sde != NULL);
	assert(str != NULL);

	char *s;
	strncpy(str, sde->DIR_Name, 8);
	str[8]='\0';
	s=strchr(str, ' ');
	if (s!=NULL) s[0]='\0';
	if ((char)(*(sde->DIR_Name+8)) != ' ') {
		strcat(str, ".");
		strncat(str, sde->DIR_Name+8, 3);
		str[12]='\0';
	}
}

int32_t checkLongDirEntries(struct sDirEntryList *list) {
/*
	does some integrity checks on LongDirEntries
*/
	assert(list != NULL);

	uint8_t calculatedChecksum;
	uint32_t i;
	uint32_t nr;
	struct sLongDirEntryList  *tmp;

	if (list->entries > 1) {
		calculatedChecksum = calculateChecksum(list->sde->DIR_Name);
		if ((list->ldel->lde->LDIR_Ord != DE_FREE) && // ignore deleted entries
			 !(list->ldel->lde->LDIR_Ord & LAST_LONG_ENTRY)) {
			myerror("LongDirEntry should be marked as last long dir entry but isn't!");
			return -1;
		}

		tmp = list->ldel;

		for(i=0;i < list->entries - 1; i++) {
			if (tmp->lde->LDIR_Ord != DE_FREE) { // ignore deleted entries
				nr=tmp->lde->LDIR_Ord & ~LAST_LONG_ENTRY;	// index of long dir entry
				//fprintf(stderr, "Debug: count=%x, LDIR_Ord=%x\n", list->entries - 1 -i, tmp->lde->LDIR_Ord);
				if (nr != (list->entries - 1 - i)) {
					myerror("LongDirEntry number is 0x%x (0x%x) but should be 0x%x!",
						nr, tmp->lde->LDIR_Ord, list->entries - 1 - i );
					return -1;
				} else if (tmp->lde->LDIR_Checksum != calculatedChecksum) {
					myerror("Checksum for LongDirEntry is 0x%x but should be 0x%x!",
						tmp->lde->LDIR_Checksum,
						calculatedChecksum);
					return -1;
				}
			}
			tmp = tmp->next;
		}
	}

	return 0;
}

uint16_t calculateExFATDirEntrySetChecksum(const struct sExFATDirEntrySet *des) {
/*
 *	calculate checksum for exFAT dir entry set
 */
	uint32_t i,j;
	uint16_t checksum=0;
	struct sExFATDirEntryList *del;

	assert(des != NULL);

	del=des->del->next;
	for (j=0; j<des->entries; j++) {
		assert(del!=NULL);
		for (i=0; i<DIR_ENTRY_SIZE; i++) {
			if ((j!=0) || ((i != 2) && (i != 3))) {
				checksum = ((checksum << 15) | (checksum >> 1)) + (uint16_t) ((uint8_t*)(&del->de))[i];
			}
		}
		del=del->next;
	}

	return checksum;
}

int32_t checkExFATDirEntrySet(const struct sExFATDirEntrySet *des) {
/*
 *	check exFAT dir entry set
 */
	assert(des != NULL);

	uint16_t checksum;

	if (STREAMEXT(des).nameLen > (des->entries-2)*15) {
		myerror("Length of directory entry name (%u) exceeds space in file name directory entries (entries: %u)!",
				STREAMEXT(des).nameLen, des->entries-2);
		return -1;
	}

	// checksum verification
	if (des->entries >= 3) {
		checksum=calculateExFATDirEntrySetChecksum(des);
		if (checksum != SwapInt16(FILEDIRENTRY(des).chksum)) {
			myerror("Checksum %04X for %s is not correct (calculated: %04X)!",
					checksum, des->name, SwapInt16(FILEDIRENTRY(des).chksum));
			return -1;
		}
	}

	return 0;
}

void printDirectoryEntryType(const struct sExFATDirEntry *de) {
/*
	prints the direcotry entry type as string
*/

	uint8_t type;

	type = de->type;

	if (type & EXFAT_FLAG_INUSE) fprintf(stderr, "INUSE ");
	if (type & EXFAT_FLAG_SECONDARY) fprintf(stderr, "SECONDARY ");
	if (!(type & EXFAT_FLAG_SECONDARY)) fprintf(stderr, "PRIMARY ");
	if (type & EXFAT_FLAG_BENIGN) fprintf(stderr, "BENIGN ");
	if ((type & EXFAT_ENTRY_TYPE_MASK) == EXFAT_ENTRY_VOLUME_LABEL) fprintf(stderr, "VOLUME_LABEL ");
	if ((type & EXFAT_ENTRY_TYPE_MASK) == EXFAT_ENTRY_ALLOC_BITMAP) fprintf(stderr, "ALLOC_BITMAP ");
	if ((type & EXFAT_ENTRY_TYPE_MASK) == EXFAT_ENTRY_UPCASE_TABLE) fprintf(stderr, "UPCASE_TABLE ");
	if ((type & EXFAT_ENTRY_TYPE_MASK) == EXFAT_ENTRY_VOLUME_GUID) fprintf(stderr, "VOLUME_GUID ");
	if ((type & EXFAT_ENTRY_TYPE_MASK) == EXFAT_ENTRY_TEXFAT_PADDING) fprintf(stderr, "TEXFAT_PADDING ");
	if ((type & EXFAT_ENTRY_TYPE_MASK) == EXFAT_ENTRY_WINCE_AC_TABLE) fprintf(stderr, "WINCE_AC_TABLE ");
	if ((type & EXFAT_ENTRY_TYPE_MASK) == EXFAT_ENTRY_FILE) fprintf(stderr, "FILE ");
	if ((type & EXFAT_ENTRY_TYPE_MASK) == EXFAT_ENTRY_STREAM_EXTENSION) fprintf(stderr, "STREAM_EXTENSION ");
	if ((type & EXFAT_ENTRY_TYPE_MASK) == EXFAT_ENTRY_FILE_NAME_EXTENSION) fprintf(stderr, "FILE_NAME_EXTENSION ");
	if ((type & EXFAT_ENTRY_TYPE_MASK) == EXFAT_ENTRY_EMPTY) fprintf(stderr, "EMPTY ");
	fprintf(stderr, "(%u)\n", type);
}

int32_t parseExFATClusterChain(struct sFileSystem *fs, struct sClusterChain *chain, struct sExFATDirEntrySetList *desl, uint32_t *direntrysets, uint32_t *reordered) {
	/*
		parses an exFAT cluster chain and puts found directory entries to list
	*/
	assert(fs != NULL);
	assert(chain != NULL);
	assert(desl != NULL);
	assert(direntrysets != NULL);
	assert(reordered != NULL);

	uint32_t j;
	int32_t ret;
	uint32_t entries=0;
	uint32_t expected_entries=0;
	uint32_t r;

	struct sExFATDirEntry de;
	struct sExFATDirEntrySet *des;
	struct sExFATDirEntryList *del=NULL;

	char name[MAX_PATH_LEN+1];
	char str[31];
	char *outptr, *inptr;
	uint8_t nameLength=0;

	size_t outcount=30;
	size_t incount, iret;

	*direntrysets=0;

	chain=chain->next;	// head element

	*reordered=0;

	name[0]='\0';
	while (chain != NULL) {
		device_seekset(fs->device, getClusterOffset(fs, chain->cluster));
		// fprintf(stderr, "cluster=%x;clusterOffset=%x\n", chain->cluster, getClusterOffset(fs, chain->cluster));
		for (j=0;j<fs->maxDirEntriesPerCluster;j++) {

			ret=parseExFATEntry(fs, &de);
			if (OPT_MORE_INFO && (ret != -1)) {
				printDirectoryEntryType(&de);
			}

			switch(ret) {
			case EXFAT_ENTRY_FILE | EXFAT_FLAG_INUSE:
				if (!entries) {
					del=newExFATDirEntryList();
					if (!del) {
						myerror("Could not create exFAT directory entry set");
						return -1;
					}
					if (insertExFATDirEntry(del, &de) == -1) {
						myerror("Could not insert exFAT file directory entry to list!");
						return -1;
					}
					entries++;
					if (de.entry.fileDirEntry.count < 2) {
						myerror("Secondary count in exFAT file directory entry is too small (%u)!",
								de.entry.fileDirEntry.count);
						return -1;
					} else if (de.entry.fileDirEntry.count > 18) {
						myerror("Secondary count in exFAT file directory entry is too big (%u)!",
								de.entry.fileDirEntry.count);
						return -1;
					}
					// expect entries from secondary count
					// secondary count does not include file dir entry
					expected_entries = de.entry.fileDirEntry.count +1;
				} else {
					myerror("Primary directory entry is not expected here (%u)!", ret);
					return -1;
				}
				break;
			case EXFAT_ENTRY_STREAM_EXTENSION | EXFAT_FLAG_INUSE:
				if (expected_entries && (entries == 1)) {
					if (insertExFATDirEntry(del, &de) == -1) {
						myerror("Could not insert exFAT stream extension directory entry to list!");
						return -1;
					}
					nameLength=de.entry.streamExtDirEntry.nameLen;
					entries++;
				} else if (!expected_entries) {
					myerror("Secondary directory entries are not expected here (%u)!", ret);
					return -1;
				} else {
					myerror("File name extension directory entry was expected (%u)!", ret);
					return -1;
				}
				break;
			case EXFAT_ENTRY_FILE_NAME_EXTENSION | EXFAT_FLAG_INUSE:
				if ((entries >= 2) && (entries < expected_entries)) {

					if (insertExFATDirEntry(del, &de) == -1) {
						myerror("Could not insert exFAT filename extension directory entry to list!");
						return -1;
					}
					entries++;

					// copy filename part to filename
					outptr = &(str[0]);
					str[0]='\0';
					inptr=&(de.entry.FileNameExtDirEntry.filename[0]);
					if (entries == expected_entries) { // last entry, so file name part is shorter
						incount = 2 * (nameLength - (entries - 3) * 15);
					} else {
						incount=30;
					}

					//printf("incount: %d, length: %d\n", incount, nameLength);
					outcount=MAX_PATH_LEN;

					/*printf("file name part: " \
						"%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx " \
						"%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx " \
						"%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx\n",
						inptr[0], inptr[1], inptr[2], inptr[3], inptr[4],
						inptr[5], inptr[6], inptr[7], inptr[8], inptr[9],
						inptr[10], inptr[11], inptr[12], inptr[13], inptr[14],
						inptr[15], inptr[16], inptr[17], inptr[18], inptr[19],
						inptr[20], inptr[21], inptr[22], inptr[23], inptr[24],
						inptr[25], inptr[26], inptr[27], inptr[28], inptr[29]
					);*/

					while (incount != 0) {
				                if ((iret=iconv(fs->cd, &inptr, &incount, &outptr, &outcount)) == (size_t)-1) {
							stderror();
				                        myerror("iconv failed! %d", iret);
							return -1;
				                }
				        }
					outptr[0]='\0';

					strncat(name, str, 31);

					// we are done here
					if (entries == expected_entries) {

						des=newExFATDirEntrySet(name, del, entries);
						if (!des) {
							myerror("Could not create exFAT directory entry set");
							return -1;
						}

						if (checkExFATDirEntrySet(des)) {
							myerror("Directory entry set check failed!");
							return -1;
						}

						if (insertExFATDirEntrySet(desl, des, &r) == -1) {
							myerror("Could not insert exFAT directory entry set to set list");
							return -1;
						}

						*reordered = *reordered || r;

						/*if (OPT_LIST) {
							printf("%s\n", name);
						}*/

						(*direntrysets)++;
						entries=0;
						name[0]='\0';
					}
				} else if (entries >= expected_entries) {
					myerror("Too many file name extension directory entries!");
					return -1;
				} else {
					myerror("File name extension directory entry is not expected here (%u)!", ret);
					return -1;
				}
				break;
			case EXFAT_ENTRY_EMPTY:
				if (entries == 0) {
					return 0;
				} else {
					myerror("%u secondary directory entries are still missing!", expected_entries - entries);
					return -1;
				}
				break;
			// we don't care for these so far
			case EXFAT_ENTRY_FILE:
			case EXFAT_ENTRY_STREAM_EXTENSION:
			case EXFAT_ENTRY_FILE_NAME_EXTENSION:
			case EXFAT_ENTRY_VOLUME_LABEL:
			case EXFAT_ENTRY_VOLUME_LABEL | EXFAT_FLAG_INUSE:
			case EXFAT_ENTRY_ALLOC_BITMAP:
			case EXFAT_ENTRY_ALLOC_BITMAP | EXFAT_FLAG_INUSE:
			case EXFAT_ENTRY_UPCASE_TABLE:
			case EXFAT_ENTRY_UPCASE_TABLE | EXFAT_FLAG_INUSE:
			case EXFAT_ENTRY_VOLUME_GUID:
			case EXFAT_ENTRY_VOLUME_GUID | EXFAT_FLAG_INUSE:
			case EXFAT_ENTRY_TEXFAT_PADDING:
			case EXFAT_ENTRY_TEXFAT_PADDING | EXFAT_FLAG_INUSE:
			case EXFAT_ENTRY_WINCE_AC_TABLE:
			case EXFAT_ENTRY_WINCE_AC_TABLE | EXFAT_FLAG_INUSE:
				if (entries == 0) {

					del=newExFATDirEntryList();

					if (!del) {
						myerror("Could not create exFAT directory entry set");
						return -1;
					}

					if (insertExFATDirEntry(del, &de) == -1) {
						myerror("Could not insert exFAT directory entry to list!");
						return -1;
					}

					des=newExFATDirEntrySet("", del, 1);
					if (!des) {
						myerror("Could not create exFAT directory entry set");
						return -1;
					}

					if (insertExFATDirEntrySet(desl, des, &r) == -1) {
						myerror("Could not insert exFAT directory entry set to set list");
						return -1;
					}

					*reordered = *reordered || r;

					(*direntrysets)++;
				} else {
					myerror("At least one secondary directory entry is still missing!");
					return -1;
				}
				break;
			case -1:
				myerror("Failed to parse cluster chain!");
				return -1;
			default:
				myerror("Unhandled return code!");
				return -1;
			}

		}
		chain=chain->next;
	}

	if (entries) {
		// some secondary directory entries are still missing
		myerror("Cluster chain ends but secondary directory entries are still missing!");
		return -1;
	}

	return 0;
}



int32_t parseClusterChain(struct sFileSystem *fs, struct sClusterChain *chain, struct sDirEntryList *list, uint32_t *direntries, uint32_t *reordered) {
/*
	parses a cluster chain and puts found directory entries to list
*/

	assert(fs != NULL);
	assert(chain != NULL);
	assert(list != NULL);
	assert(direntries != NULL);

	uint32_t j;
	int32_t ret;
	uint32_t entries=0;
	uint32_t r;
	union sDirEntry de;
	struct sDirEntryList *lnde;
	struct sLongDirEntryList *llist;
	char tmp[MAX_PATH_LEN+1], dummy[MAX_PATH_LEN+1], sname[MAX_PATH_LEN+1], lname[MAX_PATH_LEN+1];

	*direntries=0;

	chain=chain->next;	// head element

	llist = NULL;
	lname[0]='\0';
	*reordered=0;
	while (chain != NULL) {
		device_seekset(fs->device, getClusterOffset(fs, chain->cluster));
		for (j=0;j<fs->maxDirEntriesPerCluster;j++) {
			entries++;
			ret=parseEntry(fs, &de);

			switch(ret) {
			case -1:
				myerror("Failed to parse directory entry!");
				return -1;
			case 0: // current dir entry and following dir entries are free
				if (llist != NULL) {
					// short dir entry is still missing!
					myerror("ShortDirEntry is missing after LongDirEntries (cluster: %08lx, entry %u)!",
						chain->cluster, j);
					return -1;
				} else {
					return 0;
				}
			case 1: // short dir entry
				parseShortFilename(&de.ShortDirEntry, sname);
/*
				if (OPT_LIST &&
				   strcmp(sname, ".") &&
				   strcmp(sname, "..") &&
				   (((uint8_t) sname[0]) != DE_FREE) &&
				  !(de.ShortDirEntry.DIR_Atrr & ATTR_VOLUME_ID)) {

					if (!OPT_MORE_INFO) {
						printf("%s\n", (lname[0] != '\0') ? lname : sname);
					} else {
						printf("%s (%s)\n", (lname[0] != '\0') ? lname : "n/a", sname);
					}
				} else if (OPT_LIST && OPT_MORE_INFO && (((uint8_t) sname[0]) == DE_FREE)) {
					printf("!%s (#%s)\n", (lname[0] != '\0') ? lname : "n/a", sname+1);
				}
*/
				lnde=newDirEntry(sname, lname, &de.ShortDirEntry, llist, entries);
				if (lnde == NULL) {
					myerror("Failed to create DirEntry!");
					return -1;
				}

				if (checkLongDirEntries(lnde)) {
					myerror("checkDirEntry failed in cluster %08lx at entry %u!", chain->cluster, j);
					return -1;
				}

				insertDirEntryList(lnde, list, &r);
				*reordered = *reordered || r;
				(*direntries)++;
				entries=0;
				llist = NULL;
				lname[0]='\0';
				break;
			case 2: // long dir entry
				if (parseLongFilenamePart(&de.LongDirEntry, tmp, fs->cd)) {
					myerror("Failed to parse long filename part!");
					return -1;
				}

				// insert long dir entry in list
				llist=insertLongDirEntryList(&de.LongDirEntry, llist);
				if (llist == NULL) {
					myerror("Failed to insert LongDirEntry!");
					return -1;
				}

				strncpy(dummy, tmp, MAX_PATH_LEN);
				dummy[MAX_PATH_LEN]='\0';
				strncat(dummy, lname, MAX_PATH_LEN - strlen(dummy));
				dummy[MAX_PATH_LEN]='\0';
				strncpy(lname, dummy, MAX_PATH_LEN+1);

				break;
			default:
				myerror("Unhandled return code!");
				return -1;
			}

		}
		chain=chain->next;
	}

	if (llist != NULL) {
		// short dir entry is still missing!
		myerror("ShortDirEntry is missing after LongDirEntries (root directory entry %d)!", j);
		return -1;
	}

	return 0;
}

int32_t parseFat1xRootDirEntries(struct sFileSystem *fs, struct sDirEntryList *list, uint32_t *direntries, uint32_t *reordered) {
/*
	parses FAT1x root directory entries to list
*/

	assert(fs != NULL);
	assert(list != NULL);
	assert(direntries != NULL);

	off_t BSOffset;
	int32_t j, ret;
	uint32_t entries=0, r;
	union sDirEntry de;
	struct sDirEntryList *lnde;
	struct sLongDirEntryList *llist;
	char tmp[MAX_PATH_LEN+1], dummy[MAX_PATH_LEN+1], sname[MAX_PATH_LEN+1], lname[MAX_PATH_LEN+1];

	*direntries=0;

	llist = NULL;
	lname[0]='\0';
	*reordered=0;

	BSOffset = ((off_t)SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RsvdSecCnt) +
		fs->bs.xxFATxx.FAT12_16_32.BS_NumFATs * fs->FATSize) * fs->sectorSize;

	device_seekset(fs->device, BSOffset);

	for (j=0;j<SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RootEntCnt);j++) {
		entries++;
		ret=parseEntry(fs, &de);

		switch(ret) {
		case -1:
			myerror("Failed to parse directory entry!");
			return -1;
		case 0: // current dir entry and following dir entries are free
			if (llist != NULL) {
				// short dir entry is still missing!
				myerror("ShortDirEntry is missing after LongDirEntries (root directory entry %u)!", j);
				return -1;
			} else {
				return 0;
			}
		case 1: // short dir entry
			parseShortFilename(&de.ShortDirEntry, sname);

/*			if (OPT_LIST &&
			   strcmp(sname, ".") &&
			   strcmp(sname, "..") &&
			   (((uint8_t) sname[0]) != DE_FREE) &&
			  !(de.ShortDirEntry.DIR_Atrr & ATTR_VOLUME_ID)) {

				if (!OPT_MORE_INFO) {
					printf("%s\n", (lname[0] != '\0') ? lname : sname);
				} else {
					printf("%s (%s)\n", (lname[0] != '\0') ? lname : "n/a", sname);
				}
			} else if (OPT_LIST && OPT_MORE_INFO && (((uint8_t) sname[0]) == DE_FREE)) {
				printf("!%s (#%s)\n", (lname[0] != '\0') ? lname : "n/a", sname+1);
			}
*/
			lnde=newDirEntry(sname, lname, &de.ShortDirEntry, llist, entries);
			if (lnde == NULL) {
				myerror("Failed to create DirEntry!");
				return -1;
			}

			if (checkLongDirEntries(lnde)) {
				myerror("checkDirEntry failed at root directory entry %u!", j);
				return -1;
			}

			insertDirEntryList(lnde, list, &r);
			*reordered = *reordered || r;
			(*direntries)++;
			entries=0;
			llist = NULL;
			lname[0]='\0';
			break;
		case 2: // long dir entry
			if (parseLongFilenamePart(&de.LongDirEntry, tmp, fs->cd)) {
				myerror("Failed to parse long filename part!");
				return -1;
			}

			// insert long dir entry in list
			llist=insertLongDirEntryList(&de.LongDirEntry, llist);
			if (llist == NULL) {
				myerror("Failed to insert LongDirEntry!");
				return -1;
			}

			strncpy(dummy, tmp, MAX_PATH_LEN);
			dummy[MAX_PATH_LEN]='\0';
			strncat(dummy, lname, MAX_PATH_LEN - strlen(dummy));
			dummy[MAX_PATH_LEN]='\0';
			strncpy(lname, dummy, MAX_PATH_LEN+1);
			//dummy[MAX_PATH_LEN]='\0';
			break;
		default:
			myerror("Unhandled return code!");
			return -1;
		}

	}

	if (llist != NULL) {
		// short dir entry is still missing!
		myerror("ShortDirEntry is missing after LongDirEntries (root dir entry %d)!", j);
		return -1;
	}

	return 0;
}

int32_t writeList(struct sFileSystem *fs, struct sDirEntryList *list) {
/*
	writes directory entries to file
*/

	assert(fs != NULL);
	assert(list != NULL);

	struct sLongDirEntryList *tmp;

	// no signal handling while writing (atomic action)
	start_critical_section();

	while(list->next!=NULL) {
		tmp=list->next->ldel;
		while(tmp != NULL) {
			if (device_write(fs->device, tmp->lde, DIR_ENTRY_SIZE, 1)<1) {
				// end of critical section
				end_critical_section();

				stderror();
				return -1;
			}
			tmp=tmp->next;
		}
		if (device_write(fs->device, list->next->sde, DIR_ENTRY_SIZE, 1)<1) {
			// end of critical section
			end_critical_section();

			stderror();
			return -1;
		}
		list=list->next;
	}

	// sync fs
	syncFileSystem(fs);

	// end of critical section
	end_critical_section();

	return 0;
}

int32_t writeClusterChain(struct sFileSystem *fs, struct sDirEntryList *list, struct sClusterChain *chain) {
/*
	writes all entries from list to the cluster chain
*/

	assert(fs != NULL);
	assert(list != NULL);
	assert(chain != NULL);

	uint32_t i=0, entries=0;
	struct sLongDirEntryList *tmp;
	struct sDirEntryList *p=list->next;
	char empty[DIR_ENTRY_SIZE]={0};

	chain=chain->next;	// we don't need to look at the head element

	if (device_seekset(fs->device, getClusterOffset(fs, chain->cluster))==-1) {
		myerror("Seek error!");
		return -1;
	}

	// no signal handling while writing (atomic action)
	start_critical_section();

	while(p != NULL) {
		if (entries+p->entries <= fs->maxDirEntriesPerCluster) {
			tmp=p->ldel;
			for (i=1;i<p->entries;i++) {
				if (device_write(fs->device, tmp->lde, DIR_ENTRY_SIZE, 1)<1) {
					// end of critical section
					end_critical_section();

					stderror();
					return -1;
				}
				tmp=tmp->next;
			}
			if (device_write(fs->device, p->sde, DIR_ENTRY_SIZE, 1)<1) {
				// end of critical section
				end_critical_section();

				stderror();
				return -1;
			}
			entries+=p->entries;
		} else {
			tmp=p->ldel;
			for (i=1;i<=fs->maxDirEntriesPerCluster-entries;i++) {
				if (device_write(fs->device, tmp->lde, DIR_ENTRY_SIZE, 1)<1) {
					// end of critical section
					end_critical_section();

					stderror();
					return -1;
				}
				tmp=tmp->next;
			}
			chain=chain->next; entries=p->entries - (fs->maxDirEntriesPerCluster - entries);	// next cluster
			if (device_seekset(fs->device, getClusterOffset(fs, chain->cluster))==-1) {

				// end of critical section
				end_critical_section();

				myerror("Seek error!");
				return -1;
			}
			while(tmp!=NULL) {
				if (device_write(fs->device, tmp->lde, DIR_ENTRY_SIZE, 1)<1) {
					// end of critical section
					end_critical_section();

					stderror();
					return -1;
				}
				tmp=tmp->next;
			}
			if (device_write(fs->device, p->sde, DIR_ENTRY_SIZE, 1)<1) {
				// end of critical section
				end_critical_section();

				stderror();
				return -1;
			}
		}
		p=p->next;
	}
	if (entries < fs->maxDirEntriesPerCluster) {
		if (device_write(fs->device, empty, DIR_ENTRY_SIZE, 1)<1) {
			// end of critical section
			end_critical_section();

			stderror();
			return -1;
		}
	}

	// sync fs
	syncFileSystem(fs);

	// end of critical section
	end_critical_section();

	return 0;

}

int32_t writeExFATClusterChain(struct sFileSystem *fs, struct sExFATDirEntrySetList *desl, struct sClusterChain *chain) {
/*
	writes all entries from list to the cluster chain (exFAT)
*/

	assert(fs != NULL);
	assert(desl != NULL);
	assert(chain != NULL);

	uint32_t i=0, entries=0;
	struct sExFATDirEntryList *tmp;
	struct sExFATDirEntrySetList *p=desl->next;
	char empty[DIR_ENTRY_SIZE]={0};

	chain=chain->next;	// we don't need to look at the head element

	if (device_seekset(fs->device, getClusterOffset(fs, chain->cluster))==-1) {
		myerror("Seek error!");
		return -1;
	}

	// no signal handling while writing (atomic action)
	start_critical_section();
	// fprintf(stderr, "Startcluster: %u\n", chain->cluster);
	while(p != NULL) {
		// fprintf(stderr, "Entries: %u\n", p->des->entries);
		if (entries+p->des->entries <= fs->maxDirEntriesPerCluster) {
			tmp=p->des->del->next;
			for (i=1;i<=p->des->entries;i++) {
				if (device_write(fs->device, &tmp->de, DIR_ENTRY_SIZE, 1)<1) {
					// end of critical section
					end_critical_section();

					stderror();
					return -1;
				}
				tmp=tmp->next;
			}
			entries+=p->des->entries;
		} else {
			tmp=p->des->del->next;
			for (i=1;i<=fs->maxDirEntriesPerCluster-entries;i++) {
				if (device_write(fs->device, &(tmp->de), DIR_ENTRY_SIZE, 1)<1) {
					// end of critical section
					end_critical_section();

					stderror();
					return -1;
				}
				tmp=tmp->next;
			}
			chain=chain->next; entries=p->des->entries - (fs->maxDirEntriesPerCluster - entries);	// next cluster
			if (device_seekset(fs->device, getClusterOffset(fs, chain->cluster))==-1) {

				// end of critical section
				end_critical_section();

				myerror("Seek error!");
				return -1;
			}
			while(tmp!=NULL) {
				if (device_write(fs->device, &(tmp->de), DIR_ENTRY_SIZE, 1)<1) {
					// end of critical section
					end_critical_section();

					stderror();
					return -1;
				}
				tmp=tmp->next;
			}
		}
		p=p->next;
	}
	if (entries < fs->maxDirEntriesPerCluster) {
		if (device_write(fs->device, empty, DIR_ENTRY_SIZE, 1)<1) {
			// end of critical section
			end_critical_section();

			stderror();
			return -1;
		}
	}

	// sync fs
	syncFileSystem(fs);

	// end of critical section
	end_critical_section();

	return 0;

}

int32_t sortSubdirectories(struct sFileSystem *fs, struct sDirEntryList *list, const char (*path)[MAX_PATH_LEN+1]) {
/*
	sorts sub directories in a FAT file system
*/
	assert(fs != NULL);
	assert(list != NULL);
	assert(path != NULL);

	struct sDirEntryList *p;
	char newpath[MAX_PATH_LEN+1]={0};
	uint32_t c;

	// sort sub directories
	p=list->next;
	while (p != NULL) {
		if ((p->sde->DIR_Atrr & ATTR_DIRECTORY) &&
			((uint8_t) p->sde->DIR_Name[0] != DE_FREE) &&
			!(p->sde->DIR_Atrr & ATTR_VOLUME_ID) &&
			(strcmp(p->sname, ".")) && strcmp(p->sname, "..")) {

			c=(SwapInt16(p->sde->DIR_FstClusHI) * 65536 + SwapInt16(p->sde->DIR_FstClusLO));
/*			if (getFATEntry(fs, c, &value) == -1) {
				myerror("Failed to get FAT entry!");
				return -1;
			}
*/
			if ((p->lname != NULL) && (p->lname[0] != '\0')) {
				snprintf(newpath, MAX_PATH_LEN+1, "%s%s%c", (char*) path, p->lname, DIRECTORY_SEPARATOR);
			} else {
				snprintf(newpath, MAX_PATH_LEN+1, "%s%s%c", (char*) path, p->sname, DIRECTORY_SEPARATOR);
			}

			if (sortClusterChain(fs, c, (const char(*)[MAX_PATH_LEN+1]) newpath) == -1) {
				myerror("Failed to sort cluster chain!");
				return -1;
			}

		}
		p=p->next;
	}

	return 0;
}

int32_t sortExFATSubdirectories(struct sFileSystem *fs, struct sExFATDirEntrySetList *desl, const char (*path)[MAX_PATH_LEN+1]) {
/*
	sorts sub directories in a FAT file system
*/
	assert(fs != NULL);
	assert(desl != NULL);
	assert(path != NULL);

	struct sExFATDirEntrySetList *p;
	char newpath[MAX_PATH_LEN+1]={0};
	uint32_t c;

	// sort sub directories
	p=desl->next;
	while (p != NULL) {
		if ((FIRSTENTRY(p->des).type & EXFAT_FLAG_INUSE) &&
		   (EXFAT_ISTYPE(FIRSTENTRY(p->des), EXFAT_ENTRY_FILE)) &&
		   (EXFAT_HASATTR(FILEDIRENTRY(p->des), EXFAT_ATTR_DIR))) {

			c=SwapInt32(STREAMEXT(p->des).firstCluster);

		/*	if (getFATEntry(fs, c, &value) == -1) {
				myerror("Failed to get FAT entry!");
				return -1;
			}*/

			strncpy(newpath, (char*) path, MAX_PATH_LEN - strlen(newpath));
			newpath[MAX_PATH_LEN]='\0';
			strncat(newpath, p->des->name, MAX_PATH_LEN - strlen(newpath));
			newpath[MAX_PATH_LEN]='\0';
			strncat(newpath, "/", MAX_PATH_LEN - strlen(newpath));
			newpath[MAX_PATH_LEN]='\0';

			if (sortExFATClusterChain(fs, c,
					(SwapInt64(STREAMEXT(p->des).validDataLen) + fs->clusterSize -1) / fs->clusterSize,
					STREAMEXT(p->des).genSecFlags & EXFAT_GSFLAG_FAT_INVALID,
					(const char(*)[MAX_PATH_LEN+1]) newpath) == -1) {
				myerror("Failed to sort cluster chain!");
				return -1;
			}
		}
		p=p->next;
	}

	return 0;
}

void printDirEntryList(struct sDirEntryList *del) {


	while(del->next != NULL) {

		if (strcmp(del->next->sname, ".") &&
			   strcmp(del->next->sname, "..") &&
			   (((uint8_t) del->next->sname[0]) != DE_FREE) &&
			  !(del->next->sde->DIR_Atrr & ATTR_VOLUME_ID)) {

				if (!OPT_MORE_INFO) {
					printf("%s\n", (del->next->lname[0] != '\0') ? del->next->lname : del->next->sname);
				} else {
					printf("%s (%s)\n", (del->next->lname[0] != '\0') ? del->next->lname : "n/a", del->next->sname);
				}
			} else if (OPT_MORE_INFO && (((uint8_t) del->next->sname[0]) == DE_FREE)) {
				printf("!%s (#%s)\n", (del->next->lname[0] != '\0') ? del->next->lname : "n/a", del->next->sname+1);
		}

		del=del->next;
	}
	printf("\n");
}

int32_t sortClusterChain(struct sFileSystem *fs, uint32_t cluster, const char (*path)[MAX_PATH_LEN+1]) {
/*
	sorts directory entries in a cluster
*/

	assert(fs != NULL);
	assert(path != NULL);

	uint32_t direntries;
	int32_t clen;
	struct sClusterChain *ClusterChain;
	struct sDirEntryList *list;
	uint32_t reordered;

	uint32_t match;

	if (!OPT_REGEX) {
		match=matchesDirPathLists(OPT_INCL_DIRS, OPT_INCL_DIRS_REC, OPT_EXCL_DIRS, OPT_EXCL_DIRS_REC, path);
	} else {
		match=!matchesRegExList(OPT_REGEX_EXCL, (const char *) path);
		if (OPT_REGEX_INCL->next != NULL) match &= matchesRegExList(OPT_REGEX_INCL, (const char *) path);
	}

	if ((ClusterChain=newClusterChain()) == NULL) {
		myerror("Failed to generate new ClusterChain!");
		return -1;
	}

	if ((list = newDirEntryList()) == NULL) {
		myerror("Failed to generate new dirEntryList!");
		freeClusterChain(ClusterChain);
		return -1;
	}

	if ((clen=getClusterChain(fs, cluster, ClusterChain)) == -1 ) {
		myerror("Failed to get cluster chain!");
		freeDirEntryList(list);
		freeClusterChain(ClusterChain);
		return -1;
	}

	if (match) {
		if (!OPT_LIST) {
			infomsg("Sorting directory %s\n", path);
			if (OPT_MORE_INFO)
				infomsg("Start cluster: %08lx, length: %d (%d bytes)\n",
					cluster, clen, clen*fs->clusterSize);
		} else {
			printf("%s\n", (char*) path);
			if (OPT_MORE_INFO)
				infomsg("Start cluster: %08lx, length: %d (%d bytes)\n",
					cluster, clen, clen*fs->clusterSize);
		}
	}

	if (parseClusterChain(fs, ClusterChain, list, &direntries, &reordered) == -1) {
		myerror("Failed to parse cluster chain!");
		freeDirEntryList(list);
		freeClusterChain(ClusterChain);
		return -1;
	}

	if (match) {
		if (!OPT_LIST) {
			// sort directory if reordering was neccessary
			// feature: crash-safe implementation
			if (OPT_RANDOM) randomizeDirEntryList(list);

			if (reordered || OPT_RANDOM) {
				infomsg("Directory reordered. Writing changes.\n");

				if (writeClusterChain(fs, list, ClusterChain) == -1) {
					myerror("Failed to write cluster chain!");
					freeDirEntryList(list);
					freeClusterChain(ClusterChain);
					return -1;
				}
			}
		} else {
			printDirEntryList(list);
		}
	}

	freeClusterChain(ClusterChain);

	// sort subdirectories
	if (sortSubdirectories(fs, list, path) == -1 ){
		myerror("Failed to sort subdirectories!");
		return -1;
	}

	freeDirEntryList(list);

	return 0;
}

void printExFatDirEntrySets(struct sExFATDirEntrySetList *desl) {


	while(desl->next != NULL) {
		if (desl->next->des->name[0] != '\0') {
			printf("%s\n", desl->next->des->name);
		}
		desl=desl->next;
	}
	printf("\n");
}

int32_t sortExFATClusterChain(struct sFileSystem *fs, uint32_t cluster, uint32_t len, uint16_t isContiguous, const char (*path)[MAX_PATH_LEN+1]) {
/*
	sorts directory entries in a cluster
*/

	assert(fs != NULL);
	assert(path != NULL);
	assert(!isContiguous || (len != 0));	// if it is contiguous data, we need to know the length

	uint32_t direntrysets;
	int32_t clen;
	struct sClusterChain *ClusterChain;
	struct sExFATDirEntrySetList *desl;
	uint32_t i;

	uint32_t match;
	uint32_t reordered=0;

	if (!OPT_REGEX) {
		match=matchesDirPathLists(OPT_INCL_DIRS,
					OPT_INCL_DIRS_REC,
					OPT_EXCL_DIRS,
					OPT_EXCL_DIRS_REC,
					path);
	} else {
		match=!matchesRegExList(OPT_REGEX_EXCL, (const char *) path);
		if (OPT_REGEX_INCL->next != NULL) match &= matchesRegExList(OPT_REGEX_INCL, (const char *) path);
	}

	if ((ClusterChain=newClusterChain()) == NULL) {
		myerror("Failed to generate new ClusterChain!");
		return -1;
	}

	if ((desl = newExFATDirEntrySetList()) == NULL) {
		myerror("Failed to generate new dirEntrySetList!");
		freeClusterChain(ClusterChain);
		return -1;
	}

	if (isContiguous) { // len clusters starting from cluster
		clen=len;
		for(i=0; i<len; i++) {
			if (insertCluster(ClusterChain, cluster+i) == -1) {
				myerror("Failed to insert cluster!");
				freeExFATDirEntrySetList(desl);
				freeClusterChain(ClusterChain);
				return -1;
			}
		}
	} else { // OK, we have to lookup the cluster chain from FAT
		if ((clen=getClusterChain(fs, cluster, ClusterChain)) == -1 ) {
			myerror("Failed to get cluster chain!");
			freeExFATDirEntrySetList(desl);
			freeClusterChain(ClusterChain);
			return -1;
		}
	}

	if (match) {
		if (!OPT_LIST) {
			infomsg("Sorting directory %s\n", path);
		} else {
			printf("%s\n", (char*) path);
		}
		if (OPT_MORE_INFO)
			infomsg("Start cluster: %08lx, length: %d (%d bytes)\n",
				cluster, clen, clen*fs->clusterSize);
	}

	if (parseExFATClusterChain(fs, ClusterChain, desl, &direntrysets, &reordered) == -1) {
		myerror("Failed to parse cluster chain!");
		freeExFATDirEntrySetList(desl);
		freeClusterChain(ClusterChain);
		return -1;
	}

	if (match) {
		// sort directory if selected
		if (!OPT_LIST) {

			if (OPT_RANDOM) randomizeExFATDirEntrySetList(desl, direntrysets);

			if (reordered || OPT_RANDOM) {
				infomsg("Directory reordered. Writing changes.\n");

				// feature: crash-safe implementation
				if (writeExFATClusterChain(fs, desl, ClusterChain) == -1) {
					myerror("Failed to write cluster chain!");
					freeExFATDirEntrySetList(desl);
					freeClusterChain(ClusterChain);
					return -1;
				}
			}
		} else {
			printExFatDirEntrySets(desl);
		}
	}

	freeClusterChain(ClusterChain);

	// sort subdirectories
	if (sortExFATSubdirectories(fs, desl, path) == -1 ){
		myerror("Failed to sort sub directories!");
		return -1;
	}

	freeExFATDirEntrySetList(desl);

	return 0;
}

int32_t sortFat1xRootDirectory(struct sFileSystem *fs) {
/*
	sorts the root directory of a FAT12 or FAT16 file system
*/

	assert(fs != NULL);

	off_t BSOffset;

	uint32_t direntries=0;

	struct sDirEntryList *list;

	uint32_t match, reordered;
	const char rootDir[2] = {DIRECTORY_SEPARATOR, '\0'};

	if (!OPT_REGEX) {
		match=matchesDirPathLists(OPT_INCL_DIRS,
					OPT_INCL_DIRS_REC,
					OPT_EXCL_DIRS,
					OPT_EXCL_DIRS_REC,
					(const char(*)[MAX_PATH_LEN+1]) rootDir);
	} else {
		match=!matchesRegExList(OPT_REGEX_EXCL, rootDir);
		if (OPT_REGEX_INCL->next != NULL) match &= matchesRegExList(OPT_REGEX_INCL, rootDir);
	}

	if (match) {
		if (!OPT_LIST) {
			infomsg("Sorting directory /\n");
		} else {
			printf("%c\n", DIRECTORY_SEPARATOR);
		}
	}

	if ((list = newDirEntryList()) == NULL) {
		myerror("Failed to generate new dirEntryList!");
		return -1;
	}

	if (parseFat1xRootDirEntries(fs, list, &direntries, &reordered) == -1) {
		myerror("Failed to parse root directory entries!");
		return -1;
	}

	if (match) {
		if (!OPT_LIST) {
			// sort directory if reordering was neccessary
			// feature: crash-safe implementation
			if (OPT_RANDOM) randomizeDirEntryList(list);

			if (reordered || OPT_RANDOM) {

				infomsg("Directory reordered. Writing changes.\n");

				BSOffset = ((off_t)SwapInt16(fs->bs.xxFATxx.FAT12_16_32.BS_RsvdSecCnt) +
					fs->bs.xxFATxx.FAT12_16_32.BS_NumFATs * fs->FATSize) * fs->sectorSize;
				device_seekset(fs->device, BSOffset);

				// write the sorted entries back to the fs
				if (writeList(fs, list) == -1) {
					freeDirEntryList(list);
				  	myerror("Failed to write root directory entries!");
					return -1;
				}
			}

		} else {
			printDirEntryList(list);
		}
	}

	// sort subdirectories
	if (sortSubdirectories(fs, list, (const char (*)[MAX_PATH_LEN+1]) rootDir) == -1 ){
		myerror("Failed to sort subdirectories!");
		freeDirEntryList(list);
		return -1;
	}

	freeDirEntryList(list);

	return 0;
}

int32_t sortFileSystem(char *filename) {
/*
	sort FAT file system
*/

	assert(filename != NULL);

	uint32_t mode = FS_MODE_RW;

	struct sFileSystem fs;

	const char rootDir[2] = {DIRECTORY_SEPARATOR, '\0'};

	if (!OPT_FORCE && OPT_LIST) {
		mode = FS_MODE_RO_EXCL;
	} else if (!OPT_FORCE && !OPT_LIST) {
		mode = FS_MODE_RW_EXCL;
	} else if (OPT_FORCE && OPT_LIST) {
		mode = FS_MODE_RO;
	}

	if (openFileSystem(filename, mode, &fs)) {
		myerror("Failed to open file system!");
		return -1;
	}

	if (checkFATs(&fs)) {
		myerror("FATs don't match! Please repair file system!");
		closeFileSystem(&fs);
		return -1;
	}

	switch(fs.FATType) {
	case FATTYPE_FAT12:
		// FAT12
		// root directory has fixed size and position
		infomsg("File system: FAT12.\n\n");
		if (sortFat1xRootDirectory(&fs) == -1) {
			myerror("Failed to sort FAT12 root directory!");
			closeFileSystem(&fs);
			return -1;
		}
		break;
	case FATTYPE_FAT16:
		// FAT16
		// root directory has fixed size and position
		infomsg("File system: FAT16.\n\n");
		if (sortFat1xRootDirectory(&fs) == -1) {
			myerror("Failed to sort FAT16 root directory!");
			closeFileSystem(&fs);
			return -1;
		}
		break;
	case FATTYPE_FAT32:
		// FAT32
		// root directory lies in cluster chain,
		// so sort it like all other directories
		infomsg("File system: FAT32.\n\n");
		if (sortClusterChain(&fs, SwapInt32(fs.bs.xxFATxx.FAT12_16_32.FATxx.FAT32.BS_RootClus), (const char(*)[MAX_PATH_LEN+1]) rootDir) == -1) {
			myerror("Failed to sort first cluster chain!");
			closeFileSystem(&fs);
			return -1;
		}
		break;
	case FATTYPE_EXFAT:
		// exFAT
		// root directory lies in cluster chain,
		// so sort it like all other directories
		infomsg("File system: exFAT.\n\n");
		if (sortExFATClusterChain(&fs, SwapInt32(fs.bs.xxFATxx.exFAT.rootdir_cluster), 0, 0, (const char(*)[MAX_PATH_LEN+1]) "/") == -1) {
			myerror("Failed to sort first cluster chain!");
			closeFileSystem(&fs);
			return -1;
		}
		break;
	default:
		myerror("Failed to get FAT type!");
		closeFileSystem(&fs);
		return -1;
	}

	closeFileSystem(&fs);

	return 0;
}
