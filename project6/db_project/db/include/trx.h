#ifndef __TRX_H__
#define __TRX_H__

#include <stdint.h>
#include <pthread.h>

#include <queue>
#include <utility>

#include "page.h"

#define IMPLICIT_LOCKING 1

// Lock Table Manager
typedef struct lock_t lock_t;
typedef struct lock_table_entry_t lock_table_entry_t;
typedef struct std::pair<table_id_t, pagenum_t> hash_id_t;
typedef struct trx_t trx_t;
typedef struct uadate_log_t log_t;

class Hash {
    public: size_t operator() (const hash_id_t &hash_id) const {
        return hash_id.first + (hash_id.second << 5);
    }
};

enum LockMode {
    kLockShared = 0,
    kLockExclusive = 1,
};
enum TrxStatus {
    kTrxWaiting,
    kTrxRunning,
};

struct lock_t {
    page::key_t key;

    lock_t* lock_table_prev;
    lock_t* lock_table_next;
    lock_table_entry_t *sentinel;
    
    lock_t* trx_table_next;
    int owner_trx_id;

    pthread_cond_t cond_var;

    LockMode lock_mode;
};

struct lock_table_entry_t {
    table_id_t table_id;
    pagenum_t page_id;
    lock_t* lock_list_head;
    lock_t* lock_list_tail;
    lock_table_entry_t* entry_prev;
    lock_table_entry_t* entry_next;
};

struct trx_t {
    TrxStatus trx_status;
    int waiting_trx_id;
    lock_t* lock_list_head;
    lock_t* lock_list_tail;

    std::queue<int64_t> queue_LSN;
};


// Initialize any data structures,
// such as a hash table, a lock table latch, etc.
// If success, return 0.
// Otherwise, return non-zero value.
int trx_init(void);

// Allocate and append a new lock object to the lock list of the record having the page id and the key.
//      If there is a predecessor, sleep until the predecessor releases its lock.
//      If there is no predecessor, return the address of the new lock object.
// If an error occurs, return NULL.
lock_t* lock_acquire(int64_t table_id, pagenum_t page_id, int64_t key, int trx_id, int lock_mode);

// Remove the lock_obj from the lock list.
//      If there is a successor, wake up the successor.
// If success, return 0.
// Otherwise, return a non-zero value.
int lock_release(lock_t* lock_obj);


// If there is explicit lock, return true.
bool lock_exists(int64_t table_id, pagenum_t page_id, int64_t key, int trx_id);

// Allocate a transaction structure and initialize it.
// Retrun a unique transaction id (>= 1) if success, otherwise return 0.
int trx_begin(void);

// Clean up the trx and its related information that has been used in your lock manager.
// If success Return the completed trx_id.
// Otherwise, return 0.
// TODO: Users can get a response once all modifications of the trx are flushed from the log buffer to a log file.
//      If the user recieves the responce, it means that db cna recover committed trx even after a system crash.
int trx_commit(int trx_id);

// All modifications canceled and returned to the old state.
// If success,return the committed trx_id.
// Otherwise, return 0.
int trx_abort(int trx_id);
void trx_push_log_to_queue(int64_t LSN, int trx_id);
void trx_pop_log_from_queue(int trx_id);

// APi for log management.
//TODO: undo라서 락킹 필요없나?
void trx_surrect(int trx_id);
void trx_kill(int trx_id);
int trx_get_last_LSN(int trx_id);

// Utility

// Convert the implicit lock to explicit lock object.
// If not converted, return 0.
// If converted, return 1.
// If failed, return -1.
int convert_implicit_to_explicit(int64_t table_id, pagenum_t page_id, int64_t key);
bool is_alive_trx(int trx_id);
bool detect_deadlock(int trx_id);

#endif