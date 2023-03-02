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

#include "deviceio.h"

#if defined __LINUX__ || defined __BSD__ || defined __OSX__ 

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#elif defined __WIN32__

//#include <fileapi.h>
//#include <WinNT.h>
#include <windows.h>
#include <ddk/ntddstor.h>
#include <ctype.h>
#include <malloc.h>


typedef struct _STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR {
  DWORD Version;
  DWORD Size;
  DWORD BytesPerCacheLine;
  DWORD BytesOffsetForCacheAlignment;
  DWORD BytesPerLogicalSector;
  DWORD BytesPerPhysicalSector;
  DWORD BytesOffsetForSectorAlignment;
} STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR, *PSTORAGE_ACCESS_ALIGNMENT_DESCRIPTOR;

#endif

#include <stdio.h>
#include <assert.h>

#include "mallocv.h"
#include "errors.h"

#if defined __LINUX__ || defined __BSD__ || defined __OSX__ 

DEVICE *device_open(const char *path) {

  assert(path != NULL);

  int fd;
  DEVICE *dev;

  if ((fd=open(path, O_RDWR | O_EXCL)) == -1) {
    stderror();
    return NULL;
  }

  if ((dev=malloc(sizeof(DEVICE))) == NULL) {
    stderror();
    return NULL;
  }

  dev->fd=fd;

  return dev;
}

int64_t device_seekset(DEVICE *device, int64_t offset) {

  assert(device != NULL);
  assert(offset >= 0);

#if defined __BSD__ || defined __OSX__ 
  return lseek(device->fd, (off_t) offset, SEEK_SET);
#else
  return lseek64(device->fd, (off64_t) offset, SEEK_SET);
#endif
}

int64_t device_read(DEVICE *device, void *data, uint64_t size, uint64_t n) {

  assert(device != NULL);
  assert(data != NULL);

  return read(device->fd, data, (size_t) size * n);
}

int64_t device_write(DEVICE *device, const void *data, uint64_t size, uint64_t n) {

  assert(device != NULL);
  assert(data != NULL);

  return write(device->fd, (void *) data, (size_t) size * n);
}

int device_sync(DEVICE *device) {

  assert(device != NULL);

  return fsync(device->fd);
}

int device_close(DEVICE *device) {

  assert(device != NULL);

  if(!close(device->fd)) {
    free((void*) device);
    return 0;
  } else {
    return -1;
  }
}

#elif defined __WIN32__

void printSector(char *data) {
  int i,j;
  
  assert(data != NULL);

  for (j=0; j<16; j++) {
    for(i=0; i<32; i++) {
      __mingw_fprintf(stderr, "%02hhx", data[j*8+i]);
    }
    fprintf(stderr, "\n");
  }  
}

int32_t seekSector(DEVICE *device, uint64_t sectorNr) {

  assert(device != NULL);

  uint64_t offset;	
  LONG low, high;
  DWORD newLow;

  offset=sectorNr * device->sectorSize;
  high= (long) (offset / 0x100000000);
  low= (long) (offset % 0x100000000);
  
  newLow=SetFilePointer(device->h, low, &high, FILE_BEGIN);
  if (newLow == INVALID_SET_FILE_POINTER) {
    myerror("SetFilePointer failed (%u)!", GetLastError());
    return -1;
  }
  
  return 0;
  
}

int32_t writeBufferToSector(DEVICE *device) {

  DWORD nBytesWritten;

  assert(device != NULL);

  if (WriteFile(device->h, (LPCVOID) device->buffer, (DWORD) device->sectorSize, &nBytesWritten, NULL)) {
	return 0;
  } else {
	myerror("WriteFile failed (%u)!", GetLastError());
	return -1;
  }
}

int32_t readSectorToBuffer(DEVICE *device) {

  DWORD nBytesRead;

  assert(device != NULL);

  if (ReadFile(device->h, (LPVOID) device->buffer, (DWORD) device->sectorSize, &nBytesRead, NULL)) {
    
    //printSector(device->buffer);
    return 0;
    
  } else {
    
    myerror("ReadFile failed (%u)!", GetLastError());
    return -1;
    
  }
}

int32_t flushBuffer(DEVICE *device){

	assert(device != NULL);

	if (device->unwrittenChanges) {	 
	
		if (seekSector(device, device->bufferedSector)) {
			 myerror("Failed to seek to sector %u!", device->bufferedSector);
			 return -1;				
		}
	
		 if (writeBufferToSector(device)) {
			 myerror("Failed to write back buffered sector %u!", device->bufferedSector);
			 return -1;
		 }
		 device->unwrittenChanges=0;
	}
	
	return 0;
}

DEVICE *device_open(const char *path) {

  HANDLE h;
  DEVICE *dev;
  int isDrive =0;
  char drivePath[7];
  LPCSTR winPath;

  assert(path != NULL);

  // path is a windows drive letter (e.g. "C:")
  if ((strlen(path) == 2) && (path[1] == ':')) {
	  char driveLetter = toupper(path[0]);
	  if (!((driveLetter >= 'A') && (driveLetter <= 'Z'))) {
      myerror("'%c' is not a valid drive letter!", driveLetter);
      return NULL;
	  }
	  snprintf(drivePath, 7, "\\\\.\\%c:", driveLetter);
    winPath=(LPCSTR) drivePath;
	  isDrive=1;  // remember, because we need to implement our own buffering
  } else {
    winPath=(LPCSTR) path;
  }
  
  if ((h=CreateFileA(winPath,          // lpFileName
        GENERIC_READ | GENERIC_WRITE, // dwDesiredAccess
        0,                            // dwShareMode: Prevents other processes from opening a file or device
        NULL,                         // lpSecurityAttributes
        OPEN_EXISTING,                // dwCreationDisposition
        0,                            // dwFlagsAndAttributes
        NULL                          // hTemplateFile
      )) == INVALID_HANDLE_VALUE) {
    myerror("CreateFileA failed (%u)!", GetLastError());
    return NULL;
  }

  if ((dev=malloc(sizeof(DEVICE))) == NULL) {
    stderror();
    return NULL;
  }

  dev->h=h;
  if (isDrive) {  // things needed for our own buffering
    dev->isDrive=isDrive;
    
    STORAGE_PROPERTY_QUERY query;
    STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR desc;
    query.PropertyId=6;//StorageAccessAlignmentProperty;
    query.QueryType=PropertyStandardQuery;
    DWORD bytesReturned;
    
    if (!DeviceIoControl(
       dev->h,               // handle to a partition
       IOCTL_STORAGE_QUERY_PROPERTY, // dwIoControlCode
       (LPVOID)       &query,            // input buffer - STORAGE_PROPERTY_QUERY structure
       sizeof(STORAGE_PROPERTY_QUERY),         // size of input buffer
       (LPVOID)       &desc,           // output buffer - see Remarks
       sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR),        // size of output buffer
       &bytesReturned,       // number of bytes returned
       NULL)        // OVERLAPPED structure
       ) {
      myerror("Failed to get physical sector size of volume!");
      return NULL;
    }
     
    //fprintf(stderr, "%I32u", desc.BytesPerPhysicalSector);
    
    dev->sectorSize=desc.BytesPerPhysicalSector;
    dev->currentOffset=0;
    dev->unwrittenChanges=0;
    dev->bufferedSector=SECTOR_NONE;
    
    if ((dev->buffer=__mingw_aligned_malloc(dev->sectorSize, dev->sectorSize)) == NULL) {
      stderror();
      return NULL;
    }

  }

  return dev;
}

int64_t device_seekset(DEVICE *device, int64_t offset) {

  assert(device != NULL);
  assert(offset >= 0);

  if (device->isDrive) {
    
    uint64_t newSector=offset / device->sectorSize;
    
    // seek to new sector if needed
    if (device->currentOffset / device->sectorSize != newSector) {
      if (seekSector(device, newSector)) {
        myerror("Failed to seek to sector %u!", newSector);
        return -1;				
      }
    }
    
    device->currentOffset=offset;
    
    //fprintf(stderr, "newSector: %I64u, currentOffset=%I64u\n", newSector, device->currentOffset);
    
    return 0;
	  
  } else {
    
    LONG low, high;
    DWORD newLow;

	  high= (long) (offset / 0x100000000);
	  low= (long) (offset % 0x100000000);
	  
	  newLow=SetFilePointer(device->h, low, &high, FILE_BEGIN);
	  //fprintf(stderr, "newLow:%lu\n", newLow);

	  if (newLow != INVALID_SET_FILE_POINTER) {
		   return (int64_t) (high * 0x100000000 + newLow);
	  } else {
      myerror("SetFilePointer failed (%u)!", GetLastError());
      return -1;
    }

  }
}

int64_t device_read(DEVICE *device, void *data, uint64_t size, uint64_t n) {
  
  assert(device != NULL);
  assert(data != NULL);

  if (device->isDrive) {
    
    uint64_t remaining = size * n;
    
    // make sure that sector for current offset is buffered
    if (device->bufferedSector == SECTOR_NONE) {
      
      device->bufferedSector=device->currentOffset / device->sectorSize;
      
      if (readSectorToBuffer(device)) {
        myerror("Failed to read current sector %u!", device->bufferedSector);
        return -1;
      }

    } else if (device->bufferedSector != device->currentOffset / device->sectorSize) {
      
      flushBuffer(device);
      
      device->bufferedSector=device->currentOffset / device->sectorSize;
      
      if (readSectorToBuffer(device)) {
        myerror("Failed to read current sector %u!", device->bufferedSector);
        return -1;
      }      
    }

    // read data from first sector
    if (remaining <= device->sectorSize-(device->currentOffset % device->sectorSize)) {
      memcpy(data, device->buffer+(device->currentOffset % device->sectorSize), remaining);
      device->currentOffset+=remaining;
      remaining=0;
    } else {
      memcpy(data, device->buffer+(device->currentOffset % device->sectorSize), device->sectorSize);
      remaining-=device->sectorSize;
      data+=device->sectorSize;
      device->currentOffset+=device->sectorSize;
    }
    
    // flush if we're about to buffer another sector
    if (remaining > 0) {
      flushBuffer(device);
    }
	
    // copy remaining data from subsequent sectors
    while (remaining > 0) {
      
      device->bufferedSector=device->currentOffset / device->sectorSize;
      if (readSectorToBuffer(device)) {
        myerror("Failed to read current sector %u!", device->bufferedSector);
        return -1;
      }

      if (remaining < device->sectorSize) {
        memcpy(data, device->buffer, remaining);
        device->currentOffset+=remaining;
        remaining=0;
      } else {
        memcpy(data, device->buffer, device->sectorSize);
        data+=device->sectorSize;
        remaining-=device->sectorSize;
        device->currentOffset+=device->sectorSize;
      }
        
    }
    
    
/*    uint64_t seekOffset=SetFilePointer(device->h, 0, NULL, FILE_CURRENT);
    fprintf(stderr, "bytesRead=%I64u, currentOffset=%I64u, seekOffset=%I64u, bufferedSector=%I64u\n",
            n*size, device->currentOffset, seekOffset, device->bufferedSector);
*/
    return n;
	  
  } else {
    
    DWORD nBytesRead;

	  if (ReadFile(device->h, (LPVOID) data, (DWORD) n*size, &nBytesRead, NULL)) {
	/*	  int64_t i;
		  fprintf(stderr, "bytes: %u\n", n*size);
		  for (i=0;i<nBytesRead;i++) {
			  fprintf(stderr, "%02hhhx ", ((char*)data)[i]);
		  }
		  */
		   return (int64_t) nBytesRead;
	  } else {
		myerror("ReadFile failed (%u)!", GetLastError());
		return -1;
	  }
  }
}

int64_t device_write(DEVICE *device, const void *data, uint64_t size, uint64_t n) {
  
  assert(device != NULL);
  assert(data != NULL);

  if (device->isDrive) {
	  
    uint64_t remaining = size * n;
    
    // make sure that sector for current offset is buffered
    if (device->bufferedSector == SECTOR_NONE) {
      
      device->bufferedSector=device->currentOffset / device->sectorSize;
      
      // only read buffer if we do not overwrite it completely
      if (remaining < device->sectorSize-(device->currentOffset % device->sectorSize)) {
        if (readSectorToBuffer(device)) {
          myerror("Failed to read current sector %u!", device->bufferedSector);
          return -1;
        }
      }

    } else if (device->bufferedSector != device->currentOffset / device->sectorSize) {
      
      flushBuffer(device);
      
      device->bufferedSector=device->currentOffset / device->sectorSize;
      
      // only read buffer if we do not overwrite it completely
      if (remaining < device->sectorSize-(device->currentOffset % device->sectorSize)) {
        if (readSectorToBuffer(device)) {
          myerror("Failed to read current sector %u!", device->bufferedSector);
          return -1;
        }
      }
    }

    // write data to first sector
    if (remaining <= device->sectorSize-(device->currentOffset % device->sectorSize)) {
      memcpy(device->buffer+(device->currentOffset % device->sectorSize), data, remaining);
      device->currentOffset+=remaining;
      remaining=0;
    } else {
      memcpy(device->buffer+(device->currentOffset % device->sectorSize), data, device->sectorSize);
      remaining-=device->sectorSize;
      data+=device->sectorSize;
      device->currentOffset+=device->sectorSize;
    }
    device->unwrittenChanges=1;
	
    // copy remaining data from subsequent sectors
    while (remaining > 0) {
      
      // flush if we're about to buffer another sector
      flushBuffer(device);
      
      if (remaining < device->sectorSize) {

        // we only write parts to sector so read it first
        device->bufferedSector=device->currentOffset / device->sectorSize;
        if (readSectorToBuffer(device)) {
          myerror("Failed to read current sector %u!", device->bufferedSector);
          return -1;
        }
        
        memcpy(device->buffer, data, remaining);
        device->currentOffset+=remaining;
        remaining=0;
      } else {
        
        // we overwrite the sector completely, so no read needed
        device->bufferedSector=device->currentOffset / device->sectorSize;

        memcpy(device->buffer, data, device->sectorSize);
        data+=device->sectorSize;
        remaining-=device->sectorSize;
        device->currentOffset+=device->sectorSize;
      }
      
      // current buffered sectored has been updated
      device->unwrittenChanges=1;
      
    }
    
    /*
    uint64_t seekOffset=SetFilePointer(device->h, 0, NULL, FILE_CURRENT);
    fprintf(stderr, "bytesRead=%I64u, currentOffset=%I64u, seekOffset=%I64u, bufferedSector=%I64u\n",
            n*size, device->currentOffset, seekOffset, device->bufferedSector);
*/

    return n;
    
  } else {
    
    DWORD nBytesWritten;

    if (WriteFile(device->h, (LPCVOID) data, (DWORD) size * n, &nBytesWritten, NULL)) {
      return (int64_t) nBytesWritten;
    } else {
      myerror("WriteFile failed (%u)!", GetLastError());
      return -1;
    }
  }
}

int device_sync(DEVICE *device) {

  assert(device != NULL);
  
  if (device->isDrive) flushBuffer(device);

  if (FlushFileBuffers(device->h)) {
    return 0;
  } else {
    myerror("FlushFileBuffers failed (%u)!", GetLastError());
    return -1;
  }
}

int device_close(DEVICE *device) {

  assert(device != NULL);

  if (device->isDrive) flushBuffer(device);

  if(CloseHandle(device->h)) {
    if (device->isDrive) __mingw_aligned_free(device->buffer);
    free((void*) device);
    return 0;
  } else {
    myerror("CloseHandle failed (%u)!", GetLastError());
    return -1;
  }
}

#endif
