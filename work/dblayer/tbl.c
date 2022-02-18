
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tbl.h"
#include "codec.h"
#include "../pflayer/pf.h"

#define SLOT_COUNT_OFFSET 2
#define checkerr(err) {if (err < 0) {PF_PrintError(); exit(EXIT_FAILURE);}}

int* getPointer(byte* pageBuf, int i){
    // EXTRA FUNCTION: Returns pointer to ith position
    int* p = (int*) pageBuf + i;
    return p;
}

int getFreeSlot(byte* pageBuf){
    // EXTRA FUNCTION: Returns offset value for the free slot
    return *getPointer(pageBuf, 0);
}

int getLen(int slot, byte *pageBuf){
    // Returns slot size of 'slot'th slot
    if (slot == 0){
        // first slot at the bottom of the page
        int size = PF_PAGE_SIZE - *getPointer(pageBuf, slot + SLOT_COUNT_OFFSET);
    }
    else{
        int off_prev = *getPointer(pageBuf, slot + SLOT_COUNT_OFFSET - 1);
        int off_cur = *getPointer(pageBuf, slot + SLOT_COUNT_OFFSET);
        int size = off_prev - off_cur; // off_prev > off_cur
    }
    return size;
}

int getNumSlots(byte *pageBuf){
    int nslots = *getPointer(pageBuf, 1);
    return nslots;
}

void setNumSlots(byte *pageBuf, int nslots){
    *getPointer(pageBuf, 1) = nslots;
}

int getNthSlotOffset(int slot, char* pageBuf){
    int offset = *getPointer(pageBuf, SLOT_COUNT_OFFSET + slot);
    return offset;
}


/**
   Opens a paged file, creating one if it doesn't exist, and optionally
   overwriting it.
   Returns 0 on success and a negative error code otherwise.
   If successful, it returns an initialized Table*.
 */
int
Table_Open(char *dbname, Schema *schema, bool overwrite, Table **ptable)
{

    int status, fd;

    // Initialize PF, create PF file,
    PF_Init();

    if (overwrite){
        status = PF_DestroyFile(dbname); 
    }

    fd = PF_OpenFile(dbname);
    if (fd < 0){
        status = PF_CreateFile(dbname);
        if (status != PFE_OK){
            printf("Table_Open: Error while creating the file\n");
            return status;
        }
    }

    // allocate Table structure, initialize and return via ptable

    *ptable = malloc(sizeof(struct Table));
    if (*ptable == NULL){
        printf("Table_Open: Malloc error while table init\n");
        return -1;
    }
    *ptable->schema = malloc(sizeof(struct Schema)); 
    if (*ptable->schema == NULL){
        printf("Table_Open: Malloc error while schema init\n");
        return -1;
    }
    memcpy(*ptable->schema, schema, sizeof(*ptable->schema));
    *ptable->numPages = 1;
    *ptable->name = strdup(dbname);

    return 0;
}

void
Table_Close(Table *tbl) {
    UNIMPLEMENTED;
    // Unfix any dirty pages, close file.
}


int
Table_Insert(Table *tbl, byte *record, int len, RecId *rid) {
    // Allocate a fresh page if len is not enough for remaining space
    // Get the next free slot on page, and copy record in the free
    // space
    // Update slot and free space index information on top of page.
}

#define checkerr(err) {if (err < 0) {PF_PrintError(); exit(EXIT_FAILURE);}}

/*
  Given an rid, fill in the record (but at most maxlen bytes).
  Returns the number of bytes copied.
 */
int
Table_Get(Table *tbl, RecId rid, byte *record, int maxlen) {
    int slot = rid & 0xFFFF;
    int pageNum = rid >> 16;

    UNIMPLEMENTED;
    // PF_GetThisPage(pageNum)
    // In the page get the slot offset of the record, and
    // memcpy bytes into the record supplied.
    // Unfix the page
    return len; // return size of record
}

void
Table_Scan(Table *tbl, void *callbackObj, ReadFunc callbackfn) {

    UNIMPLEMENTED;

    // For each page obtained using PF_GetFirstPage and PF_GetNextPage
    //    for each record in that page,
    //          callbackfn(callbackObj, rid, record, recordLen)
}


