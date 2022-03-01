#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../amlayer/am.h"
#include "../pflayer/pf.h"
#include "codec.h"
#include "tbl.h"
#include "util.h"

#define MAX_PAGE_SIZE 4000

#define DB_NAME "data.db"
#define INDEX_NAME "data.db.2"

/*
  EXTRA FUNCTION
  prints error
*/
void derror(int status, char *s) {
  if (status < 0) {
    printf("%s\n", s);
    PF_PrintError();
  }
  return;
}

void printRow(void *callbackObj, RecId rid, byte *row, int len) {
  // The call back object is schema, row is the record
  Schema *sch = (Schema *)callbackObj;
  byte *cursor = row;
  int n = sch->numColumns;

  // Initialize the result
  char res[MAX_PAGE_SIZE];
  int res_len = 0;

  // For every column in schema
  for (int i = 0; i < n; ++i) {
    switch (sch->columns[i]->type) {

    case VARCHAR: {
      // Decode 2 bytes to get length of string as 'size'
      // Followed by decoding 'size' bytes to get the string
      char str[len];
      int size = DecodeCString(cursor, str, len);

      // Move the cursor by 'size' + 2 bytes
      cursor += (size + 2);
      len -= (size + 2);

      // Append the string to the result
      memcpy(res + res_len, str, size);
      res_len += size;

      // Append comma to the result
      res[res_len] = ',';
      res_len += 1;
      break;
    }

    case INT: {
      // Decode 4 bytes to get integer value
      int val = DecodeInt(cursor);

      // Move cursor by 4 bytes
      cursor += 4;
      len -= 4;

      // Append the integer to the result
      res_len += snprintf(res + res_len, MAX_PAGE_SIZE, "%i", val);

      // Append comma to the result
      res[res_len] = ',';
      res_len += 1;
      break;
    }

    case LONG: {
      // Decode 8 bytes to get integer value
      long long val = DecodeLong(cursor);

      // Move cursor by 8 bytes
      cursor += 8;
      len -= 8;

      // Append the integer to the result
      res_len += snprintf(res + res_len, MAX_PAGE_SIZE, "%lld", val);

      // Append comma to the result
      res[res_len] = ',';
      res_len += 1;
      break;
    }

    default:
      printf("Unknown type %d\n", sch->columns[i]->type);
      break;
    }
  }

  // Replace the last comma with \0
  res[res_len - 1] = '\0';

  // Print the result
  printf("%s\n", res);

  return;
}

void index_scan(Table *tbl, Schema *schema, int indexFD, int op, int value) {
  // Open Index
  int scanD = AM_OpenIndexScan(indexFD, 'i', 4, op, (char *)&value);
  derror(scanD, "DumpDB: error while opening index scan\n");

  char record[MAX_PAGE_SIZE];
  while (true) {
    // Find next entry using index and break at AME_EOF
    int recId = AM_FindNextEntry(scanD);
    if (recId == AME_EOF)
      break;
    derror(recId, "DumpDB: error while finding next entry\n");

    // Get record corresponding to the record ID
    int status = Table_Get(tbl, recId, record, sizeof(record));
    derror(status, "DumpDB: error while getting record\n");

    // Print record
    printRow(schema, recId, record, status);
  }

  // Close Index
  int err = AM_CloseIndexScan(scanD);
  derror(err, "DumpDB: error while closing index scan\n");

  return;
}

int main(int argc, char **argv) {

  // Wrong usage
  if (argc < 2) {
    printf("Wrong Usage\n");
    exit(1);
  }

  int status;

  // Parse schema
  char *schemaTxt = "Country:varchar,Capital:varchar,Population:int";
  Schema *sch = parseSchema(schemaTxt);
  Table *tbl;

  // Open main db file
  status = Table_Open(DB_NAME, sch, false, &tbl);
  derror(status, "DumpDB: error while opening table\n");

  if (*(argv[1]) == 's') {
    // SEquential scan
    Table_Scan(tbl, sch, printRow);

  } else if (*(argv[1]) == 'i') {
    // Index scan

    // Open index file FD
    int indexFD = PF_OpenFile(INDEX_NAME);
    derror(status, "DumpDB: error while opening index file\n");

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

    // Close index file FD
    status = PF_CloseFile(indexFD);
    derror(status, "DumpDB: error while closing index file\n");
  }

  // Close main db file
  Table_Close(tbl);

  return 0;
}
