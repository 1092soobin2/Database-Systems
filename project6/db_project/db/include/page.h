#ifndef __DB_PAGE_H__
#define __DB_PAGE_H__

#include <stdint.h>

#define INITIAL_DB_FILE_SIZE 10485760   // 10MiB
#define PAGE_SIZE 4096          // 4KiB
#define PAGE_OFFSET(X) (X * 4096)
#define LOG_FILE_ID (-2)

#define PAGE_HEADER_SIZE 128
#define SLOT_SIZE 16
#define INITIAL_FREE_SPACE (PAGE_SIZE - PAGE_HEADER_SIZE)       // 3968
#define VALUE_OFFSET(X) (X - PAGE_HEADER_SIZE)

#define OP_SUCCESS 0
#define OP_FAILURE -1


typedef uint64_t pagenum_t;
typedef int64_t table_id_t;

namespace page {
typedef int64_t key_t;
}

// This structure that the upper layer component will use
// which means the disk space manager reads 4096 bytes from a database file
// and puts it into the buffer.
struct page_t {
    // in-memory page structure
    // Disk space manager never opens allocated pages.
    char buffer[PAGE_SIZE];
};
typedef struct page_t page_t;

// Dsik space management layer
typedef struct {
    union {                         // [0-7]
        // points to the first free page
        // 0, if there is no free page list
        pagenum_t first_free_pagenum;           //  for header page
        // points the next free page
        // 0, if end of the free page list
        pagenum_t next_free_pagenum;            //  for free page
    };
    uint64_t number_of_pages;       // [8-15] for header page (size of db file)
    char reserved[4080];            // [16-4095]
} file_manager_page_t;

// Index management layer
#define THRESHOLD 2500
#define ORDER 249

typedef struct header_page_t {
    pagenum_t first_free_pagenum;   // [0-7]
    uint64_t number_of_pages;       // [8-15] 
    pagenum_t root_pagenum;         // [16-23]
    int64_t LSN;                    // [24-31]
    char reserved[4064];            // [32-4095]
} header_page_t;

typedef struct free_page_t {
    pagenum_t next_free_pagenum;    // [0-7]
    char reserved[4088];            // [8-4095]
} free_page_t;

// header foramt 128B
typedef struct page_header_t {
    pagenum_t parent_pagenum;       // [0-7]
    int is_leaf;                    // [8-11]
    int number_of_keys;             // [12-15]
    char reserved[8];               // [16-23]
    int64_t LSN;                    // [24-31]
    char reserved2[80];              // [32-111]
    uint64_t amount_of_free_space;  // [112-119]    for leaf page
    union {                         // [120-127]
        pagenum_t right_sibling_pagenum;        //  for leaf page
        pagenum_t branch_first_pagenum;         //  for internal page
    };
} page_header_t;

// slot format 16B
#pragma pack(push, 1)
typedef struct slot_t {
    int64_t     key;                   // [0-7]
    uint16_t    size;                  // [8-9]
    uint16_t    offset;                // [10-11]
    int         trx_id;                // [12-15]
} slot_t;
#pragma pack(pop)

// branch format 16B
typedef struct branch_t {
    int64_t key;                    // [0-7]
    pagenum_t pagenum;              // [8-15]
} branch_t;

typedef struct node_page_t {
    page_header_t header;           // [0-127]
    union {                         // [128-4095]
        slot_t slots[64];
        char values[INITIAL_FREE_SPACE];
        branch_t branchs[ORDER - 1];
    };
} node_page_t;

#endif