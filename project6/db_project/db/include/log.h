
#ifndef __LOG_H__
#define __LOG_H__

#include <stdint.h>

#include <set>

#include "page.h"

#define LOG_BUFFER_SIZE (PAGE_SIZE * 1000)
#define TRX_LOG_SIZE 28

enum LogType {
    kBegin = 0,
    kUpdate = 1,
    kCommit = 2,
    kRollback = 3,
    kCompensate = 4,
};

struct trx_log_t {
    int log_size;
    int64_t LSN;
    int64_t prev_LSN;
    int trx_id;
    LogType type;
};

struct update_log_t {
    int log_size;
    int64_t LSN;
    int64_t prev_LSN;
    int trx_id;
    LogType type;

    table_id_t table_id;
    pagenum_t page_id;
    uint16_t offset;
    uint16_t data_length;
    char old_img[108];
    char new_img[108];
};

struct compensate_log_t {
    int log_size;
    int64_t LSN;
    int64_t prev_LSN;
    int trx_id;
    LogType type;

    table_id_t table_id;
    pagenum_t page_id;
    uint16_t offset;
    uint16_t data_length;
    char old_img[108];
    char new_img[108];
    int64_t next_undo_LSN;
};

// APIs
void log_init(int flag, int log_num, char* log_path, char* logmsg_path);

// If success, return LSN.
// Otherwise, negative value.
int64_t log_create(LogType type, int trx_id);
int64_t log_create(LogType type, int trx_id, table_id_t table_id, pagenum_t page_id, uint16_t offset, uint16_t data_length, char* old_img, char* new_img);
int64_t log_create(LogType type, int trx_id, table_id_t table_id, pagenum_t page_id, uint16_t offset, uint16_t data_length, char* old_img, char* new_img, int64_t next_undo_LSN);

int log_flush(void);
void log_get_update_log(update_log_t* log, int64_t LSN);

// Utility
void append_log(void* log, int log_size);
void recover(int flag, int log_num);

std::set<int> analysis(void);
// log_num == 0: normal recovery
int redo(int log_num);
int undo(std::set<int> loser_list, int log_num);

void read_log_file_to_buffer(int64_t LSN);
void write_log_buffer_to_file(int log_buffer_size);
void read_log_from_file(void* log, int offset_from_end, int log_size);

void get_log(void* log, int64_t LSN, int log_size);



#endif

