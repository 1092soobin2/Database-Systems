#ifndef __LOCK_TABLE_H__
#define __LOCK_TABLE_H__

#include <pthread.h>
#include <stdint.h>

#include <utility>

typedef struct lock_t lock_t;
typedef struct lock_table_entry_t lock_table_entry_t;
typedef std::pair<int64_t, int64_t> hash_id_t;

class Hash {
    public:
        size_t operator() (const hash_id_t &hash_id) const {
            return hash_id.first + (hash_id.second << 5);
        }
};

struct lock_t {
    lock_t* lock_prev;
    lock_t* lock_next;
    lock_table_entry_t* sentinel;
    pthread_cond_t cond_var;
};

struct lock_table_entry_t {
    int64_t table_id;
    int64_t key;
    lock_t* lock_list_head;
    lock_t* lock_list_tail;
    lock_table_entry_t* entry_prev;
    lock_table_entry_t* entry_next;
};

/* APIs for lock table */

// Initialize any data structures
// If success, return 0.
// Otherwise, return non-zero value.
int init_lock_table(void);

// Allocate and append a new lock object.
//  If there is a predecessor, sleep until it to release its lock.
//  If there is no predecessor, return the address of the new lock object.
// If an error occurs, return NULL.
// TODO: when does a error occur?
lock_t* lock_acquire(int64_t table_id, int64_t key);

// Remove the lock_obj from the lock list.
//  If there is a successor, wake up the successor.
// If sucess, return 0. Otherewise, return non-zero value.
int lock_release(lock_t* lock_obj);

#endif /* __LOCK_TABLE_H__ */
