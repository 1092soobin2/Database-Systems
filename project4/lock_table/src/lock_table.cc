#include "lock_table.h"

#include <pthread.h>

#include <unordered_map>
#include <utility>

// Data structres
// Hash table
std::unordered_map<std::pair<int64_t, int64_t>, lock_table_entry_t, Hash> lock_table;
pthread_mutex_t lock_table_mutex;

int init_lock_table(void) {
    lock_table_mutex = PTHREAD_MUTEX_INITIALIZER;
    return 0;
}

lock_t* lock_acquire(int64_t table_id, int64_t key) {

    std::pair<int64_t, int64_t> hash_id;
    lock_t* new_lock_obj;
    lock_table_entry_t* lock_table_entry;

    pthread_mutex_lock(&lock_table_mutex);
    
    hash_id = std::make_pair(table_id, key);
    
    // Create a new lock object.
    new_lock_obj = (lock_t*)malloc(sizeof(lock_t));
    if (new_lock_obj == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    new_lock_obj->sentinel = &(lock_table[hash_id]);
    new_lock_obj->cond_var = PTHREAD_COND_INITIALIZER;

    lock_table_entry = &lock_table[hash_id];
    // I. there is some entry. Resolve collision.
    if (key << 5 == lock_table_entry->key << 5) {
        while (key != lock_table_entry->key) {
            // If there is no lock_table_entry corresponding table_id and key.
            if (lock_table_entry->entry_next == NULL) {
                lock_table_entry->entry_next = (lock_table_entry_t*)malloc(sizeof(lock_table_entry));
                if (lock_table_entry->entry_next == NULL) {
                    perror("Memory allocation error");
                    exit(EXIT_FAILURE);
                }
                lock_table_entry->entry_next->entry_prev = lock_table_entry;
                lock_table_entry = lock_table_entry->entry_next;
                lock_table_entry->key = key;
                lock_table_entry->table_id = table_id;
                lock_table_entry->entry_next = NULL;
                lock_table_entry->lock_list_head = NULL;
                lock_table_entry->lock_list_tail = NULL;
                break;
            }
            lock_table_entry = lock_table_entry->entry_next;
        }
        new_lock_obj->sentinel = lock_table_entry;
    }
    // II. there is no entry.
    else {
        lock_table_entry->table_id = table_id;
        lock_table_entry->key = key;
        lock_table_entry->lock_list_head = NULL;
        lock_table_entry->lock_list_tail = NULL;
        lock_table_entry->entry_prev = NULL;
        lock_table_entry->entry_next = NULL;
    }

    // I. there is no predecessor, acquire lock immediately.
    if (lock_table_entry->lock_list_head == NULL) {
        // Push the lock object into the lock list.
        new_lock_obj->lock_prev = NULL;
        new_lock_obj->lock_next = NULL;
        lock_table_entry->lock_list_head = new_lock_obj;
        lock_table_entry->lock_list_tail = new_lock_obj;
    }
    // II. there is a predecessor.
    else {
        // Push the lock object into the lock list.
        lock_table_entry->lock_list_tail->lock_next = new_lock_obj;
        new_lock_obj->lock_prev = lock_table_entry->lock_list_tail;
        new_lock_obj->lock_next = NULL;
        lock_table_entry->lock_list_tail = new_lock_obj;

        // sleep until it to release its lock.
        pthread_cond_wait(&new_lock_obj->cond_var, &lock_table_mutex);
    }

    pthread_mutex_unlock(&lock_table_mutex);

    return new_lock_obj;
}

int lock_release(lock_t* lock_obj) {
    lock_table_entry_t* lock_list;

    pthread_mutex_lock(&lock_table_mutex);

    lock_list = lock_obj->sentinel;

    // I. there is no successor.
    if (lock_obj->lock_next == NULL) {
        // Remove the lock_obj from the lock list.
        lock_list->lock_list_tail = NULL;
        lock_list->lock_list_head = NULL;

        // Remove the lock_table_entry if not the first.
        if (lock_list->entry_prev != NULL) {
            lock_list->entry_prev->entry_next = lock_list->entry_next;
            if (lock_list->entry_next != NULL)
                lock_list->entry_next->entry_prev = lock_list->entry_prev;
            free(lock_list);
        }
    }
    // II. there is a successor.
    else {
        // Remove the lock_obj from the lock list. Make the successor as head.
        lock_obj->lock_next->lock_prev = NULL;
        lock_list->lock_list_head = lock_obj->lock_next;
        // Wake up the successor.
        pthread_cond_signal(&(lock_obj->lock_next->cond_var));
    }

    free(lock_obj);

    pthread_mutex_unlock(&lock_table_mutex);

    return 0;
}
