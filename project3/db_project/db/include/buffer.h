#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <stdint.h>

#include "page.h"

#define INVALID_BUFNUM -1
typedef struct buffer_cntl_block_t {
    table_id_t table_id;
    pagenum_t pagenum;

    bool is_dirty;
    int number_of_pins;

    // for maintaining data structure
    union {
        int free_next_bufnum;       // If there is no next free buffer, INVALID_BUFNUM.
        int LRU_next_bufnum;        
    };
    int LRU_prev_bufnum;

} buffer_cntl_block_t;

typedef struct {
    int first_free_bufnum;      // free buffer list
    int LRU_head_bufnum;        // Most recently used
    int LRU_tail_bufnum;        // Least recently used

} buffer_info_t ;



// APIs

pagenum_t buffer_alloc_page(table_id_t table_id);

void buffer_free_page(table_id_t table_id, pagenum_t pagenum);

// If the page exists in buffer, return it.
// Otherwise, call disk manager (file_read_page).
int get_new_bufnum(table_id_t table_id, pagenum_t pagenum);
// Store page and bufnum into the parameter, if requeested buffer is valid.
int buffer_request_page(table_id_t table_id, pagenum_t pagenum, header_page_t*& page, int* bufnum);
int buffer_request_page(table_id_t table_id, pagenum_t pagenum, node_page_t*& page, int* bufnum);

// Remain the page dirty in buffer.
// If header, immediately flush
void buffer_release_page(int bufnum, bool is_dirty);

void buffer_init(int num_buf);
void buffer_close_table_files(void);

// Utility


#endif