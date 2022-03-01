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

  // printf("RID: %i\n", rid);
  // printf("Len: %i\n", len);
  // printf("Record: %s\n", row);

  Schema *sch = (Schema *)callbackObj;
  byte *cursor = row;

  char str[len];
  char res[MAX_PAGE_SIZE];
  int res_len = 0;

  int n = sch->numColumns;
  for (int i = 0; i < n; ++i) {
    switch (sch->columns[i]->type) {
    case VARCHAR: {
      int size = DecodeCString(cursor, str, len);
      cursor += (size + 2);
      len -= (size + 2);

      memcpy(res + res_len, str, size);
      res_len += size;

      res[res_len] = ',';
      res_len += 1;

      break;
    }

    case INT: {
      int val = DecodeInt(cursor);
      cursor += 4;
      len -= 4;

      res_len += snprintf(res + res_len, MAX_PAGE_SIZE, "%i", val);

      res[res_len] = ',';
      res_len += 1;

      break;
    }

    case LONG: {
      long long val = DecodeLong(cursor);
      cursor += 8;
      len -= 8;

      res_len += snprintf(res + res_len, MAX_PAGE_SIZE, "%lld", val);

      res[res_len] = ',';
      res_len += 1;

      break;
    }

    default:
      printf("Unknown type %d\n", sch->columns[i]->type);
      break;
    }
  }

  res[res_len - 1] = '\0';
  printf("%s\n", res);

  return;
}

void index_scan(Table *tbl, Schema *schema, int indexFD, int op, int value) {

  int scanD = AM_OpenIndexScan(indexFD, 'i', 4, op, &value);
  tperror(scanD, "DumpDB: error while opening index scan\n");

  char record[MAX_PAGE_SIZE];
  while (true) {
    int recId = AM_FindNextEntry(scanD);
    if (recId == AME_EOF)
      break;
    tperror(recId, "DumpDB: error while finding next entry\n");

    int status = Table_Get(tbl, recId, record, sizeof(record));
    tperror(status, "DumpDB: error while getting record\n");

    printRow(schema, recId, record, status);
  }

  int err = AM_CloseIndexScan(scanD);
  tperror(err, "DumpDB: error while closing index scan\n");

  return;
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
        index_scan(tbl, sch, indexFD, EQUAL, atoi(argv[3]));
      } else if (strcmp(argv[2], "LESS_THAN") == 0) {
        index_scan(tbl, sch, indexFD, LESS_THAN, atoi(argv[3]));
      } else if (strcmp(argv[2], "LESS_THAN_EQUAL") == 0) {
        index_scan(tbl, sch, indexFD, LESS_THAN_EQUAL, atoi(argv[3]));
      } else if (strcmp(argv[2], "GREATER_THAN") == 0) {
        index_scan(tbl, sch, indexFD, GREATER_THAN, atoi(argv[3]));
      } else if (strcmp(argv[2], "GREATER_THAN_EQUAL") == 0) {
        index_scan(tbl, sch, indexFD, GREATER_THAN_EQUAL, atoi(argv[3]));
      } else {
        printf("Wrong Operation\n");
        exit(1);
      }

    } else {
      // Complete index scan
      index_scan(tbl, sch, indexFD, LESS_THAN_EQUAL, 1000000);
      index_scan(tbl, sch, indexFD, GREATER_THAN, 1000000);
    }

    status = PF_CloseFile(indexFD);
    tperror(status, "DumpDB: error while closing index file\n");
  }

  Table_Close(tbl);

  return 0;
}
