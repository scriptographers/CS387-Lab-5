#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  printf("RID: %i: %i\n", rid, len);
  printf("Record: %s\n", row);
  Schema *sch = (Schema *)callbackObj;
  byte *cursor = row;

  char str[len];
  char *res = "";

  int n = sch->numColumns;
  for (int i = 0; i < n; ++i) {
    // printf("%s: %s\n", sch->columns[i]->name, fields[i]);
    switch (sch->columns[i]->type) {
    case VARCHAR: {
      printf("Remaining len: %i\n", len);
      short size1 = DecodeShort(cursor);
      printf("Size of string: %i\n", size1);
      int size = DecodeCString(cursor, str, len);
      cursor += size;
      len -= size;
      printf("%i: %s\n", size, str);
      fflush(stdout);
      break;
    }
    case INT: {
      int val = DecodeInt(cursor);
      cursor += 2;
      len -= 2;
      break;
    }
    case LONG: {
      long long val = DecodeLong(cursor);
      cursor += 4;
      len -= 4;
      break;
    }
    default:
      printf("Unknown type %d\n", sch->columns[i]->type);
      break;
    }
  }
  printf("Output: %i: %s\n", rid, res);
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

    printf("RID: %i\n", recId);
    printf("Record: %s\n", record);
    printf("Status: %i\n", status);
    // printRow(schema, recId, record, status);
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
