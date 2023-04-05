#include "log.h"

#include <unistd.h>         // for read(), write(), fsync()
#include <fcntl.h>          // for open()
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <algorithm>

#include "page.h"
#include "file.h"
#include "buffer.h"
#include "trx.h"


int log_file_fd;
FILE* logmsg_file_fp;
int64_t g_flushed_LSN;

pthread_mutex_t log_buffer_latch;
char* log_buffer;
int64_t g_buffer_LSN;
// TODO: 초기화 시에 적절히 numbering 해주기.
// TODO: 뮤텍스로 보호해 줄 필요 없나?
int64_t g_LSN;


void log_init(int flag, int log_num, char* log_path, char* logmsg_path) {
    log_buffer = (char*)malloc(sizeof(LOG_BUFFER_SIZE));
    log_buffer_latch = PTHREAD_MUTEX_INITIALIZER;
    log_file_fd = open(pathname, O_RDWR|O_CREAT, 0777);
    // TODO: 언제 close 해주주지지이
    if(log_file_fd < 0) {
        perror("open() failure");
        exit(EXIT_FAILURE);
    }
    logmsg_file_fp = fopen(logmsg_path, "wr");
    if (logmsg_file_fp == NULL) {
        perror("fopen() failure");
        exit(EXIT_FAILURE);
    }

    g_flushed_LSN = lseek(log_file_fd, 0, SEEK_END);
    g_buffer_LSN = g_LSN = g_flushed_LSN;

    recover(flag, lognum);
    // force all logs.
    pthread_mutex_lock(&log_buffer_latch);
    write_log_buffer_to_file(g_LSN - g_flushed_LSN);
    g_flushed_LSN = g_LSN;
    pthread_mutex_unlock(&log_buffer_latch);
}

int64_t log_create(LogType type, int trx_id) {

    // Make a new log.
    trx_log_t* log = (trx_log_t*)malloc(sizeof(trx_log_t));
    if (log == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    log->log_size = TRX_LOG_SIZE;
    log->trx_id = trx_id;
    log->type = type;
    
    // Append the new log to the log buffer.
    pthread_mutex_lock(&log_buffer_latch);
    log->prev_LSN = g_LSN;
    g_LSN += log->log_size;
    log->LSN = g_LSN;

    append(log, log->log_size);

    pthread_mutex_unlock(&log_buffer_latch);
    return log->LSN;
}
int64_t log_create(LogType type, int trx_id, table_id_t table_id, pagenum_t page_id, uint16_t offset, uint16_t data_length, char* old_img, char* new_img) {
    update_log_t* log = (update_log_t*)malloc(sizeof(update_log_t));
    if (log == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    log->trx_id = trx_id;
    log->type = type;

    log->table_id = table_id;
    log->page_id = page_id;
    log->offset = offset;
    log->data_length = data_length;
    memcpy(log->old_img, old_img, data_length);
    memcpy(log->new_img, new_img, data_length);

    // Append the new log to the log buffer.
    pthread_mutex_lock(log_buffer_latch);
    log->log_size = 264;
    log->prev_LSN = g_LSN;
    g_LSN += 264;
    log->LSN = g_LSN;

    append_log(log, log->log_size);

    pthread_mutex_unlock(&log_buffer_latch);

    return log->LSN;
}
int64_t log_create(LogType type, int trx_id, table_id_t table_id, pagenum_t page_id, uint16_t offset, uint16_t data_length, char* old_img, char* new_img, int64_t next_undo_LSN) {
    update_log_t* log = (update_log_t*)malloc(sizeof(update_log_t));
    if (log == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    log->trx_id = trx_id;
    log->type = type;

    log->table_id = table_id;
    log->page_id = page_id;
    log->offset = offset;
    log->data_length = data_length;
    memcpy(log->old_img, old_img, data_length);
    memcpy(log->new_img, new_img, data_length);

    // Append the new log to the log buffer.
    pthread_mutex_lock(&log_buffer_latch);
    log->log_size = 272;
    log->prev_LSN = g_LSN;
    g_LSN += 272;
    log->LSN = g_LSN;
    log->next_undo_LSN = next_undo_LSN;

    append_log(log, log->log_size);

    pthread_mutex_unlock(&log_buffer_latch);

    return log->LSN;
}

int log_flush(void) {
    pthread_mutex_lock(&log_buffer_latch);
    write_log_buffer_to_file(g_LSN - g_flushed_LSN);
    g_flushed_LSN = g_LSN;
    pthread_mutex_unlock(&log_buffer_latch);
}
void log_get_update_log(update_log_t* log,int64_t LSN) {
    get_log(log, LSN, 272);
}

// Utility
void append_log(void*bfzedc log, int log_size) {
    if ((log->LSN - g_buffer_LSN) + log_size) > LOG_BUFFER_SIZE) {
        write_log_buffer_to_file(log->LSN - g_buffer_LSN);
        g_buffer_LSN = g_flushed_LSN = log->LSN;
    }
    memcpy(&log_buffer[log->LSN - g_buffer_LSN], log, log->log_size);
}

void recover(int flag, int log_num) {
    std::set<int> set_loser_trx_id;

    set_loser_trx_id = analysis();
    if (flag == 0) {
        redo(0);
        undo(set_loser_trx_id, 0);
    }
    if (flag == 1) {
        redo(log_num);
    }
    if (flag == 2) {
        redo(0);
        undo(set_loser_trx_id, log_num);
    }
}

std::set<int> analysis(void) {
    int64_t LSN;
    std::set<int> loser_list;
    std::set<int> winner_list;
    trx_log_t* trx_log;
    update_log_t* update_log;
    compensate_log_t* compensate_log;

    fprintf(logmsg_file_fp, "[ANALYSIS] Analysis pass start\n");
    
    LSN = 0;
    while (LSN < g_flushed_LSN) {
        read_log_file_to_buffer(LSN);
        while (LSN + TRX_LOG_SIZE) <= LOG_BUFFER_SIZE) {
            mempcy(trx_log, &log_buffer[LSN], TRX_LOG_SIZE);
            if (trx_log->type == kBegin) {
                loser_list.insert(trx_log->trx_id);
            } else if (trx_log->type == kCommit || trx_log->type == kRollback) {
                loser_list.erase(trx_log->trx_id);
                winner_list.insert(trx_log->trx_id);
            }
            LSN += trx_log->log_size;
        }
    }

    fprintf(logmsg_file_fp, "[ANALYSIS] Analysis success. Winner:");
    for (std::vector<int>::iterator it = winner_list.begin(); it != winner_list.end(); it++) 
        fpinrtf(logmsg_file_fp, " %d", *it);
    fprintf(logmsg_file_fp, ", Loser:");
    for (std::vector<int>::iterator it = loser_list.begin(); it != loser_list.end(); it++)
        fprintf(logmsg_file_fp, " %d", *it);
    fprintf("\n");

    return loser_list;
}
int redo(int log_num) {
    int64_t LSN;
    trx_log_t* trx_log;
    update_log_t* update_log;
    compensate_log_t* compensate_log;
    int leaf_bufnum;
    node_page_t* leaf_page;
    int log_count = 0;
    bool update_flag

    fprintf(logmsg_file_fp, "[REDO] Redo pass start\n");

    LSN = 0;
    while (LSN < g_flushed_LSN) {
        read_log_file_to_buffer(LSN);
        while(LSN + TRX_LOG_SIZE <= LOG_BUFFER_SIZE) {
            if (log_num != 0 && log_count == log_num) {
                return OP_SUCCESS;
            }
            memcpy(trx_log, &log_buffer[LSN], TRX_LOG_SIZE);
            if (trx_log->type == kBegin) {
                frpintf(logmsg_file_fp, "LSN %lu [BEGIN] Transaction id %d\n", trx_log->LSN, trx_log->trx_id);
                log_count += 1;
            } else if (trx_log->type == kCommit) {
                fprintf(logmsg_file_fp, "LSN %lu [COMMIT] Transaction id %d\n", trx_log->LSN, trx_log->trx_id);
                log_count += 1;
            } else if (trx_log->type == kRollback) {
                fprintf(logmsg_file_fp, "LSN %lu [ROLLBACK] Transaction id %d\n", trx_log->LSN, trx_log->trx_id);
                log_count += 1;
            } else if (trx_log->type == kUpdate) {
                memcpy(update_log, &log_buffer[LSN], trx_log->log_size);
                // Redo.
                buffer_request_page(update_log->table_id, update_log->page_id, leaf_page, &leaf_bufnum);
                if (update_log->LSN > leaf_page->header.LSN) {
                    memcpy(&leaf_page->values[VALUE_OFFSET(update_log->offset)], update_log->new_img, update_log->data_length);
                    leaf_page->header.LSN = update_log->LSN;
                    fprintf(logmsg_file_fp, "LSN %lu [UPDATE] Transaction id %d redo apply\n", update_log->LSN, update_log->trx_id);
                } else {
                    fprintf(logmsg_file_fp, "LSN %lu [CONSIDER-REDO] Transaction id %d\n", update_log->LSN, update_log->trx_id);
                }
                log_count += 1;
                buffer_release_page(leaf_bufnum, true);
            } else if (trx_log->type == kCompensate) {
                memcpy(compensate_log, &log_buffer[LSN], trx_log->log_size);
                // Redo.
                buffer_request_page(compensate_log->table_id, compensate_log->page_id, leaf_page, &leaf_bufnum);
                update_flag = false;
                if (compensate_log->LSN > leaf_page->header.LSN) {
                    update_flag = true;
                    memcpy(&leaf_page->values[VALUE_OFFSET(compensate_log->offset)], compensate_log->new_img, compensate_log->data_length);
                    leaf_page->header.LSN = compensate_log->LSN;
                    fprintf(logmsg_file_fp, "LSN %lu [CLR] next undo lsn %lu\n", compensate_log->LSN, compensate_log->next_undo_LSN);
                } else {
                    fprintf(logmsg_file_fp, "LSN %lu [CONSIDER-REDO] Transaction id %d\n", compensate_log->LSN, compensate_log->trx_id);
                }
                buffer_release_page(leaf_bufnum, update_flag;)
                log_count += 1;
            } else {
                perror("Log type error: in redo()");
                exit(EXIT_FAILURE);
            }
            LSN += trx_log->log_size;
        }
    }
    fprintf(logmsg_file_fp, "[REDO] Redo pass end\n");

    return OP_SUCCESS;
}
int undo(std::set<int> loser_list, int log_num) {
    std::set<int>::iterator trx_id;

    int64_t LSN;
    trx_log_t* trx_log;
    update_log_t* update_log;
    compensate_log_t* compensate_log;
    int leaf_bufnum;
    node_page_t* leaf_page;

    int64_t lastLSN;
    bool update_flag;

    int log_count = 0;

    fprintf(logmsg_file_fp, "[UNDO] Undo pass start\n");

    // Make the losers active.
    for (trx_id = loser_list.begin(); trx_id != loser_list.end(); trx_id++) {
        trx_surrect(trx_surrect);
    }
    // Add the LSNs to trx queue_LSN.
    LSN = 0;
    while (LSN < g_flushed_LSN) {
        read_log_file_to_buffer(LSN);
        while(LSN + TRX_LOG_SIZE <= LOG_BUFFER_SIZE) {
            memcpy(trx_log, &log_buffer[LSN], TRX_LOG_SIZE);
            if (loser_list.find(trx_log->trx_id) != loser_list.end())
                trx_push_log_to_queue(trx_log->LSN, trx_log->trx_id);
            LSN += trx_log->log_size;
        }
    }

    lastLSN = LSN - trx_log->log_size;
    trx_log = (trx_log_t*)malloc(sizeof(trx_log_t));
    if (trx_log == NULL) {
        perror("Memory allcation error");
        exit(EXIT_FAILURE);
    }
    update_log = (update_log_t*)malloc(sizeof(update_log_t));
    if (update_log == NULL) {
        perror("Memory allcation error");
        exit(EXIT_FAILURE);
    }
    compensate_log = (compensate_log_t*)malloc(sizeof(compensate_log));
    if (compensate_log == NULL) {
        perror("Memory allcation error");
        exit(EXIT_FAILURE);
    }

    // TODO: 여기서 부터 user connection 받아야 하는데 어떻게 같이 받지?
    // Undo.
    while (loser_list.empty() != false) {
        if (log_num != 0 && log_count == log_num) {
            free(trx_log);
            free(update_log);
            free(compensate_log);
            return OP_SUCCESS;
        }

        // Identify the log type.
        get_log(trx_log, lastLSN, TRX_LOG_SIZE);
        if (trx_log->type == kBegin) {

            // Remain log.
            log_create(kRollback, trx_log->trx_id);

            // Kill the trx.
            trx_kill(trx_log->trx_id);
            loser_list.erase(trx_log->trx_id);

            fprintf(logmsg_file_fp, "LSN %lu [ROLLBACK] Transaction id %d", trx_log->LSN, trx_id, trx_log->trx_id);

            // TODO: begin도 세어야 하나?
            count += 1;
        } else if (trx_log->type == kUpdate) {
            get_log(update_log, lastLSN, trx_log->log_size);

            // Pop the LSN from the queue of the trx.
            trx_pop_log_from_queue(update_log->trx_id);
            // Update the last LSN
            lastLSN = trx_get_last_LSN(update_log->trx_id);

            // Undo.
            buffer_request_page(update_log->table_id, update_log->page_id, leaf_page, &leaf_bufnum);
            update_flag = false;
            if (update_log->LSN <= leaf_page->header.LSN) {
                // Create CLR.
                log_create(kCompensate, update_log->trx_id, update_log->table_id, update_log->page_id, update_log->offset, update_log->data_length, update_log->new_img, update_log->old_img, trx_get_last_LSN(update_log->trx_id));

                // Update.
                update_flag = true;
                memcpy(&leaf_page->values[VALUE_OFFSET(update_log->offset)], update_log->old_img, update_log->data_length);
                fprintf(logmsg_file_fp, "LSN %lu [UPDATE] Transaction id %d undo apply", update_log->LSN, update_log->trx_id);

                count += 1;
            }
            buffer_release_page(leaf_bufnum, update_flag);
        } else if (trx_log->type == kCompensate) {
            get_log(compensate_log, lastLSN, trx_log->log_size);

            // Pop the LSN (> lastLSN) from the queue of the trx.
            while (trx_get_last_LSN(compensate_log->trx_id) > compensate_log->next_undo_LSN)
                trx_pop_log_from_queue(compensate_log->trx_id);
            // Update the last LSN.
            lastLSN = trx_get_last_LSN(compensate_log->trx_id);

            // Undo.
            buffer_request_page(compensate_log->table_id, compensate_log->page_id, leaf_page, &leaf_bufnum);
            update_flag = false;
            if (compensate_log->LSN <= leaf_page->header.LSN) {
                // Create CLR.
                log_create(kCompensate, compensate_log->trx_id, compensate_log->table_id, compensate_log->page_id, compensate_log->offset, compensate_log->data_length, compensate_log->new_img, compensate_log->old_img, trx_get_last_LSN(compensate_log->trx_id));

                // Update.
                update_flag = true;
                memcpy(&leaf_page->values[VALUE_OFFSET(compensate_log->offset)], compensate_log->old_img, compensate_log->data_length);
                fprintf(logmsg_file_fp, "LSN %lu [CLR] next undo lsn %lu\n", compensate_log->LSN, compensate_log->next_undo_LSN);

                count += 1;
            }
            buffer_release_page(leaf_bufnum, update_flag);
        }
        
        // Update the last LSN of the trx. Find the maximum last LSN.
        for (trx_id = loser_list.begin(); trx_id != loser_list.end(); trx_id++)
            lastLSN = std::max(lastLSN, trx_get_last_LSN(trx_id));
    }

    free(trx_log);
    free(update_log);
    free(compensate_log);

    fprintf(logmsg_file_fp, "[UNDO] Undo pass end\n");

    return OP_SUCCESS;
}

void read_log_file_to_buffer(int64_t LSN) {
    lseek(log_file_fd, LSN, 0);
    if (read(log_file_fd, log_buffer, LOG_BUFFER_SIZE)) {
        perror("File read failure");
        exit(EXIT_FAILURE);
    }
}
void wrtie_log_buffer_to_file(int log_buffer_size) {
    lseek(log_file_fd, 0, SEEK_END);
    if (write(log_file_fd, log_buffer, log_buffer_size) != log_buffer_size) {
        perror("File write failure");
        exit(EXIT_FAILURE);
    }
    if (fsync(log_file_fd) < 0) {
        perror("File sync failure");
        exit (EXIT_FAILURE);
    }
}
void read_log_from_file(void* log, int offset_from_end, int log_size) {
    lseek(log_file_fd, offset_from_end, SEEK_END);
    if (read(log_file_fd, log, log_size) != log_size) {
        perror("File read failure: in read_log_from_file()");
        exit(EXIT_FAILURE);
    }
}

void get_log(void* log, int64_t LSN, int log_size) {

    // The log is on disk.
    if (LSN < g_buffer_LSN) {
        if (g_buffer_LSN < 0) {
            perror("fatal: in log_get_update_log()");
            exit(EXIT_FAILURE);
        }
        read_log_from_file(log, LSN - g_flushed_LSN, log->log_size);
    }
    // The log is in buffer.
    else {
        memcpy(log, &log_buffer[LSN - g_buffer_LSN], log_size);
    }
}