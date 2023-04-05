#ifndef __BPT_H__
#define __BPT_H__

#include <string>
#include <unistd.h>

#include "page.h"

// ----------------------------------------------------------------
// ----------------------------------------------------------------
// API to be exported to its upper layer.
// ----------------------------------------------------------------
// ----------------------------------------------------------------

// Open existing data file using pathname or create a new one if not existed.
// If success, return unique table id, which represents the own table in this database.
// If failure, return negative value.
int64_t open_table(char *pathname);

// ----------------------------------------------------------------
// Search
// ----------------------------------------------------------------

// Return the leaf page numnber which may contain the key.
// If tree does not exist, return 0.
// pagenum_t find_leaf_pagenum(table_id_t table_id, pagenum_t root_pagenum, page::key_t key);

// Find key and store matched value and size in ret_val and val_size if key exists.
// If success, return 0. Otherwise, return non-zero value.
// The caller must allocate ret_val and val_size.
// int db_find(int64_t table_id, int64_t key, char *ret_val, uint16_t *val_size);

// ----------------------------------------------------------------
// Insertion
// ----------------------------------------------------------------

slot_t make_slot(page::key_t key, uint16_t size);
// Make a new leaf page or internal page.
pagenum_t make_node_page(int is_leaf);

// Insert the new slot and value into the leaf page.
int insert_into_leaf_page(table_id_t table_id, pagenum_t leaf_pagenum, slot_t slot, char* value);
int insert_into_leaf_page_after_splitting(table_id_t table_id, pagenum_t leaf_pagenum, slot_t slot, char* value);

// Insert the new branch into the internal page.
int insert_into_internal_page(table_id_t table_id, pagenum_t internal_pagenum, page::key_t new_right_key, pagenum_t new_branch_pagenum);
int insert_into_internal_page_after_splitting(table_id_t table_id, pagenum_t internal_pagenum, page::key_t new_right_key, pagenum_t new_branch_pagenum);

// Insert the new branch into the parent page.
int insert_into_parent_page(table_id_t table_id, pagenum_t branch_pagenum, page::key_t new_right_key, pagenum_t new_branch_pagenum);

// Insert into internal root page.
int insert_into_new_root_page(table_id_t table_id, pagenum_t branch_pagenum, page::key_t new_right_key, pagenum_t new_branch_pagenum);
// Insert into leaf root page.
int start_new_page_tree(table_id_t table_id, slot_t slot, char* value);

// Insert key and value to data file at the right place.
// If success, return 0. Otherwise, return non-zero value.
int db_insert(int64_t table_id, int64_t key, char* value, uint16_t val_size);

// ----------------------------------------------------------------
// Deletion
// ----------------------------------------------------------------
// Remove the slot containing the key and pointed value in the leaf page.
int remove_entry_from_leaf_page(table_id_t table_id, pagenum_t leaf_pagenum, page::key_t key);
// Remove the branch containing the key in the internal page. (coresspoding pagenum is alwarys in the branch containing the key. See coalescense function.)
int remove_entry_from_internal_page(table_id_t table_id, pagenum_t internal_pagenum, page::key_t key);

int adjust_root_page(table_id_t table_id, pagenum_t root_pagenum);

// Coalesce a too small page with a neighbor page
// which can accept the additional entries
// without exceeding the maximum.
int coalesce_leaf_pages(table_id_t table_id, pagenum_t leaf_pagenum, pagenum_t neighbor_pagenum, page::key_t key_between_two);
int coalesce_internal_pages(table_id_t table_id, pagenum_t internal_pagenum, pagenum_t neighbor_pagenum, page::key_t key_between_two);

// Redistribute between a too small page and a neighbor page
// which is too big to app\end the small page
// without exceeding the maximum.
// Move some entries from the neighbor page to the leaf page.
int redistribute_leaf_pages(table_id_t table_id, pagenum_t leaf_pagenum, pagenum_t neighbor_pagenum, int branch_index_between_two);
// Move a entry from the nieghbor page to the internal page.
int redistribute_internal_pages(table_id_t table_id, pagenum_t internal_pagenum, pagenum_t neighbor_pageum, int branch_index_between_two);

// Delete the key in the page.
int delete_entry_in_page(table_id_t table_id, pagenum_t pagenum, int is_leaf, page::key_t key);

// Find key and delete it if found.
// If success, return 0. Otherwise, return non-zero value.
int db_delete(int64_t table_id, int64_t key);

int init_db(void);
int shutdown_db(void);

// Utility
void print_page(table_id_t table_id, pagenum_t pagenum);
int64_t get_root_pagenum(int64_t table_id);

// Project5 API
pagenum_t find_leaf_pagenum(table_id_t table_id, pagenum_t root_pagenum, page::key_t key, int trx_id);
// Read a value in the table with a matching key for the txn have trx_id.
// If success, operation is successfully done, and the txn can ccountinue the next operation.
// If failed, return non-zero.
// Operation is failed (e.g., deadlock detected), and the txn should be aborted.
// Note that all tasks that need to be handled. (e.g., releasing the vlocks that are held by this txn, rollback of previous operations, etc.) should be completed in db_find().
// TODO: 아... rollback을 따로 구현하는 게 아니었어...?
int db_find(int64_t table_id, int64_t key, char* ret_val, uint16_t *val_size, int trx_id);

// Find the matching key and modify the values.
// If found, update the value of the record to 'values' with its 'new_val_size' and store its size in 'old_val_size'.
// If success, return 0. 
// Operation is successfully done, and the transaction can countinue the next operation.
// If failed, return non-zero.
// Operation is failed (e.g., deadlock detected), and the txn should be aborted.
// Note that all tasks taht need to be handled (e.g., releasing the locks that are held on this txn, rollback of previous operations, etc.) should be completed in db_update().
int db_update(int64_t table_id, int64_t key, char* values, uint16_t new_val_szie, uint16_t* old_val_size, int trx_id);

#endif /* __BPT_H__*/
