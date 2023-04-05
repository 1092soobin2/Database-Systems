#include "file.h"

#include <unistd.h>         // for read(), write(), fsync()
#include <fcntl.h>          // for open()
#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>         // for exit()
#include <stdint.h>
#include <vector>
#include <string.h>         // for memset()
#include <utility>

#include "page.h"

using namespace std;

extern bool verbose;

// Data structure used in disk space management layer.
std::vector< std::pair<const char*, int> > tid_vector_path_fd;

// Open existing database file or create one if not existed.
// Return new table_id.
int64_t file_open_table_file(const char* pathname) {
    
    table_id_t tid;
    int is_new_file;
    int fd;
    file_manager_page_t *header_page;
    pagenum_t free_pagenum;
    file_manager_page_t *free_page;

    if (verbose) {
        printf("\t|file_open_table_file ");
    }
    // I. The table file is opened.
    for (tid = 0; tid < tid_vector_path_fd.size(); tid++)
        if (strcmp(tid_vector_path_fd[tid].first, pathname) == 0)
            return tid;
    
    // II. The table not yet opened. But the number of tables exceeds MAX_TABLES.
    if (tid_vector_path_fd.size() == MAX_TABLES)
        return -1;

    // III. Get the table file.
    is_new_file = access(pathname, F_OK);
    // III-1. Open the table file.
    fd = open(pathname, O_RDWR|O_CREAT, 0777);
    if (fd < 0) {
        perror("File open failure");
        // exit(EXIT_FAILURE);
        return -1;
    }

    // III-2. Create the table file.
    if (is_new_file != 0) {
        if (verbose) {
            printf("(newly created file");
        }
        // Update the metadata in the header page. (Alloc, modify, write)
        header_page = (file_manager_page_t*)malloc(PAGE_SIZE);
        if (header_page == NULL) {
            perror("Memory allocation error");
            exit(EXIT_FAILURE);
        }
        header_page->first_free_pagenum = 1;
        header_page->number_of_pages = 2560;    // 10M / 4K
        if (pwrite(fd, header_page, PAGE_SIZE, 0) != PAGE_SIZE) {
            perror("File write failure");
            exit(EXIT_FAILURE);
        }
        free(header_page);

        // Update the newly created free pages. (Alloc, modify, write)
        free_page = (file_manager_page_t*)malloc(PAGE_SIZE);
        if (free_page == NULL) {
            perror("Memory allocation error");
            exit(EXIT_FAILURE);
            return -1;
        }
        for (free_pagenum = 1; free_pagenum < 2559; free_pagenum++) {
            if (verbose) {
                printf(".");
            }
            free_page->next_free_pagenum = free_pagenum + 1;
            if (pwrite(fd, free_page, PAGE_SIZE, PAGE_OFFSET(free_pagenum)) != PAGE_SIZE) {
                perror("File write failure");
                exit(EXIT_FAILURE);
            }
        }
        // Update the last free page.
        free_page->next_free_pagenum = 0;
        if (pwrite(fd, free_page, PAGE_SIZE, PAGE_OFFSET(free_pagenum)) != PAGE_SIZE) {
            perror("File write failure");
            exit(EXIT_FAILURE);
        }
        free(free_page);
    }

    // Add table_id, pathname and opened fd to tid vector.
    tid = tid_vector_path_fd.size();
    tid_vector_path_fd.push_back(make_pair(pathname, fd));

    if (fsync(fd)< 0) {
        perror("File sync failure");
        exit (EXIT_FAILURE);
    }
    
    if (verbose) {
        printf("(tid: %ld)|", tid);
    }

    return tid;
}

// Allocate an on-disk page from the free page list.
pagenum_t file_alloc_page(table_id_t table_id) {

    int fd;
    file_manager_page_t *header_page;
    pagenum_t first_free_pagenum;
    pagenum_t free_pagenum;
    file_manager_page_t *free_page;
    file_manager_page_t *first_free_page;

    if (verbose) {
        printf("\t|file_alloc_page");
    }

    // Convert the table id to the mapped file.
    fd = tid_vector_path_fd[table_id].second;

    // Read the header page.
    wrapper_read(fd, 0, header_page);

    // If there is no page in the free page list,
    // Double the current file. Allocate as much as the current DB size.
    if (header_page->first_free_pagenum == 0) {
        if (verbose) {
            printf("(no free page list)");
        }
        // Modify the metadata in header page.
        header_page->first_free_pagenum = header_page->number_of_pages;
        header_page->number_of_pages *= 2;

        // Update the newly created free pages. (Alloc, modify, write)
        free_page = (file_manager_page_t*)malloc(PAGE_SIZE);
        if (free_page == NULL) {
            perror("Memory allocation error");
            exit(EXIT_FAILURE);
        }
        for (free_pagenum = header_page->first_free_pagenum; free_pagenum < header_page->number_of_pages - 1; free_pagenum++ ) {
            free_page->next_free_pagenum = free_pagenum + 1;
            if (pwrite(fd, free_page, PAGE_SIZE, PAGE_OFFSET(free_pagenum)) != PAGE_SIZE) {
                perror("File write failure");
                exit(EXIT_FAILURE);
            }
        }
        // Update the last free page.
        free_page->next_free_pagenum = 0;
        if (pwrite(fd, free_page, PAGE_SIZE, PAGE_OFFSET(free_pagenum)) != PAGE_SIZE) {
            perror("File write failure");
            exit(EXIT_FAILURE);
        }
        free(free_page);
    }

    // Get the first free page number.
    first_free_pagenum = header_page->first_free_pagenum;
    // Read the first free page.
    wrapper_read(fd, first_free_pagenum, first_free_page);

    // Modify the metadata in the header page.
    header_page->first_free_pagenum = first_free_page->next_free_pagenum;
    if (verbose) {
        printf(" first_free_pn: %ld, next: %ld", first_free_pagenum, header_page->first_free_pagenum);
    }
    if (pwrite(fd, header_page, PAGE_SIZE, 0) != PAGE_SIZE) {
        perror("File write failure");
        exit(EXIT_FAILURE);
    }
    free(header_page);

    // Initialize the first free page that will be allocated.
    memset (first_free_page, 0, PAGE_SIZE);
    if (pwrite(fd, first_free_page, PAGE_SIZE, PAGE_OFFSET(first_free_pagenum)) != PAGE_SIZE) {
        perror("File write failure");
        exit(EXIT_FAILURE);
    }
    free(first_free_page);

    if (fsync(fd) < 0) {
        perror("File sync failure");
        exit(EXIT_FAILURE);
    }

    if (verbose) {
        printf("|");;
    }

    return first_free_pagenum;
}

// Free an on-disk page to the free page list.
void file_free_page(table_id_t table_id, pagenum_t pagenum) {

    int fd;
    file_manager_page_t *header_page;
    file_manager_page_t *new_free_page;

    if (verbose) {
        printf(" |file_free_page");
    }
    // Convert the table id to the mapped file.
    fd = tid_vector_path_fd[table_id].second;

    // Read the pages.
    wrapper_read(fd, 0, header_page);
    wrapper_read(fd, pagenum, new_free_page);

    // Add the page in free page list.
    new_free_page->next_free_pagenum = header_page->first_free_pagenum;
    header_page->first_free_pagenum = pagenum;

    // Write the pages.
    if (pwrite(fd, header_page, PAGE_SIZE, 0) != PAGE_SIZE) {
        perror("File write failure");
        exit(EXIT_FAILURE);
    }
    free(header_page);
    if (pwrite(fd, new_free_page, PAGE_SIZE, PAGE_OFFSET(pagenum)) != PAGE_SIZE) {
        perror("File write failure");
        exit(EXIT_FAILURE);
    }
    free(new_free_page);

    if (verbose) {
        printf(" |");
    }
}

// Read an on-disk page into the in-memory page structure(dest).
void file_read_page(table_id_t table_id, pagenum_t pagenum, page_t* dest) {
    
    int fd;

    // Convert the table id to the mapped file.
    fd = tid_vector_path_fd[table_id].second;

    if (pread(fd, dest, PAGE_SIZE, PAGE_OFFSET(pagenum)) != PAGE_SIZE) {
        perror("File read failure");
        exit(EXIT_FAILURE);
    }
}

// Write an in-memory page(src) to the on-disk page.
void file_write_page(table_id_t table_id, pagenum_t pagenum, const page_t* src){

    int fd;

    // Convert the table id to the mapped file.
    fd = tid_vector_path_fd[table_id].second;

    if (pwrite(fd, src, PAGE_SIZE, PAGE_OFFSET(pagenum)) != PAGE_SIZE) {
        perror("File write failure");
        exit(EXIT_FAILURE);
    }
    if (fsync(fd) < 0) {
        perror("File sync failure");
        exit (EXIT_FAILURE);
    }
}

// Stop referencing the database file
void file_close_table_files(void) {

    table_id_t tid;
    
    for (tid = 0; tid < tid_vector_path_fd.size(); tid++)
        if ( close(tid_vector_path_fd[tid].second) < 0){
            perror("File close failure");
            exit(EXIT_FAILURE);
        }

    tid_vector_path_fd.clear();  
}

bool file_is_valid_table_id(table_id_t table_id) {
    if (table_id >= 0 && table_id < tid_vector_path_fd.size())
        return true;
    else
        return false;
}

void file_init(void) {
    tid_vector_path_fd.reserve(MAX_TABLES);
}

inline void wrapper_read(int fd, pagenum_t pagenum, file_manager_page_t*& dst) {
    dst = (file_manager_page_t*)malloc(PAGE_SIZE);
    if (dst == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    if (pread(fd, dst, PAGE_SIZE, PAGE_OFFSET(pagenum)) != PAGE_SIZE) {
        perror("File read failure");
        exit(EXIT_FAILURE);
    }
}

inline void wrapper_write(int fd, pagenum_t pagenum, file_manager_page_t*& src) {
    if (pwrite(fd, src, PAGE_SIZE, PAGE_OFFSET(pagenum)) != PAGE_SIZE) {
        perror("File write failure");
        exit(EXIT_FAILURE);
    }
    free(src);
}

// test APIs
pagenum_t file_test_get_ffpn (table_id_t tid) {
    
    int fd;
    file_manager_page_t *header_page;
    pagenum_t ffpn;

    if (file_is_valid_table_id(tid) == false)
        return -1;

    fd = tid_vector_path_fd[tid].second;
    wrapper_read(fd, 0, header_page);
    ffpn = header_page->first_free_pagenum;

    delete header_page;

    return ffpn;
}

uint64_t file_test_get_np (table_id_t tid) {

    int fd;
    file_manager_page_t *header_page;
    uint64_t number_of_pages;
    
    if (file_is_valid_table_id(tid) == false)
        return -1;

    fd = tid_vector_path_fd[tid].second;

    wrapper_read(fd, 0, header_page);
    number_of_pages = header_page->number_of_pages;

    free(header_page);

    return number_of_pages;
}

pagenum_t file_test_get_nfn(table_id_t tid, pagenum_t ffn) {

    int fd;
    file_manager_page_t* free_page;
    pagenum_t nfn;

    fd = tid_vector_path_fd[tid].second;

    wrapper_read(fd, ffn , free_page);
    nfn = free_page->next_free_pagenum;

    free(free_page);

    return nfn;
}

const char* get_pathname (table_id_t tid) {
    
    if (tid >= 0 && tid < tid_vector_path_fd.size())
        return tid_vector_path_fd[tid].first;
    else
        return nullptr;
}

