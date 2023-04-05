#include "buffer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "page.h"
#include "file.h"
#include "log.h"

// Data Structures used in buffer management layer.
buffer_info_t buffer_info;
buffer_cntl_block_t *buffer_cntl_blocks;
page_t *frames;
pthread_mutex_t buffer_manager_latch;

extern bool verbose;
extern bool verbose2;

pagenum_t buffer_alloc_page(table_id_t table_id) {
    
    pagenum_t pagenum;
    int bufnum = INVALID_BUFNUM;
    int header_bufnum;
    header_page_t* header_page;

    if (verbose) {
        printf(" |buffer_alloc_page");
    }

    // Allocate a new page number.
    pagenum = file_alloc_page(table_id);
    // Maintain consistency of the header page with disk and buffer.
    for (header_bufnum = buffer_info.LRU_head_bufnum; header_bufnum != INVALID_BUFNUM; header_bufnum = buffer_cntl_blocks[header_bufnum].LRU_next_bufnum) 
        if (table_id == buffer_cntl_blocks[header_bufnum].table_id && 0 == buffer_cntl_blocks[header_bufnum].pagenum) {
            file_read_page(table_id, 0, &frames[header_bufnum]);
            if (verbose) {
                header_page = (header_page_t*)(&frames[header_bufnum]);
                printf(" (header_page(%ld %ld %ld) sync.ed imm.)", header_page->first_free_pagenum, header_page->number_of_pages, header_page->root_pagenum);
            }
            break;
        }

    // Load the new page into the buffer.
    bufnum = get_new_bufnum(table_id, pagenum);

    if (verbose) {
        printf("\tpagenum: %ld, bufnum: %d", pagenum, bufnum);
        printf("|");
    }
    return pagenum;
}

void buffer_free_page(table_id_t table_id, pagenum_t pagenum) {

    int bufnum;
    int header_bufnum;
    header_page_t* header_page;

    pthread_mutex_lock(&buffer_manager_latch);

    if (verbose) {
        printf(" |buffer_free_page p: %ld", pagenum);
    }

    if (verbose2) {
        printf("\n0.buf free");
        getchar();
    }
    // TODO: what if num_pins != 0
    // If the page exists in buffer, free the buffer.
    for (bufnum = buffer_info.LRU_head_bufnum; bufnum != INVALID_BUFNUM; bufnum = buffer_cntl_blocks[bufnum].LRU_next_bufnum)
        if (table_id == buffer_cntl_blocks[bufnum].table_id && pagenum == buffer_cntl_blocks[bufnum].pagenum) {
            if (verbose2) {
                printf("(b: (%d)-%d-(%d), dirty: %d)", buffer_cntl_blocks[bufnum].LRU_prev_bufnum, bufnum, buffer_cntl_blocks[bufnum].LRU_next_bufnum, buffer_cntl_blocks[bufnum].is_dirty);
                getchar();
            }
            // Remove from the LRU list.
            if (bufnum == buffer_info.LRU_tail_bufnum) {
                if (verbose2) {
                    printf("(LRU tail)");
                    getchar();
                }
                buffer_info.LRU_tail_bufnum = buffer_cntl_blocks[bufnum].LRU_prev_bufnum;
                buffer_cntl_blocks[buffer_info.LRU_tail_bufnum].LRU_next_bufnum = INVALID_BUFNUM;
            } else if (bufnum == buffer_info.LRU_head_bufnum) {
                if (verbose2) {
                    printf("(LRU head)");
                    getchar();
                }
                buffer_info.LRU_head_bufnum = buffer_cntl_blocks[bufnum].LRU_next_bufnum;
                buffer_cntl_blocks[buffer_info.LRU_head_bufnum].LRU_prev_bufnum = INVALID_BUFNUM;
            } else {
                buffer_cntl_blocks[buffer_cntl_blocks[bufnum].LRU_prev_bufnum].LRU_next_bufnum = buffer_cntl_blocks[bufnum].LRU_next_bufnum;
                buffer_cntl_blocks[buffer_cntl_blocks[bufnum].LRU_next_bufnum].LRU_prev_bufnum = buffer_cntl_blocks[bufnum].LRU_prev_bufnum;
            }
            // Write the page if dirty.
            if (buffer_cntl_blocks[bufnum].is_dirty == true) {
                file_write_page(buffer_cntl_blocks[bufnum].table_id, buffer_cntl_blocks[bufnum].pagenum, &frames[bufnum]);
            }
            // for tidiness
            memset(&frames[bufnum], 0, PAGE_SIZE);

            // Push the buffer into the free buffer list.
            buffer_cntl_blocks[bufnum].free_next_bufnum = buffer_info.first_free_bufnum;
            buffer_info.first_free_bufnum = bufnum;
            break;
        }

    if (verbose2) {
        printf("\n1. free buf");
        getchar();
    }

    // Free the page.
    file_free_page(table_id, pagenum);

    if (verbose2) {
        printf("\n2. free the page");
        getchar();
    }

    // Maintain consistency of the header page with disk and buffer.
    for (header_bufnum = buffer_info.LRU_head_bufnum; header_bufnum != INVALID_BUFNUM; header_bufnum = buffer_cntl_blocks[header_bufnum].LRU_next_bufnum) 
        if (table_id == buffer_cntl_blocks[header_bufnum].table_id && 0 == buffer_cntl_blocks[header_bufnum].pagenum) {
            file_read_page(table_id, 0, &frames[header_bufnum]);
            if (verbose) {
                header_page = (header_page_t*)(&frames[header_bufnum]);
                printf(" (header_page(%ld %ld %ld) sync.ed imm.)", header_page->first_free_pagenum, header_page->number_of_pages, header_page->root_pagenum);
            }
            break;
        }

    if (verbose2) {
        printf("\n3. sync header");
        getchar();
    }
    if (verbose) {
        printf("|");
    }

    pthread_mutex_unlock(&buffer_manager_latch);
}

int get_new_bufnum(table_id_t table_id, pagenum_t pagenum) {
    int bufnum = INVALID_BUFNUM;
    
    if (verbose) {
        printf(" |get_new_bufnum");
    }
    // Otherwise, call disk manager for read.
    // I. there are some free buffers.
    if (buffer_info.first_free_bufnum != INVALID_BUFNUM) {
        if (verbose) {
            printf(" in free list");
        }
        // Get the first free buffer number.
        bufnum = buffer_info.first_free_bufnum;
        buffer_info.first_free_bufnum = buffer_cntl_blocks[bufnum].free_next_bufnum;
    }
    // II. buffer is full. LRU policy
    else {
        if (verbose) {
            printf(" in LRU list");
        }
        // TODO: The case buffer pool is full and there is no unpinned buffer.
        // 1. return OP_FAILURE
        // 2. 가능 버퍼가 생길 때까지 반복
        // 현재로써는 roll-back algorithm을 구현할 수 없다고 생각하여 2안 선택
        while (bufnum == INVALID_BUFNUM) {
            for (bufnum = buffer_info.LRU_tail_bufnum; bufnum != INVALID_BUFNUM; bufnum = buffer_cntl_blocks[bufnum].LRU_prev_bufnum)
                // Get unpinned LRU buffer number.
                // if (buffer_cntl_blocks[bufnum].number_of_pins == 0) {
                if (pthread_mutex_trylock(&buffer_cntl_blocks[bufnum].page_latch) == 0) {
                    // Update the buffer LRU list.
                    if (bufnum == buffer_info.LRU_tail_bufnum) {
                        buffer_info.LRU_tail_bufnum = buffer_cntl_blocks[bufnum].LRU_prev_bufnum;
                        buffer_cntl_blocks[buffer_info.LRU_tail_bufnum].LRU_next_bufnum = INVALID_BUFNUM;
                    } else if (bufnum == buffer_info.LRU_head_bufnum) {
                        buffer_info.LRU_head_bufnum = buffer_cntl_blocks[bufnum].LRU_next_bufnum;
                        buffer_cntl_blocks[buffer_info.LRU_head_bufnum].LRU_prev_bufnum = INVALID_BUFNUM;
                    } else {
                        buffer_cntl_blocks[buffer_cntl_blocks[bufnum].LRU_prev_bufnum].LRU_next_bufnum = buffer_cntl_blocks[bufnum].LRU_next_bufnum;
                        buffer_cntl_blocks[buffer_cntl_blocks[bufnum].LRU_next_bufnum].LRU_prev_bufnum = buffer_cntl_blocks[bufnum].LRU_prev_bufnum;
                    }
                    break;
                }
        }
        // Flush the LRU buffer. Write the page, if dirty.
        if (buffer_cntl_blocks[bufnum].is_dirty == true)
            file_write_page(buffer_cntl_blocks[bufnum].table_id, buffer_cntl_blocks[bufnum].pagenum, &frames[bufnum]);
    }

    // Initialize the buffer_cntl_block.
    buffer_cntl_blocks[bufnum].table_id = table_id;
    buffer_cntl_blocks[bufnum].pagenum = pagenum;
    buffer_cntl_blocks[bufnum].is_dirty = false;
    // buffer_cntl_blocks[bufnum].number_of_pins = 0;
    pthread_mutex_unlock(&buffer_cntl_blocks[bufnum].page_latch);

    // Read the page into the frame..
    file_read_page(table_id, pagenum, &frames[bufnum]);

    // Engueue LRU list.
    // I. no empty LRU list
    if (buffer_info.LRU_head_bufnum != INVALID_BUFNUM) {
        buffer_cntl_blocks[buffer_info.LRU_head_bufnum].LRU_prev_bufnum = bufnum;
        buffer_cntl_blocks[bufnum].LRU_next_bufnum = buffer_info.LRU_head_bufnum;
        buffer_cntl_blocks[bufnum].LRU_prev_bufnum = INVALID_BUFNUM;
        buffer_info.LRU_head_bufnum = bufnum;
    } 
    // II. empty LRU list
    else {
        buffer_cntl_blocks[bufnum].LRU_next_bufnum = INVALID_BUFNUM;
        buffer_cntl_blocks[bufnum].LRU_prev_bufnum = INVALID_BUFNUM;
        buffer_info.LRU_head_bufnum = bufnum;
        buffer_info.LRU_tail_bufnum = bufnum;
    }

    return bufnum;
}
int buffer_request_page(table_id_t table_id, pagenum_t pagenum, header_page_t*& page, int* bufnum) {

    int temp_bufnum;

    pthread_mutex_lock(&buffer_manager_latch);

    if (verbose) {
        printf(" |bufReq");
    }

    // Get the bufnum.
    // I. if the page exists in buffer, return it.
    for (temp_bufnum = buffer_info.LRU_head_bufnum; temp_bufnum != INVALID_BUFNUM; temp_bufnum = buffer_cntl_blocks[temp_bufnum].LRU_next_bufnum)
        if (table_id == buffer_cntl_blocks[temp_bufnum].table_id && pagenum == buffer_cntl_blocks[temp_bufnum].pagenum) {
            break;
        }
    // II. otherwise, get the new buffer number.
    if (temp_bufnum == INVALID_BUFNUM) 
        temp_bufnum = get_new_bufnum(table_id, pagenum);
    
    // Store the page and bufnum to parameter.
    *bufnum = temp_bufnum;
    page = (header_page_t*)(&frames[temp_bufnum]);

    // Pin the buffer.
    pthread_mutex_lock(&buffer_cntl_blocks[temp_bufnum].page_latch);
    // buffer_cntl_blocks[temp_bufnum].number_of_pins++;

    if (verbose) {
        printf(" p: %ld, b: %d", pagenum, *bufnum);
        printf("|");
    }

    pthread_mutex_unlock(&buffer_manager_latch);

    return OP_SUCCESS;
}
int buffer_request_page(table_id_t table_id, pagenum_t pagenum, node_page_t*& page, int* bufnum) {

    int temp_bufnum;

    pthread_mutex_lock(&buffer_manager_latch);

    if (verbose) {
        printf(" |bufReq");
    }
    // Get the bufnum.
    // I. if the page exists in buffer, return it.
    for (temp_bufnum = buffer_info.LRU_head_bufnum; temp_bufnum != INVALID_BUFNUM; temp_bufnum = buffer_cntl_blocks[temp_bufnum].LRU_next_bufnum)
        if (table_id == buffer_cntl_blocks[temp_bufnum].table_id && pagenum == buffer_cntl_blocks[temp_bufnum].pagenum) {
            break;
        }
    // II. otherwise, get the new buffer number.
    if (temp_bufnum == INVALID_BUFNUM) 
        temp_bufnum = get_new_bufnum(table_id, pagenum);
    
    // Store the page and bufnu m to parameter.
    *bufnum = temp_bufnum;
    page = (node_page_t*)(&frames[temp_bufnum]);

    // Pin the buffer.
    pthread_mutex_lock(&buffer_cntl_blocks[temp_bufnum].page_latch);
    // buffer_cntl_blocks[temp_bufnum].number_of_pins++;

    if (verbose) {
        printf(" p: %ld, b: %d", pagenum, *bufnum);
        printf("-");
    }
    
    pthread_mutex_unlock(&buffer_manager_latch);
    return OP_SUCCESS;
}

void buffer_release_page(int bufnum, bool is_dirty) {
    header_page_t* header_page;

    if (verbose) {
        printf("-bufRel %d", bufnum);
    }

    // I. Write the page, if header page
    if (buffer_cntl_blocks[bufnum].pagenum == 0 && is_dirty == true) {
        if (verbose) {
            header_page = (header_page_t*)(&frames[bufnum]);
            printf(" (header_page(%ld %ld %ld) flushed imm.)", header_page->first_free_pagenum, header_page->number_of_pages, header_page->root_pagenum);
        }
        file_write_page(buffer_cntl_blocks[bufnum].table_id, 0, &frames[bufnum]);
    }
    // II. Remain the page dirty in buffer.
    else
        buffer_cntl_blocks[bufnum].is_dirty |= is_dirty;
    
    // Unpin the buffer.
    pthread_mutex_unlock(&buffer_cntl_blocks[bufnum].page_latch);
    // buffer_cntl_blocks[bufnum].number_of_pins--;
    
    if (verbose) {
        // printf("(pin %d)", buffer_cntl_blocks[bufnum].number_of_pins);
        printf("|");
    }
}

void buffer_init(int num_buf) {
    int bufnum;

    if (verbose) {
        printf(" |buffer_init");
    }
    buffer_info.LRU_head_bufnum = INVALID_BUFNUM;
    buffer_info.LRU_tail_bufnum = INVALID_BUFNUM;
    buffer_info.first_free_bufnum = 0;

    buffer_cntl_blocks = (buffer_cntl_block_t*)malloc(num_buf * sizeof(buffer_cntl_block_t));
    if (buffer_cntl_blocks == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    // free buffer list
    for (bufnum = 0; bufnum < num_buf - 1; bufnum++)
        buffer_cntl_blocks[bufnum].free_next_bufnum = bufnum + 1;
    // The last buffer has no next free buffer.
    buffer_cntl_blocks[bufnum].free_next_bufnum = INVALID_BUFNUM;

    frames = (page_t*)malloc(num_buf * PAGE_SIZE);
    if (frames == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    if (verbose) {
        printf("|");
    }
}

void buffer_close_table_files(void) {
    int bufnum;
    // Write all dirty pages
    for (bufnum = buffer_info.LRU_head_bufnum; bufnum != INVALID_BUFNUM; bufnum = buffer_cntl_blocks[bufnum].LRU_next_bufnum)
        if (buffer_cntl_blocks[bufnum].is_dirty == true) {
            file_write_page(buffer_cntl_blocks[bufnum].table_id, buffer_cntl_blocks[bufnum].pagenum, &frames[bufnum]);
        }
    free(buffer_cntl_blocks);
    free(frames);
}