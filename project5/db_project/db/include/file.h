#ifndef __DB_FILE_H_
#define __DB_FILE_H_

#include <stdint.h>
#include <vector>

#include "page.h"

#define MAX_TABLES 20


// API to be exported to upper layer.
// Open existing database file or create one if not existed.
// Return new fd.
// If failed, return -1.
table_id_t file_open_table_file(const char* pathname);

// Allocate an on-disk page from the free page list.
pagenum_t file_alloc_page(table_id_t table_id);

// Free an on-disk page to the free page list.
void file_free_page(table_id_t table_id, pagenum_t pagenum);

// Read an on-disk page into the in-memory page structure(dest).
void file_read_page(table_id_t table_id, pagenum_t pagenum, page_t* dest);

// Write an in-memory page(src) to the on-disk page.
void file_write_page(table_id_t table_id, pagenum_t pagenum, const page_t* src);

// Stop referencing the database file
// Close all of the opened file
void file_close_table_files(void);

bool file_is_valid_table_id(table_id_t tid);

void file_init(void);

inline void wrapper_read (int fd, pagenum_t pagenum, file_manager_page_t*& dest);
inline void wrapper_write (int fd, pagenum_t pagenum, file_manager_page_t*& src);


// test API
pagenum_t file_test_get_ffpn (table_id_t fd);
uint64_t file_test_get_np (table_id_t fd);
pagenum_t file_test_get_nfn (table_id_t fd, pagenum_t ffn);
const char* get_pathname (table_id_t table_id);

#endif  // DB_FILE_H_
