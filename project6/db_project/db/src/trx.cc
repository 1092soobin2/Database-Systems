#include "trx.h"

#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include <unordered_map>
#include <utility>
#include <cstring>

#include "buffer.h"
#include "log.h"

// Lock Table
std::unordered_map<hash_id_t, lock_table_entry_t, Hash> lock_table;
pthread_mutex_t lock_table_latch;

// Trx Table
int g_trx_id;
std::unordered_map<int, trx_t> trx_table;
pthread_mutex_t trx_table_latch;

int trx_init(void) {
    lock_table_latch = PTHREAD_MUTEX_INITIALIZER;
    trx_table_latch = PTHREAD_MUTEX_INITIALIZER;
    g_trx_id = 0;
    return 0;
}
#if IMPLICIT_LOCKING
lock_t* lock_acquire(int64_t table_id, pagenum_t page_id, int64_t key, int trx_id, int lock_mode) {

    hash_id_t hash_id;
    lock_t* new_lock_obj;
    lock_table_entry_t* lock_table_entry;
    lock_t* lock_pred;
    bool pred_flag = false, shared_pred_flag;        // pred_flag: same key record, shared_flag: lock mode of that record.

    pthread_mutex_lock(&lock_table_latch);

    if (convert_implicit_to_explicit(table_id, page_id, key) == -1)
        return NULL;
    
    hash_id = std::make_pair(table_id, page_id);

    // Hash lock table entry.
    lock_table_entry = &lock_table[hash_id];
    // I. there is some entry. Resolve collision.
    if (page_id << 5 == lock_table_entry->page_id) {
        while (page_id != lock_table_entry->page_id) {
            if (lock_table_entry->entry_next == NULL) {
                // If there is no entry corresponding table_id and page_id.
                lock_table_entry->entry_next = (lock_table_entry_t*) malloc(sizeof(lock_table_entry_t));
                if (lock_table_entry->entry_next == NULL) {
                    perror("Memory allocation error");
                    exit(EXIT_FAILURE);
                }
                lock_table_entry->entry_next->entry_prev = lock_table_entry;
                lock_table_entry = lock_table_entry->entry_next;
                lock_table_entry->entry_next = NULL;
                lock_table_entry->table_id = table_id;
                lock_table_entry->page_id = page_id;
                lock_table_entry->lock_list_head = NULL;
                lock_table_entry->lock_list_tail = NULL;
                break;
            } 
            lock_table_entry = lock_table_entry->entry_next;
        }
    }
    // II. there is no entry.
    else {
        lock_table_entry->table_id = table_id;
        lock_table_entry->page_id = page_id;
        lock_table_entry->lock_list_head = NULL;
        lock_table_entry->lock_list_tail = NULL;
        lock_table_entry->entry_prev = NULL;
        lock_table_entry->entry_next = NULL;
    }

    // Traverse lock table list.
    lock_pred = lock_table_entry->lock_list_head;
    while (lock_pred != NULL) {
        if (lock_pred->key == key) {
            if (lock_pred->owner_trx_id == trx_id) {
                pthread_mutex_lock(&trx_table_latch);
                if (lock_pred->lock_mode == kLockExclusive && trx_table[lock_pred->owner_trx_id].trx_status == kTrxRunning) {
                    pthread_mutex_unlock(&trx_table_latch);
                    return lock_pred;
                }
                pthread_mutex_unlock(&trx_table_latch);
            } else {
                pred_flag = true;
                if (lock_pred->lock_mode == kLockShared && lock_mode == kLockShared) {
                    shared_pred_flag = true;
                } else {
                    shared_pred_flag = false;
                    break;
                }
            }
        }
        lock_pred = lock_pred->lock_table_next;         
    }

    // Create a new lock object.
    new_lock_obj = (lock_t*)malloc(sizeof(lock_t));
    if (new_lock_obj == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    new_lock_obj->key = key;
    new_lock_obj->sentinel = lock_table_entry;
    new_lock_obj->owner_trx_id = trx_id;
    new_lock_obj->cond_var = PTHREAD_COND_INITIALIZER;
    new_lock_obj->lock_mode = (LockMode)lock_mode;

    // Push the new_lock_obj into trx_table.
    pthread_mutex_lock(&trx_table_latch);
    // I. there is no lock object.
    if (trx_table[trx_id].lock_list_head == NULL) {
        new_lock_obj->trx_table_next = NULL;
        trx_table[trx_id].lock_list_head = new_lock_obj;
        trx_table[trx_id].lock_list_tail = new_lock_obj;
    }
    // II. there is some lock objects.
    else {
        new_lock_obj->trx_table_next = NULL;
        trx_table[trx_id].lock_list_tail->trx_table_next = new_lock_obj;
        trx_table[trx_id].lock_list_tail = new_lock_obj;
    }
    pthread_mutex_unlock(&trx_table_latch);

    // Push the lock object into the lock table list.
    // I. there is no predecossor whose key is same, acquire lock immediately.
    if (pred_flag == false) {
        // Push the lock object into the lock table list.
        // I-1. empty lock table list.
        if (lock_table_entry->lock_list_head == NULL) {
            new_lock_obj->lock_table_prev = NULL;
            new_lock_obj->lock_table_next = NULL;
            lock_table_entry->lock_list_head = new_lock_obj;
            lock_table_entry->lock_list_tail = new_lock_obj;
        }
        // I-2. non-empty lock table list.
        else {
            lock_table_entry->lock_list_tail->lock_table_next = new_lock_obj;
            new_lock_obj->lock_table_prev = lock_table_entry->lock_list_tail;
            new_lock_obj->lock_table_next = NULL;
            lock_table_entry->lock_list_tail = new_lock_obj;
        }
    }
    // II. There are some predecessors whose key is same..
    else {
        // Push the lock object into the lock table list.
        lock_table_entry->lock_list_tail->lock_table_next = new_lock_obj;
        new_lock_obj->lock_table_prev = lock_table_entry->lock_list_tail;
        new_lock_obj->lock_table_next = NULL;
        lock_table_entry->lock_list_tail = new_lock_obj;

        // If not compatable, sleep until the pred to release its lock.
        if (shared_pred_flag == false) {

            // Transaction is waiting.
            pthread_mutex_lock(&trx_table_latch);
            trx_table[new_lock_obj->owner_trx_id].trx_status = kTrxWaiting;
            trx_table[new_lock_obj->owner_trx_id].waiting_trx_id = lock_pred->owner_trx_id;

            // If the predecessor's status is 'waiting', detect deadlock.
            if (trx_table[lock_pred->owner_trx_id].trx_status == kTrxWaiting) {
                if (detect_deadlock(trx_id) == true) {
                    return NULL;
                }
            }
            pthread_mutex_unlock(&trx_table_latch);

            pthread_cond_wait(&new_lock_obj->cond_var, &lock_table_latch);
        }
    }
    pthread_mutex_unlock(&lock_table_latch);

    // The transaction is running.
    pthread_mutex_lock(&trx_table_latch);
    trx_table[new_lock_obj->owner_trx_id].trx_status = kTrxRunning;
    pthread_mutex_unlock(&trx_table_latch);
    return new_lock_obj;
}
#else
lock_t* lock_acquire(int64_t table_id, pagenum_t page_id, int64_t key, int trx_id, int lock_mode) {

    hash_id_t hash_id;
    lock_t* new_lock_obj;
    lock_table_entry_t* lock_table_entry;
    lock_t* lock_pred;
    bool pred_flag = false, shared_pred_flag;        // pred_flag: same key record, shared_flag: lock mode of that record.

    pthread_mutex_lock(&lock_table_latch);

    hash_id = std::make_pair(table_id, page_id);

    // Hash lock table entry.
    lock_table_entry = &lock_table[hash_id];
    // I. there is some entry. Resolve collision.
    if (page_id << 5 == lock_table_entry->page_id) {
        while (page_id != lock_table_entry->page_id) {
            if (lock_table_entry->entry_next == NULL) {
                // If there is no entry corresponding table_id and page_id.
                lock_table_entry->entry_next = (lock_table_entry_t*) malloc(sizeof(lock_table_entry_t));
                if (lock_table_entry->entry_next == NULL) {
                    perror("Memory allocation error");
                    exit(EXIT_FAILURE);
                }
                lock_table_entry->entry_next->entry_prev = lock_table_entry;
                lock_table_entry = lock_table_entry->entry_next;
                lock_table_entry->entry_next = NULL;
                lock_table_entry->table_id = table_id;
                lock_table_entry->page_id = page_id;
                lock_table_entry->lock_list_head = NULL;
                lock_table_entry->lock_list_tail = NULL;
                break;
            }
            lock_table_entry = lock_table_entry->entry_next;
        }
    }
    // II. there is no entry.
    else {
        lock_table_entry->table_id = table_id;
        lock_table_entry->page_id = page_id;
        lock_table_entry->lock_list_head = NULL;
        lock_table_entry->lock_list_tail = NULL;
        lock_table_entry->entry_prev = NULL;
        lock_table_entry->entry_next = NULL;
    }

    // Traverse lock table list.
    lock_pred = lock_table_entry->lock_list_head;
    while (lock_pred != NULL) {
        if (lock_pred->key == key) {
            if (lock_pred->owner_trx_id == trx_id) {
                if (lock_pred->lock_mode == kLockShared && lock_mode == kLockExclusive) 
                    continue;
                else
                    return lock_pred;
            } else {
                pred_flag = true;
                if (lock_pred->lock_mode == kLockShared && lock_mode == kLockShared) {
                    shared_pred_flag = true;
                } else {
                    shared_pred_flag = false;
                    break;
                }
            }
        }
        lock_pred = lock_pred->lock_table_next;         
    }

    // Create a new lock object.
    new_lock_obj = (lock_t*)malloc(sizeof(lock_t));
    if (new_lock_obj == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    new_lock_obj->key = key;
    new_lock_obj->sentinel = lock_table_entry;
    new_lock_obj->owner_trx_id = trx_id;
    new_lock_obj->cond_var = PTHREAD_COND_INITIALIZER;
    new_lock_obj->lock_mode = (LockMode)lock_mode;

    // Push the new_lock_obj into trx_table.
    pthread_mutex_lock(&trx_table_latch);
    // I. there is no lock object.
    if (trx_table[trx_id].lock_list_head == NULL) {
        new_lock_obj->trx_table_next = NULL;
        trx_table[trx_id].lock_list_head = new_lock_obj;
        trx_table[trx_id].lock_list_tail = new_lock_obj;
    }
    // II. there is some lock objects.
    else {
        new_lock_obj->trx_table_next = NULL;
        trx_table[trx_id].lock_list_tail->trx_table_next = new_lock_obj;
        trx_table[trx_id].lock_list_tail = new_lock_obj;
    }
    pthread_mutex_unlock(&trx_table_latch);

    // Push the lock object into the lock table list.
    // I. there is no predecossor whose key is same, acquire lock immediately.
    if (pred_flag == false) {
        // Push the lock object into the lock table list.
        // I-1. empty lock table list.
        if (lock_table_entry->lock_list_head == NULL) {
            new_lock_obj->lock_table_prev = NULL;
            new_lock_obj->lock_table_next = NULL;
            lock_table_entry->lock_list_head = new_lock_obj;
            lock_table_entry->lock_list_tail = new_lock_obj;
        }
        // I-2. non-empty lock table list.
        else {
            lock_table_entry->lock_list_tail->lock_table_next = new_lock_obj;
            new_lock_obj->lock_table_prev = lock_table_entry->lock_list_tail;
            new_lock_obj->lock_table_next = NULL;
            lock_table_entry->lock_list_tail = new_lock_obj;
        }
    }
    // II. There are some predecessors whose key is same..
    else {
        // Push the lock object into the lock table list.
        lock_table_entry->lock_list_tail->lock_table_next = new_lock_obj;
        new_lock_obj->lock_table_prev = lock_table_entry->lock_list_tail;
        new_lock_obj->lock_table_next = NULL;
        lock_table_entry->lock_list_tail = new_lock_obj;

        // If not compatable, sleep until the pred to release its lock.
        if (shared_pred_flag == false) {

            // Transaction is waiting.
            pthread_mutex_lock(&trx_table_latch);
            trx_table[new_lock_obj->owner_trx_id].trx_status = kTrxWaiting;
            trx_table[new_lock_obj->owner_trx_id].waiting_trx_id = lock_pred->owner_trx_id;

            // If the predecessor's status is 'waiting', detect deadlock.
            if (trx_table[lock_pred->owner_trx_id].trx_status == kTrxWaiting) {
                if (detect_deadlock(trx_id) == true) {
                    return NULL;
                }
            }
            pthread_mutex_unlock(&trx_table_latch);

            pthread_cond_wait(&new_lock_obj->cond_var, &lock_table_latch);
        }
    }
    pthread_mutex_unlock(&lock_table_latch);

    // The transaction is running.
    pthread_mutex_lock(&trx_table_latch);
    trx_table[new_lock_obj->owner_trx_id].trx_status = kTrxRunning;
    pthread_mutex_unlock(&trx_table_latch);
    return new_lock_obj;
}
#endif
int lock_release(lock_t* lock_obj) {

    lock_table_entry_t* lock_table_entry;
    lock_t *lock_pred, *lock_succ;
    bool waiting_succ_flag = false, pred_flag = false;

    pthread_mutex_lock(&lock_table_latch);

    lock_table_entry = lock_obj->sentinel;

    // Traverse lock table list.
    lock_succ = lock_obj->lock_table_next;
    while (lock_succ != NULL) {
        if (lock_succ->key == lock_obj->key) {
            // I. There is a waitng successor.
            pthread_mutex_lock(&trx_table_latch);
            if (trx_table[lock_succ->owner_trx_id].trx_status == kTrxWaiting)
                waiting_succ_flag = true;
            pthread_mutex_unlock(&trx_table_latch);
            
            // II. There is a running successor.
            // do nothing

            break;
        }
        lock_succ = lock_succ->lock_table_next;
    }
    
    if (waiting_succ_flag == true) {
        lock_pred = lock_table_entry->lock_list_head;
        while (lock_pred != lock_obj) {
            if (lock_pred->key == lock_obj->key && lock_pred->owner_trx_id != lock_succ->owner_trx_id) {
                pred_flag = true;
                break;
            }
            lock_pred = lock_pred->lock_table_next;
        }
    }

    // Remove the lock_obj from the lock_table.
    if (lock_obj->lock_table_next != NULL && lock_obj->lock_table_prev != NULL) {
        lock_obj->lock_table_prev->lock_table_next = lock_obj->lock_table_next;
        lock_obj->lock_table_next->lock_table_prev = lock_obj->lock_table_prev;
    } else if (lock_obj->lock_table_next != NULL && lock_obj->lock_table_prev == NULL) {
        lock_obj->lock_table_next->lock_table_prev = NULL;
        lock_table_entry->lock_list_head = lock_obj->lock_table_next;
    } else if (lock_obj->lock_table_next == NULL && lock_obj->lock_table_prev != NULL) {
        lock_obj->lock_table_prev->lock_table_next = NULL;
        lock_table_entry->lock_list_tail = lock_obj->lock_table_prev;
    } else {
        lock_table_entry->lock_list_tail = NULL;
        lock_table_entry->lock_list_head = NULL;
        // Remove the lock_table_entry if not the first.
        if (lock_table_entry->entry_prev != NULL) {
            lock_table_entry->entry_prev->entry_next = lock_table_entry->entry_next;
            if (lock_table_entry->entry_next != NULL)
                lock_table_entry->entry_next->entry_prev = lock_table_entry->entry_prev;
            free(lock_table_entry);
        }
    }

    // If there is a waiting successor and no predecessor.
    if (waiting_succ_flag == true && pred_flag == false)
        pthread_cond_signal(&lock_succ->cond_var);

    free(lock_obj);

    pthread_mutex_unlock(&lock_table_latch);

    return 0;
}

bool lock_exists(int64_t table_id, pagenum_t page_id, int64_t key, int trx_id) {

    hash_id_t hash_id;
    lock_table_entry_t* lock_table_entry;
    lock_t* lock_pred;

    pthread_mutex_lock(&lock_table_latch);

    hash_id = std::make_pair(table_id, page_id);
    // Hash lock table entry.
    lock_table_entry = &lock_table[hash_id];
    // I. there is some entry. Resolve collision.
    if (page_id << 5 == lock_table_entry->page_id) {
        while (page_id != lock_table_entry->page_id) {
            if (lock_table_entry->entry_next == NULL) {
                // If there is no entry corresponding table_id and page_id.
                return false;
            }
            lock_table_entry = lock_table_entry->entry_next;
        }
    }
    // II. there is no entry.
    else {
        return false;
    }

    // Traverse lock table list.
    lock_pred = lock_table_entry->lock_list_head;
    while (lock_pred != NULL) {
        if (lock_pred->key == key) {
            if (lock_pred->owner_trx_id == trx_id && lock_pred->lock_mode == kLockShared)
                continue;
            else
                return true;
        }
        lock_pred = lock_pred->lock_table_next;
    }

    pthread_mutex_unlock(&lock_table_latch);

    return false;
}

int trx_begin(void) {
    pthread_mutex_lock(&trx_table_latch);
    g_trx_id++;
    trx_table[g_trx_id].lock_list_head = NULL;
    trx_table[g_trx_id].lock_list_tail = NULL;
    trx_table[g_trx_id].waiting_trx_id = 0;
    pthread_mutex_unlock(&trx_table_latch);

    log_create(kBegin, g_trx_id);
    // TODO: overflow 나면 어떡하지..? 언제 fail되는 거지?
    return g_trx_id;
}

int trx_commit(int trx_id) {
    lock_t* lock_obj;

    // Clean up the info in the trx_table.
    pthread_mutex_lock(&trx_table_latch);
    while (trx_table[trx_id].lock_list_head != NULL) {
        lock_obj = trx_table[trx_id].lock_list_head;
        
        // Remove the lock_obj from the trx_table.
        trx_table[trx_id].lock_list_head = lock_obj->trx_table_next;

        // Release the lock_obj.
        if (lock_release(lock_obj) != 0)
            return 0;
    }
    trx_table.erase(trx_id);
    pthread_mutex_unlock(&trx_table_latch);

    log_create(kCommit, trx_id);
    // by WAL
    log_flush();
    return trx_id;
}

int trx_abort(int trx_id) {
    int leaf_bufnum;
    node_page_t* leaf_page;
    update_log_t* update_log;
    lock_t* lock_obj;

    pthread_mutex_lock(&trx_table_latch);

    update_log = (update_log_t*)malloc(sizeof(update_log_t));
    if (update_log == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    // Updates canceled.
    while (trx_table[trx_id].queue_LSN.empty() == false) {
        log_get_update_log(update_log, trx_table[trx_id].queue_LSN.front());
        trx_table[trx_id].queue_LSN.pop();

        // Create CLR.
        log_create(kCompensate, trx_id, update_log->table_id, update_log->page_id, update_log->offset, update_log->data_length, update_log->new_img, update_log->old_img, trx_table[trx_id].queue_LSN.front());

        // Undo.
        buffer_request_page(update_log->table_id, update_log->page_id, leaf_page, &leaf_bufnum);
        memcpy(&leaf_page->values[VALUE_OFFSET(update_log->offset)], update_log->old_img, update_log->data_length);
        buffer_release_page(leaf_bufnum, true);
    }

    free(update_log);

    // Trx cleaned up in trx_table
    pthread_mutex_lock(&trx_table_latch);
    while (trx_table[trx_id].lock_list_head != NULL) {
        lock_obj = trx_table[trx_id].lock_list_head;
 
        // Remove the lock_obj from the lock list of trx_table.
        trx_table[trx_id].lock_list_head = lock_obj->trx_table_next;

        // Release the lock_obj.
        if (lock_release(lock_obj) != 0)
            return 0;
    }
    trx_table.erase(trx_id);
    pthread_mutex_unlock(&trx_table_latch);

    log_create(kRollback, trx_id);
    // by WAL
    log_flush();

    pthread_mutex_unlock(&trx_table_latch);

    return trx_id;
}

void trx_push_log_to_queue(int64_t LSN, int trx_id) {
    pthread_mutex_lock(&trx_table_latch);
    trx_table[trx_id].queue_LSN.push(LSN);
    pthread_mutex_unlock(&trx_table_latch);
}
void trx_pop_log_from_queue(int trx_id) {
    pthread_mutex_lock(&trx_table_latch);
    trx_table[trx_id].queue_LSN.pop();
    pthread_mutex_unlock(&trx_table_latch);
}

// API for log management.
void trx_surrect(int trx_id) {
    trx_table[trx_id].lock_list_head = NULL;
    trx_table[trx_id].lock_list_tail = NULL;
    trx_table[trx_id].waiting_trx_id = 0;
}
void trx_kill(int trx_id) {
    lock_t* lock_obj;

    // Trx cleaned up in trx_table
    pthread_mutex_lock(&trx_table_latch);
    while (trx_table[trx_id].lock_list_head != NULL) {
        lock_obj = trx_table[trx_id].lock_list_head;
 
        // Remove the lock_obj from the lock list of trx_table.
        trx_table[trx_id].lock_list_head = lock_obj->trx_table_next;

        // Release the lock_obj.
        lock_release(lock_obj);
    }
    trx_table.erase(trx_id);
    pthread_mutex_unlock(&trx_table_latch);
}
int trx_get_last_LSN(int trx_id) {
    if (trx_table[trx_id].queue_LSN.empty() == true)
        return 0;
    else 
        return trx_table[trx_id].queue_LSN.front();
}

int convert_implicit_to_explicit(int64_t table_id, pagenum_t page_id, int64_t key) {

    int leaf_bufnum;
    node_page_t* leaf_page;
    hash_id_t hash_id;
    lock_table_entry_t* lock_table_entry;
    lock_t* new_lock_obj;
    int slot_index;
    bool existence_flag = false;

    // Get trx_id which modified the page last.
    buffer_request_page(table_id, page_id, leaf_page, &leaf_bufnum);
    for (slot_index = 0; slot_index < leaf_page->header.number_of_keys; slot_index++) 
        if (key == leaf_page->slots[slot_index].key)
            break;
    existence_flag = is_alive_trx(leaf_page->slots[slot_index].trx_id);

    // I. there is no implicit lock. No convert.
    if (existence_flag == false) {
        buffer_release_page(leaf_bufnum, false);
        return 0;
    }
    
    // II. there is a implicit lock. Convert the implicit lock to the explicit lock object.
    hash_id = std::make_pair(table_id, page_id);

    // Hash the lock table entry.
    lock_table_entry = &lock_table[hash_id];
    // I. there is some entry. Resolve collision.
    if (page_id << 5 == lock_table_entry->page_id) {
        while (page_id != lock_table_entry->page_id) {
            if (lock_table_entry->entry_next == NULL) {
                // If there is no entry corresponding table_id and page_id.
                lock_table_entry->entry_next = (lock_table_entry_t*) malloc(sizeof(lock_table_entry_t));
                if (lock_table_entry->entry_next == NULL) {
                    perror("Memory allocation error");
                    return -1;
                }
                lock_table_entry->entry_next->entry_prev = lock_table_entry;
                lock_table_entry = lock_table_entry->entry_next;
                lock_table_entry->entry_next = NULL;
                lock_table_entry->table_id = table_id;
                lock_table_entry->page_id = page_id;
                lock_table_entry->lock_list_head = NULL;
                lock_table_entry->lock_list_tail = NULL;
                break;
            }
            lock_table_entry = lock_table_entry->entry_next;
        }
    }
    // II. there is no entry.
    else {
        lock_table_entry->table_id = table_id;
        lock_table_entry->page_id = page_id;
        lock_table_entry->lock_list_head = NULL;
        lock_table_entry->lock_list_tail = NULL;
        lock_table_entry->entry_prev = NULL;
        lock_table_entry->entry_next = NULL;
    }

    // Create a new lock object.
    new_lock_obj = (lock_t*)malloc(sizeof(lock_t));
    if (new_lock_obj == NULL) {
        perror("Memory allocation error");
        return OP_FAILURE;
    }
    new_lock_obj->key = key;
    new_lock_obj->sentinel = lock_table_entry;
    new_lock_obj->owner_trx_id = leaf_page->slots[slot_index].trx_id;
    new_lock_obj->cond_var = PTHREAD_COND_INITIALIZER;
    new_lock_obj->lock_mode = kLockExclusive;

    buffer_release_page(leaf_bufnum, false);

    // Push the new_lock_obj into trx_table.
    pthread_mutex_lock(&trx_table_latch);
    // I. there is no lock object.
    if (trx_table[new_lock_obj->owner_trx_id].lock_list_head == NULL) {
        new_lock_obj->trx_table_next = NULL;
        trx_table[new_lock_obj->owner_trx_id].lock_list_head = new_lock_obj;
        trx_table[new_lock_obj->owner_trx_id].lock_list_tail = new_lock_obj;
    }
    // II. there is some lock objects.
    else {
        new_lock_obj->trx_table_next = NULL;
        trx_table[new_lock_obj->owner_trx_id].lock_list_tail->trx_table_next = new_lock_obj;
        trx_table[new_lock_obj->owner_trx_id].lock_list_tail = new_lock_obj;
    }
    pthread_mutex_unlock(&trx_table_latch);

    // Push the lock object into the lock table list.
    // I-1. empty lock table list.
    if (lock_table_entry->lock_list_head == NULL) {
        new_lock_obj->lock_table_prev = NULL;
        new_lock_obj->lock_table_next = NULL;
        lock_table_entry->lock_list_head = new_lock_obj;
        lock_table_entry->lock_list_tail = new_lock_obj;
    }
    // I-2. non-empty lock table list.
    else {
        lock_table_entry->lock_list_tail->lock_table_next = new_lock_obj;
        new_lock_obj->lock_table_prev = lock_table_entry->lock_list_tail;
        new_lock_obj->lock_table_next = NULL;
        lock_table_entry->lock_list_tail = new_lock_obj;
    }

    return 1;
}

bool is_alive_trx(int trx_id) {
    return (trx_table.find(trx_id) != trx_table.end());
}

bool detect_deadlock(int trx_id) {
    // Everytime the txn waits for the conflicting operation
    // check whether the lock holder is sleeping or not.
    // If the lock holder is waiting for me, it means cycle, deadlock.
    // 이미 lock_table_latch 얻어진 상태이기 때문에 따로 lock&unlock하지 않아도 될 듯?

    int pred_trx_id = trx_table[trx_id].waiting_trx_id;

    while (is_alive_trx(pred_trx_id)) {
        if (trx_table[pred_trx_id].trx_status == kTrxRunning)
            break;

        if (trx_table[pred_trx_id].waiting_trx_id == trx_id) {
            return true;
        }
        pred_trx_id = trx_table[pred_trx_id].waiting_trx_id;
    }
    return false;
}