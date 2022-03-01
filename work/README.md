# CS 387 Lab 5 - ToyDB

## Getting started

Makefile is presented in the base of the directory.

Use `make` command to compile pflayer, amlayer and dblayer.  
![`make`](./screenshots/make.png)

Use `make clean` command to revert to pre-compile stage.  
![`make clean`](./screenshots/make_clean.png)

## Using ToyDB

Change the directory to dblayer using `cd dblayer/`.

### Load

Use `./loaddb` command to load `data.csv` and generate the database `data.db` and secondary index table `data.db.2`.  
Refer to documentation for more details.  
![`loaddb`](./screenshots/loaddb.png)  
Complete output is present in `../output/loaddb.txt`

### Sequential Scan

Use `./dumpdb s` command to sequentially scan through the database `data.db`.  
Refer to documentation for more details.  
![`dumpdb s`](./screenshots/dumpdb_s.png)  
Complete output is present in `../output/dumpdb_s.txt`

### Index Scan

Use `./dumpdb s` command to index scan through the database `data.db` using index stored in `data.db.2`.  
Refer to documentation for more details.  
![`dumpdb i`](./screenshots/dumpdb_i.png)  
Complete output is present in `../output/dumpdb_i.txt`

### Querying using Index Scan

Use `./dumpdb s [condition] [value]` command to query records satisfying the condition using index scan.  
Refer to documentation for more details.  
![`dumpdb i LESS_THAN 100000`](./screenshots/dumpdb_i_less_than_100000.png)  
Complete output for querying population less than 100000 is present in `../output/dumpdb_i_less_than_100000.txt`.

## Documentation

- `tbl.c` and `tbl.h` are based on the slotted-page structure as described in [1], [2].  
  A file/table is divided into multiple fixed-size pages, which are in turn divided into variable-length slots. Top of the page contains the header, which stores a pointer/offset to the free space, the number of slots, and the offsets for each slot. Records are stored starting from the bottom of the page. New pages are allocated on demand.  
  Refer to comments in `tbl.c` for detailed description.

- `loaddb.c` reads `data.csv` and creates database `data.db` and stores secondary index in `data.db.2`.  
  Each row is encoded into a record which is then inserted into the table and also stored in secondary index using appropriate index value.  
  Refer to comments in `loaddb.c` for detailed description.

- `dumpdb.c` scans through the database `data.db` either sequentially or using index stored in `data.db.2`.  
  Sequential scan goes through the database one record after another, whereas index scan uses the B-Tree structure to fetch records ordered by the index.  
  Refer to comments in `dumpdb.c` for detailed description.

## References

- [1] https://www.javatpoint.com/file-organization-storage
- [2] https://www.db-book.com/slides-dir/PDF-dir/ch13.pdf

## Team

| Name          | Roll Number | Contributions                                       |
| ------------- | ----------- | --------------------------------------------------- |
| Devansh Jain  | 190100044   | loaddb.c, dumpdb.c, testing & debugging             |
| Harshit Varma | 190100055   | tbl.h, tbl.c, helper functions, testing & debugging |
