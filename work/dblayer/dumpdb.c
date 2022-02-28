#include <stdio.h>
#include <stdlib.h>

#include "../amlayer/am.h"
#include "../pflayer/pf.h"
#include "codec.h"
#include "tbl.h"
#include "util.h"

#define checkerr(err)                                                          \
  {                                                                            \
    if (err < 0) {                                                             \
      PF_PrintError();                                                         \
      exit(1);                                                                 \
    }                                                                          \
  }

#define MAX_PAGE_SIZE 4000

#define DB_NAME "data.db"
#define INDEX_NAME "data.db.2"

extern void tperror(int, char *);

void printRow(void *callbackObj, RecId rid, byte *row, int len) {
  Schema *schema = (Schema *)callbackObj;
  byte *cursor = row;

  printf("%i: %s", rid, row);
}

void index_scan(Table *tbl, Schema *schema, int indexFD, int op, char *value) {
  printf("%i: %s\n", op, value);
  int scanD = AM_OpenIndexScan(indexFD, 'i', 4, op, value);
  tperror(scanD, "DumpDB: error while opening index scan\n");

  char record[MAX_PAGE_SIZE];
  while (true) {
    int recId = AM_FindNextEntry(scanD);
    if (recId == AME_EOF)
      break;
    tperror(recId, "DumpDB: error while finding next entry\n");

    int status = Table_Get(tbl, recId, record, sizeof(record));
    tperror(status, "DumpDB: error while getting record\n");

    printRow(schema, recId, record, sizeof(record));
  }
  int err = AM_CloseIndexScan(scanD);
  tperror(err, "DumpDB: error while closing index scan\n");
}

int main(int argc, char **argv) {

  if (argc < 2) {
    printf("Wrong Usage\n");
    exit(1);
  }

  int status;

  char *schemaTxt = "Country:varchar,Capital:varchar,Population:int";
  Schema *sch = parseSchema(schemaTxt);
  Table *tbl;

  status = Table_Open(DB_NAME, sch, false, &tbl);
  tperror(status, "DumpDB: error while opening table\n");

  if (*(argv[1]) == 's') {
    // Scan the table
    Table_Scan(tbl, sch, printRow);

  } else if (*(argv[1]) == 'i') {
    // Index scan
    int indexFD = PF_OpenFile(INDEX_NAME);
    tperror(status, "DumpDB: error while opening index file\n");

    if (argc == 4) {
      // Parse arguments
      if (strcmp(argv[2], "EQUAL") == 0) {
        index_scan(tbl, sch, indexFD, EQUAL, argv[3]);
      } else if (strcmp(argv[2], "NOT_EQUAL") == 0) {
        index_scan(tbl, sch, indexFD, NOT_EQUAL, argv[3]);
      } else if (strcmp(argv[2], "LESS_THAN") == 0) {
        index_scan(tbl, sch, indexFD, LESS_THAN, argv[3]);
      } else if (strcmp(argv[2], "LESS_THAN_EQUAL") == 0) {
        index_scan(tbl, sch, indexFD, LESS_THAN_EQUAL, argv[3]);
      } else if (strcmp(argv[2], "GREATER_THAN") == 0) {
        index_scan(tbl, sch, indexFD, GREATER_THAN, argv[3]);
      } else if (strcmp(argv[2], "GREATER_THAN_EQUAL") == 0) {
        index_scan(tbl, sch, indexFD, GREATER_THAN_EQUAL, argv[3]);
      } else {
        printf("Wrong Operation\n");
        exit(1);
      }

    } else {
      // Complete index scan
      index_scan(tbl, sch, indexFD, LESS_THAN_EQUAL, "100000");
      index_scan(tbl, sch, indexFD, GREATER_THAN, "100000");
    }

    status = PF_CloseFile(indexFD);
    tperror(status, "DumpDB: error while closing index file\n");
  }

  Table_Close(tbl);

  return 0;
}
