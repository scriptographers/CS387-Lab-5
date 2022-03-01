# CS 387 Lab 5 - ToyDB

## Documentation
- `tbl.c` and `tbl.h` are based on the slotted-page structure as described in [1], [2].
  A file/table is divided into multiple fixed-size pages, which are in turn divided into variable-length slots. Top of the page contains the header, which stores a pointer/offset to the free space, the number of slots, and the offsets for each slot. Records are stored starting from the bottom of the page. New pages are allocated on demand. 

## References
- [1] https://www.javatpoint.com/file-organization-storage
- [2] https://www.db-book.com/slides-dir/PDF-dir/ch13.pdf

## Team

| Name          | Roll Number | Contributions |
| ------------- | ----------- | ------------- |
| Devansh Jain  | 190100044   | loaddb.c, dumpdb.c, testing/debugging |
| Harshit Varma | 190100055   | tbl.h, tbl.c, helper functions, testing/debugging |
