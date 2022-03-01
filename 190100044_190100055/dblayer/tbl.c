
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../pflayer/pf.h"
#include "codec.h"
#include "tbl.h"

#define SLOT_COUNT_OFFSET 2

#define checkerr(err)                                                          \
  {                                                                            \
    if (err < 0) {                                                             \
      PF_PrintError();                                                         \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  }

/*
  EXTRA FUNCTION
  prints error
*/
void tperror(int status, char *s) {
  if (status < 0) {
    printf("%s\n", s);
    PF_PrintError();
  }
  return;
}

/*
  EXTRA FUNCTION
  traverses a char array in steps of size 4 bytes, useful for storing int data
  in a char* array
*/
int *getPointer(byte *pageBuf, int i) {
  int *p = ((int *)pageBuf) + i;
  return p;
}

/*
  EXTRA FUNCTION
  Returns offset value (in bytes) for the free slot
*/
int getFreeSlot(byte *pageBuf) { return *getPointer(pageBuf, 0); }

/*
  EXTRA FUNCTION
  sets offset value (in bytes) for free slot
*/
void setFreeOffset(byte *pageBuf, int offset) {
  *getPointer(pageBuf, 0) = offset;
  return;
}

/*
  Returns slot size (in bytes) of 'slot'th slot
*/
int getLen(int slot, byte *pageBuf) {
  int size;

  if (slot == 0) {
    // first slot at the bottom of the page
    size = PF_PAGE_SIZE - *getPointer(pageBuf, SLOT_COUNT_OFFSET);
  } else {
    int off_prev = *getPointer(pageBuf, slot + SLOT_COUNT_OFFSET - 1);
    int off_cur = *getPointer(pageBuf, slot + SLOT_COUNT_OFFSET);
    size = off_prev - off_cur; // off_prev > off_cur
  }

  return size;
}

/*
  Return number of slots
*/
int getNumSlots(byte *pageBuf) {
  int nslots = *getPointer(pageBuf, 1);
  return nslots;
}

/*
  Set number of slots
*/
void setNumSlots(byte *pageBuf, int nslots) {
  *getPointer(pageBuf, 1) = nslots;
  return;
}

/*
  Returns offset value of the nth slot in bytes
*/
int getNthSlotOffset(int slot, char *pageBuf) {
  int offset = *getPointer(pageBuf, SLOT_COUNT_OFFSET + slot);
  return offset;
}

/*
  EXTRA FUNCTION
  Returns total free space value (in bytes) in the page
*/
int remainingSpace(byte *pageBuf) {
  int nslots = getNumSlots(pageBuf);
  int free_start = 4 * (SLOT_COUNT_OFFSET + nslots);
  int free_end = getFreeSlot(pageBuf);
  int rem = free_end - free_start;

  if (rem < 0) { // just a sanity check
    printf("error, remainingSpace = %d\n", rem);
    exit(EXIT_FAILURE);
  }

  return rem;
}

/*
  Opens a paged file, creating one if it doesn't exist, and optionally
  overwriting it.
  Returns 0 on success and a negative error code otherwise.
  If successful, it returns an initialized Table*.
*/
int Table_Open(char *dbname, Schema *schema, bool overwrite, Table **ptable) {
  int status, fd;
  bool file_exists = true;

  // Initialize PF, create PF file
  PF_Init();

  // Check existence
  fd = PF_OpenFile(dbname);
  if (fd < 0) {
    file_exists = false;
  } else {
    // close now to avoid destroy errors
    status = PF_CloseFile(fd);
    tperror(status, "Table_Open: error while closing file after checking");
    if (status < 0) {
      return status;
    }
  }

  // Destroy file if doesn't exist
  if (overwrite && file_exists) {
    status = PF_DestroyFile(dbname);
    tperror(status, "Table_Open: error while destroying file");
    if (status < 0) {
      return status;
    }
  }

  // Create file if doesn't exist or destroyed/overwrite = True
  if (!file_exists || overwrite) {
    status = PF_CreateFile(dbname);
    tperror(status, "Table_Open: error while creating file");
    if (status < 0) {
      return status;
    }
  }

  // File guaranteed to exist now
  fd = PF_OpenFile(dbname);
  tperror(fd, "Table_Open: error while opening file");
  if (fd < 0) {
    return fd;
  }

  // Allocate table space
  *ptable = malloc(sizeof(Table));
  if (*ptable == NULL) {
    printf("Table_Open: Malloc error while table init\n");
    return -20;
  }

  // Allocate schema space
  (*ptable)->schema = malloc(sizeof(Schema));
  if ((*ptable)->schema == NULL) {
    printf("Table_Open: Malloc error while schema init\n");
    return -20;
  }

  // Copy the given schemas and assign metadata
  memcpy((*ptable)->schema, schema, sizeof((*ptable)->schema));
  (*ptable)->numPages = 0;
  (*ptable)->name = strdup(dbname);

  // Close PF file
  status = PF_CloseFile(fd);
  tperror(status, "Table_Open: error while closing file");
  if (status < 0) {
    return status;
  }

  return 0;
}

void Table_Close(Table *tbl) {
  int status, fd;

  // Open the PF file
  fd = PF_OpenFile(tbl->name);
  tperror(fd, "Table_Close: error while opening file");
  if (fd < 0) {
    return;
  }

  // Unfix any dirty pages
  for (int i = 0; i < tbl->numPages; i++) {
    // always set dirty = true
    // a better method would be to track dirty pages in table struct
    status = PF_UnfixPage(fd, i, true);
    if (!(status == PFE_OK) && !(status == PFE_PAGEUNFIXED) &&
        !(status == PFE_PAGENOTINBUF)) {
      printf("err: %d\n", status);
      tperror(status, "Table_Close: error while unfixing page");
      if (status < 0) {
        return;
      }
    }
  }

  // Close PF file
  status = PF_CloseFile(fd);
  tperror(status, "Table_Close: error while closing file");
  if (status < 0) {
    return;
  }
}

int Table_Insert(Table *tbl, byte *record, int len, RecId *rid) {
  int status, fd;
  int pagenum;   // page number
  byte *pagebuf; // pointer to the buffer

  // len is in bytes
  // Check if len exceeds page_size
  if (len > PF_PAGE_SIZE) {
    printf("Table_Insert: length of record exceed page size\n");
    return -21;
  }

  // Open the PF file
  fd = PF_OpenFile(tbl->name);
  tperror(fd, "Table_Insert: error while opening file");
  if (fd < 0) {
    return fd;
  }

  // Get the last page
  int rem;
  int num_pages = tbl->numPages;
  if (num_pages > 0) {
    // Get last page
    pagenum = num_pages - 1;
    status = PF_GetThisPage(fd, pagenum, &pagebuf);
    tperror(status, "Table_Insert: error while opening page");
    if (status < 0) {
      return status;
    }
    // Get remaining space on page
    rem = remainingSpace(pagebuf);
  } else
    rem = 0; // no pages allocated yet to file

  // Allocate a fresh page if len is not enough for remaining space
  if (rem < len) {
    // Unfix previous page if fixed
    if (num_pages > 0) {
      status = PF_UnfixPage(fd, pagenum, false);
      tperror(status, "Table_Insert: error while unfixing space constrained page");
      if (status < 0) {
        return status;
      }
    }

    // Alloc new page
    status = PF_AllocPage(fd, &pagenum, &pagebuf);
    tperror(status, "Table_Insert: error while allocating page");
    if (status < 0) {
      return status;
    }

    // Update metdata
    tbl->numPages++;
    setNumSlots(pagebuf, 0);
    setFreeOffset(pagebuf, PF_PAGE_SIZE);
  }

  // Get the next free slot on page
  int nslots = getNumSlots(pagebuf);
  int free_offset = getFreeSlot(pagebuf); // in bytes
  int slot_offset = free_offset - len;    // in bytes

  // Copy
  memcpy(pagebuf + slot_offset, record, len);

  // Update number of slots
  setNumSlots(pagebuf, nslots + 1);
  // Set new free offset
  setFreeOffset(pagebuf, slot_offset);
  // Store new slot offset
  *getPointer(pagebuf, SLOT_COUNT_OFFSET + nslots) = slot_offset;

  // Compute RID
  int page_bits = pagenum << 16;
  (*rid) = page_bits + nslots;

  // Unfix the page
  status = PF_UnfixPage(fd, pagenum, true);
  tperror(status, "Table_Insert: error while unfixing page");
  if (status < 0) {
    return status;
  }

  // Close PF file
  status = PF_CloseFile(fd);
  tperror(status, "Table_Insert: error while closing file");
  if (status < 0) {
    return status;
  }

  return 0;
}

/*
  Given an rid, fill in the record (but at most maxlen bytes).
  Returns the number of bytes copied.
*/
int Table_Get(Table *tbl, RecId rid, byte *record, int maxlen) {
  int status, fd;
  byte *pagebuf; // pointer to the buffer
  // maxlen is in bytes

  int slot = rid & 0xFFFF;
  int pageNum = rid >> 16;

  // Open the PF file
  fd = PF_OpenFile(tbl->name);
  tperror(fd, "Table_Get: error while opening file");
  if (fd < 0) {
    return fd;
  }

  // Get the page where the record is stored
  status = PF_GetThisPage(fd, pageNum, &pagebuf);
  tperror(status, "Table_Get: error while opening page");
  if (status < 0) {
    return status;
  }

  // In the page, get the slot offset of the record
  int offset = getNthSlotOffset(slot, pagebuf); // in bytes

  // Get length of the record
  int rlen = getLen(slot, pagebuf); // in bytes
  // Compute the length to be copied
  int clen = (rlen > maxlen) ? maxlen : rlen;

  // memcpy clen bytes into the record supplied starting from the specified
  // offset
  memcpy(record, pagebuf + offset, clen);

  // Unfix the page
  status = PF_UnfixPage(fd, pageNum, true);
  tperror(status, "Table_Get: error while unfixing page");
  if (status < 0) {
    return status;
  }

  // Close PF file
  status = PF_CloseFile(fd);
  tperror(status, "Table_Get: error while closing file");
  if (status < 0) {
    return status;
  }

  return clen; // return number of bytes copied
}

void Table_Scan(Table *tbl, void *callbackObj, ReadFunc callbackfn) {
  int status, fd, pagenum = -1;
  byte *pagebuf; // pointer to the buffer

  // Open the PF file
  fd = PF_OpenFile(tbl->name);
  tperror(fd, "Table_Scan: error while opening file");
  if (fd < 0) {
    return;
  }

  // Scan
  int rid, nslots, slen, offset;
  byte *record = NULL;
  while (true) {

    status = PF_GetNextPage(fd, &pagenum, &pagebuf);

    if (status == PFE_EOF)
      break;
    else
      tperror(status, "Table_Scan: error while fetching next page");

    nslots = getNumSlots(pagebuf);

    for (int s = 0; s < nslots; s++) {
      // Compute rid
      rid = (pagenum << 16) + s;
      // Get slot length and offset
      slen = getLen(s, pagebuf);             // in bytes
      offset = getNthSlotOffset(s, pagebuf); // in bytes
      // Allocate space for record and copy the slot into record
      record = malloc(sizeof(byte) * slen);
      memcpy(record, pagebuf + offset, slen);
      // Callback
      callbackfn(callbackObj, rid, record, slen);
      // Free the allocated memory
      free(record);
    }

    // Unfix the page
    status = PF_UnfixPage(fd, pagenum, false);
    tperror(status, "Table_Scan: error while unfixing page");
    if (status < 0) {
      break;
    }
  }

  // Close PF file
  status = PF_CloseFile(fd);
  tperror(status, "Table_Scan: error while closing file");

  return;
}
