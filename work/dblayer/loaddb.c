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

#define MAX_PAGE_SIZE 4000

#define DB_NAME "data.db"
#define INDEX_NAME "data.db.2"
#define CSV_NAME "data.csv"

/*
  EXTRA FUNCTION
  prints error
*/
void lerror(int status, char *s) {
  if (status < 0) {
    printf("%s\n", s);
    PF_PrintError();
    exit(EXIT_FAILURE);
  }
}

/*
  Takes a schema, and an array of strings (fields), and uses the functionality
  in codec.c to convert strings into compact binary representations
*/
int encode(Schema *sch, char **fields, byte *record, int spaceLeft) {
  int n = sch->numColumns;
  int len = 0;

  // For every column in schema
  for (int i = 0; i < n; ++i) {
    switch (sch->columns[i]->type) {

    case VARCHAR: {
      // encode field value into 'size' bytes from record[len]
      int size = EncodeCString(fields[i], record + len, spaceLeft);
      len += size;
      spaceLeft -= size;
      break;
    }

    case INT: {
      // encode field value into 'size' (4) bytes from record[len]
      int size = EncodeInt(atoi(fields[i]), record + len);
      len += size;
      spaceLeft -= size;
      break;
    }

    case LONG: {
      // encode field value into 'size' (8) bytes from record[len]
      int size = EncodeLong(atoll(fields[i]), record + len);
      len += size;
      spaceLeft -= size;
      break;
    }

    default:
      break;
    }
  }

  // If no space is left
  if (spaceLeft <= 0) {
    printf("Not enough space left\n");
    return -1;
  }

  // Return length of the record
  return len;
}

Schema *loadCSV() {
  int status;

  // Open csv file
  FILE *fp = fopen(CSV_NAME, "r");
  if (!fp) {
    perror("data.csv could not be opened");
    exit(EXIT_FAILURE);
  }

  // Get first line
  char buf[MAX_LINE_LEN];
  char *line = fgets(buf, MAX_LINE_LEN, fp);
  if (line == NULL) {
    fprintf(stderr, "Unable to read data.csv\n");
    exit(EXIT_FAILURE);
  }

  // Parse Schema
  Schema *sch = parseSchema(line);
  Table *tbl;

  // Open main db file
  status = Table_Open(DB_NAME, sch, true, &tbl);
  lerror(status, "LoadDB: error while opening table\n");

  // Create secondary index
  status = AM_DestroyIndex(DB_NAME, 2);
  status = AM_CreateIndex(DB_NAME, 2, 'i', 4);
  lerror(status, "LoadDB: error while creating index\n");

  // Open index file FD
  int indexFD = PF_OpenFile(INDEX_NAME);
  lerror(status, "LoadDB: error while opening index file\n");

  char *tokens[MAX_TOKENS];
  char record[MAX_PAGE_SIZE];

  while ((line = fgets(buf, MAX_LINE_LEN, fp)) != NULL) {

    // Split every line into tokens
    int n = split(line, ",", tokens);
    assert(n == sch->numColumns);

    // Encode the tokens into record of 'len' bytes
    int len = encode(sch, tokens, record, sizeof(record)); // in bytes

    // Insert the record into the table with 'rid' as record ID
    RecId rid;
    status = Table_Insert(tbl, record, len, &rid);
    lerror(status, "LoadDB: error while inserting into table\n");

    printf("RID: %i | Length (bytes): %d | Data: %s\n", rid, len, line);
    fflush(stdout);

    // Use population as secondary index to insert into index file
    int population = atoi(tokens[2]);
    status = AM_InsertEntry(indexFD, 'i', 4, (char *)&population, rid);
    lerror(status, "LoadDB: error while inserting into index file\n");
  }

  // Close index file FD
  status = PF_CloseFile(indexFD);
  lerror(status, "LoadDB: error while closing index file\n");

  // Close main db file
  Table_Close(tbl);

  // Close csv file
  fclose(fp);

  return sch;
}

int main() {

  loadCSV();

  return 0;
}
