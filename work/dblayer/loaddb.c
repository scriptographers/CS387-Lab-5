#include <assert.h>
#include <ctype.h>
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
#define CSV_NAME "data.csv"

// extern void tperror2(int, char *);

void tperror2(int status, char *s) {
  // EXTRA FUNCTION: prints error
  if (status < 0) {
    printf("%s\n", s);
    PF_PrintError();
    exit(EXIT_FAILURE);
  }
}

/**
 * Takes a schema, and an array of strings (fields), and uses the functionality
 * in codec.c to convert strings into compact binary representations
 */
int encode(Schema *sch, char **fields, byte *record, int spaceLeft) {
  int n = sch->numColumns;
  int len = 0;

  for (int i = 0; i < n; ++i) {
    // printf("%s: %s\n", sch->columns[i]->name, fields[i]);
    switch (sch->columns[i]->type) {
    case VARCHAR: {
      int size = EncodeCString(fields[i], record + len, spaceLeft);
      len += size;
      spaceLeft -= size;
      // printf("Size: %d\n", size);
      // printf("Record: %s\n", record);
      break;
    }
    case INT: {
      int size = EncodeInt(atoi(fields[i]), record + len);
      len += size;
      spaceLeft -= size;
      // printf("Record: %s\n", record);
      break;
    }
    case LONG: {
      int size = EncodeLong(atoll(fields[i]), record + len);
      len += size;
      spaceLeft -= size;
      // printf("Record: %s\n", record);
      break;
    }
    default:
      // printf("Unknown type %d\n", sch->columns[i]->type);
      break;
    }
  }

  if (spaceLeft <= 0) {
    printf("Not enough space left\n");
    return -1;
  }

  return len;
}

Schema *loadCSV() {

  int status;

  // Open csv file, parse schema
  FILE *fp = fopen(CSV_NAME, "r");
  if (!fp) {
    perror("data.csv could not be opened");
    exit(EXIT_FAILURE);
  }

  char buf[MAX_LINE_LEN];
  char *line = fgets(buf, MAX_LINE_LEN, fp);
  if (line == NULL) {
    fprintf(stderr, "Unable to read data.csv\n");
    exit(EXIT_FAILURE);
  }

  // Open main db file
  Schema *sch = parseSchema(line);
  Table *tbl;

  status = Table_Open(DB_NAME, sch, true, &tbl);
  tperror2(status, "LoadDB: error while opening table\n");

  status = AM_DestroyIndex(DB_NAME, 2);
  status = AM_CreateIndex(DB_NAME, 2, 'i', 4);
  tperror2(status, "LoadDB: error while creating index\n");

  int indexFD = PF_OpenFile(INDEX_NAME);
  tperror2(status, "LoadDB: error while opening index file\n");

  char *tokens[MAX_TOKENS];
  char record[MAX_PAGE_SIZE];

  while ((line = fgets(buf, MAX_LINE_LEN, fp)) != NULL) {

    int n = split(line, ",", tokens);
    assert(n == sch->numColumns);
    int len = encode(sch, tokens, record, sizeof(record)); // in bytes

    RecId rid;
    status = Table_Insert(tbl, record, len, &rid);
    tperror2(status, "LoadDB: error while inserting into table\n");

    printf("RID: %i | Length (bytes): %d | Data: %s\n", rid, len, line);
    fflush(stdout);

    int population = atoi(tokens[2]);
    status = AM_InsertEntry(indexFD, 'i', 4, &population, rid);
    tperror2(status, "LoadDB: error while inserting into index file\n");
  }

  status = PF_CloseFile(indexFD);
  tperror2(status, "LoadDB: error while closing index file\n");

  Table_Close(tbl);
  fclose(fp);

  return sch;
}

int main() {

  loadCSV();

  return 0;
}
