#include "bpt.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "page.h"
#include "file.h"
#include "buffer.h"

bool verbose;
bool verbose2;

// Open existing data file using pathname or create a new one if not existed.
// If success, return unique table id, which represents the own table in this database.
// If failure, return negative value.
int64_t open_table (char *pathname) {
    int64_t table_id;

    table_id = file_open_table_file(pathname);
    
    if (table_id < 0)
        return OP_FAILURE;
    else 
        return table_id;
}


// Search 

// Return the leaf page number which may contain the key.
// If tree does not exist, return 0.
pagenum_t find_leaf_pagenum(table_id_t table_id, pagenum_t root_pagenum, page::key_t key) {
    int branch_index;
    int node_bufnum;
    node_page_t* node_page;
    pagenum_t node_pagenum = root_pagenum;

    if (verbose)
        printf(" |find_leaf_pagenum");

    // Tree does not exist.
    if (root_pagenum == 0)
        return 0;
    
    // Request the root page.
    buffer_request_page(table_id, root_pagenum, node_page, &node_bufnum);
    
    // Find the leaf page which may contain the key.
    while (node_page->header.is_leaf == 0) {
        // Find the child page number which may contain the key.
        for (branch_index = 0; branch_index < node_page->header.number_of_keys; branch_index++) {
            if (key < node_page->branchs[branch_index].key) 
                break;
        }
        branch_index--;
        if (branch_index == -1)
            node_pagenum = node_page->header.branch_first_pagenum;
        else
            node_pagenum = node_page->branchs[branch_index].pagenum;
        
        // Release the page.
        buffer_release_page(node_bufnum, false);

        // Request the child page.
        buffer_request_page(table_id, node_pagenum, node_page, &node_bufnum);

    }

    // Release the leaf page.
    buffer_release_page(node_bufnum, false);
    
    if (verbose) {
        printf("| ");
    }
    // Return the leaf page number.
    return node_pagenum;
}
// Find the key and store matched value and size in ret_val and val_size if the key exists.
// If success, return 0. Otherwise, return non-zero vlaue.
// The caller must allocate ret_val and val_size.
int db_find(table_id_t table_id, page::key_t key, char *ret_val, uint16_t *val_size) {

    int slot_index;
    int header_bufnum;
    header_page_t *header_page;
    pagenum_t root_pagenum, leaf_pagenum;
    int leaf_bufnum;
    node_page_t *leaf_page;
    bool existence_flag = false;

    if (verbose) {
        printf("|db_find %lld ", key);;
    }

    // Check the table_id is valid.
    if (file_is_valid_table_id(table_id) == false)
        return OP_FAILURE;

    // Get the root_pagenum.
    buffer_request_page(table_id, 0, header_page, &header_bufnum);
    root_pagenum = header_page->root_pagenum;
    buffer_release_page(header_bufnum, false);

    // Find the leaf page number which may contain the key in the tree.
    leaf_pagenum = find_leaf_pagenum(table_id, root_pagenum, key);
    
    // Tree does not exist.
    if (leaf_pagenum == 0)
        return OP_FAILURE;

    if (verbose) {
        print_page(table_id, leaf_pagenum);
    }

    // Request the leaf page.
    buffer_request_page(table_id, leaf_pagenum, leaf_page, &leaf_bufnum);

    // Find the key in the leaf page.
    for (slot_index = 0; slot_index < leaf_page->header.number_of_keys; slot_index++) {
        // Store the value and size, if exist.
        if (leaf_page->slots[slot_index].key == key) {
            existence_flag = true;
            memcpy(ret_val, &(leaf_page->values[VALUE_OFFSET(leaf_page->slots[slot_index].offset)]), leaf_page->slots[slot_index].size);
            *val_size = leaf_page->slots[slot_index].size;
            break;
        }
    }
    
    // Release the leaf page.
    buffer_release_page(leaf_bufnum, false);
    
    if (verbose) {
        printf("|");
    }

    if (existence_flag)
        return OP_SUCCESS;
    else
        return OP_FAILURE;
}

// Insertion
slot_t make_slot(page::key_t key, uint16_t size) {
    slot_t new_slot;
    new_slot.key = key;
    new_slot.size = size;

    return new_slot;
}

pagenum_t make_node_page(table_id_t table_id, int is_leaf) {
    
    pagenum_t new_pagenum;
    int new_bufnum;
    node_page_t* new_page;
    

    if (verbose) {
        printf(" |make_node_page ");
    }
    // Allocate the new page.
    new_pagenum = buffer_alloc_page(table_id);

    // Request the new page.
    buffer_request_page(table_id, new_pagenum, new_page, &new_bufnum);

    // Initialize metadata in the page.
    new_page->header.is_leaf = is_leaf;
    new_page->header.number_of_keys = 0;
    new_page->header.amount_of_free_space = INITIAL_FREE_SPACE;

    // Release the new page. Write.
    buffer_release_page(new_bufnum, true);

    if (verbose)
        printf("| ");
    return new_pagenum;
}

int insert_into_leaf_page(table_id_t table_id, pagenum_t leaf_pagenum, slot_t slot, char* value) {
    
    int leaf_bufnum;
    node_page_t* leaf_page;
    int slot_index, insertion_index;

    if (verbose) {
        printf("\n|insert_into_leaf_page (%lld, %d) in %lld", slot.key, slot.size, leaf_pagenum);
        printf("\n\tbefore insertion ");
        print_page(table_id, leaf_pagenum);
    }
    
    // Request the leaf page.
    buffer_request_page(table_id, leaf_pagenum, leaf_page, &leaf_bufnum);

    // Find the insertion index.
    // The first index where new key is smaller than key in the leaf page.
    for (insertion_index = 0; insertion_index < leaf_page->header.number_of_keys; insertion_index++)
        if (slot.key < leaf_page->slots[insertion_index].key)
            break;
    
    // Make room for insertion. Move all data behind the insertion index.
    for (slot_index = leaf_page->header.number_of_keys; slot_index > insertion_index; slot_index--)
        leaf_page->slots[slot_index] = leaf_page->slots[slot_index - 1];

    // Insert the slot and value at the insertion index. 
    slot.offset = PAGE_HEADER_SIZE + SLOT_SIZE * leaf_page->header.number_of_keys + leaf_page->header.amount_of_free_space - slot.size;
    leaf_page->slots[insertion_index] = slot;
    memcpy(&leaf_page->values[VALUE_OFFSET(slot.offset)], value, slot.size);
    // Modify the metadata in leaf_page.
    leaf_page->header.number_of_keys++;
    leaf_page->header.amount_of_free_space -= (SLOT_SIZE + slot.size);

    // Release the leaf page. Write.
    buffer_release_page(leaf_bufnum, true);

    if (verbose) {
        printf("\n\tafter insertion ");
        print_page(table_id, leaf_pagenum);
        printf("|");
    }

    return OP_SUCCESS;
}

int insert_into_leaf_page_after_splitting(table_id_t table_id, pagenum_t leaf_pagenum, slot_t slot, char* value) {

    pagenum_t new_leaf_pagenum;
    int leaf_bufnum, new_leaf_bufnum;
    node_page_t *leaf_page, *new_leaf_page;
    slot_t* temp_slots;
    slot_t temp_slot;
    int number_of_temp_slots;
    char temp_values[INITIAL_FREE_SPACE];
    int split_index, insertion_index, slot_index, temp_slot_index;
    int occupied_space = 0;
    int new_key;
    
    if (verbose) {
        printf("\n|insert_into_leaf_page_after_splitting (%lld, %d) in %lld", slot.key, slot.size, leaf_pagenum);
        printf("\n\tbefore insertion");
        print_page(table_id, leaf_pagenum);
    }

    // Request the leaf page.
    buffer_request_page(table_id, leaf_pagenum, leaf_page, &leaf_bufnum);

    // Find the insertion index,
    // which is the first index where new key is smaller than key in the leaf page.
    for (insertion_index = 0; insertion_index < leaf_page->header.number_of_keys; insertion_index++)
        if (slot.key < leaf_page->slots[insertion_index].key)
            break;
    if (verbose) {
        printf("(insertion_index:%d)", insertion_index);
    }
    
    // Create a temporary array of slots,
    // which of length is longer as 1 number_of_keys in leaf_page.
    number_of_temp_slots = leaf_page->header.number_of_keys + 1;
    temp_slots = (slot_t*)malloc(number_of_temp_slots * SLOT_SIZE);
    if (temp_slots == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    // Move leaf_page->values into temp_values.
    memcpy(&temp_values[0], &leaf_page->values[0], INITIAL_FREE_SPACE);
    // Move leaf_page->slots into temp_slots. Hop at insertion_index.
    for (slot_index = 0, temp_slot_index = 0; slot_index < leaf_page->header.number_of_keys; slot_index++, temp_slot_index++) {
        if (temp_slot_index == insertion_index)
            temp_slot_index++;
        temp_slots[temp_slot_index] = leaf_page->slots[slot_index];
    }
    // Insert the new slot and value into the temporary arrays.
    slot.offset = PAGE_HEADER_SIZE + SLOT_SIZE * leaf_page->header.number_of_keys + leaf_page->header.amount_of_free_space - slot.size;
    temp_slots[insertion_index] = slot;
    memcpy(&temp_values[VALUE_OFFSET(slot.offset)], value, slot.size);

    // Modify the metadata in leaf page.
    leaf_page->header.number_of_keys = 0;
    leaf_page->header.amount_of_free_space = INITIAL_FREE_SPACE;
    memset(&leaf_page->values[0], 0, INITIAL_FREE_SPACE);

    // Find the split index of temp_slots.
    for (split_index = 0; split_index < number_of_temp_slots; split_index++) {
        occupied_space += temp_slots[split_index].size;
        if (occupied_space >= INITIAL_FREE_SPACE / 2)
            break;
    }
    if (verbose) {
        printf("(occupied_space:%d, split_index:%d)", occupied_space, split_index);
    }

    // Move the first half into leaf_page.
    for (temp_slot_index = 0; temp_slot_index < split_index; temp_slot_index++) {
        temp_slot = temp_slots[temp_slot_index];
        // Append temp_slot and pointed value.
        temp_slot.offset = PAGE_HEADER_SIZE + SLOT_SIZE * leaf_page->header.number_of_keys + leaf_page->header.amount_of_free_space - temp_slot.size;
        leaf_page->slots[temp_slot_index] = temp_slot;
        memcpy(&leaf_page->values[VALUE_OFFSET(temp_slot.offset)], &temp_values[VALUE_OFFSET(temp_slots[temp_slot_index].offset)], temp_slot.size);
        // Modify the metadata.
        leaf_page->header.number_of_keys++;
        leaf_page->header.amount_of_free_space -= (SLOT_SIZE + temp_slot.size);
    }
    
    // Create a new leaf page.
    new_leaf_pagenum = make_node_page(table_id, 1);
    // Request the new leaf page.
    buffer_request_page(table_id, new_leaf_pagenum, new_leaf_page, &new_leaf_bufnum);

    // Set the metadata (right sibling and parent).
    new_leaf_page->header.right_sibling_pagenum = leaf_page->header.right_sibling_pagenum;
    leaf_page->header.right_sibling_pagenum = new_leaf_pagenum;
    new_leaf_page->header.parent_pagenum = leaf_page->header.parent_pagenum;

    // Move the second half into new_leaf_page.
    for (temp_slot_index = split_index, slot_index = 0; temp_slot_index < number_of_temp_slots; temp_slot_index++, slot_index++) {
        temp_slot = temp_slots[temp_slot_index];
        // Append temp_slot and pointed value.
        temp_slot.offset = PAGE_HEADER_SIZE + SLOT_SIZE * new_leaf_page->header.number_of_keys + new_leaf_page->header.amount_of_free_space - temp_slot.size;
        new_leaf_page->slots[slot_index] = temp_slot;
        memcpy(&new_leaf_page->values[VALUE_OFFSET(temp_slot.offset)], &temp_values[VALUE_OFFSET(temp_slots[temp_slot_index].offset)], temp_slot.size);
        // Modify the metadata.
        new_leaf_page->header.number_of_keys++;
        new_leaf_page->header.amount_of_free_space -= (SLOT_SIZE + temp_slot.size);
    }

    // Deallocate the temp_slots.
    free(temp_slots);

    // Get the new key to be inserted into parent.
    new_key = new_leaf_page->slots[0].key;
    
    // Release the pages. Write the pages.
    buffer_release_page(leaf_bufnum, true);
    buffer_release_page(new_leaf_bufnum, true);

    if (verbose) {
        printf("\n\tafter insertion");
        print_page(table_id, leaf_pagenum);
        print_page(table_id, new_leaf_pagenum);
        printf("->");
    }
    // Insert new_right_key and new_leaf_pagenum into parent.
    return insert_into_parent_page(table_id, leaf_pagenum, new_key, new_leaf_pagenum);
}

int insert_into_internal_page(table_id_t table_id, pagenum_t internal_pagenum, page::key_t new_right_key, pagenum_t new_branch_pagenum) {

    int internal_bufnum;
    node_page_t* internal_page;
    int branch_index, insertion_index;

    if (verbose) {
        printf("\n|insert_into_internal_page(%lld, %lld) in page %lld", new_right_key, new_branch_pagenum, internal_pagenum);
    }
    
    // Request the internal page.
    buffer_request_page(table_id, internal_pagenum, internal_page, &internal_bufnum);

    // Find the insertion_index.
    // The first index where new key is smaller than key in the internal page.
    for (insertion_index = 0; insertion_index < internal_page->header.number_of_keys; insertion_index++) 
        if (new_right_key < internal_page->branchs[insertion_index].key)
            break;

    // Move all data behind the insertion index to make room for insertion.
    for (branch_index = internal_page->header.number_of_keys; branch_index > insertion_index; branch_index--)
        internal_page->branchs[branch_index] = internal_page->branchs[branch_index - 1];
    
    // Insert the new key and pagenum.
    internal_page->branchs[insertion_index].key = new_right_key;
    internal_page->branchs[insertion_index].pagenum = new_branch_pagenum;
    // Modify the metadata in internal_page.
    internal_page->header.number_of_keys++;

    // Release the page. Write the page.
    buffer_release_page(internal_bufnum,true);

    if (verbose) {
        printf("\tafter insertion");
        print_page(table_id, internal_pagenum);
        printf("|");
    }

    return OP_SUCCESS;
}

int insert_into_internal_page_after_splitting(table_id_t table_id, pagenum_t internal_pagenum, page::key_t new_right_key, pagenum_t new_branch_pagenum) {

    pagenum_t new_internal_pagenum;
    int internal_bufnum, new_internal_bufnum;
    node_page_t *internal_page, *new_internal_page;
    branch_t* temp_branchs;
    int branch_index, insertion_index, split_index, temp_branch_index;
    int new_key;
    int branch_bufnum;
    node_page_t *branch_page;

    if (verbose)
        printf("\n|insert_into_internal_page_after_splitting (%lld, %lld) in page %lld", new_right_key, new_branch_pagenum, internal_pagenum);

    // Request the internal page.
    buffer_request_page(table_id, internal_pagenum, internal_page, &internal_bufnum);

    // Create a temporary array of branchs,
    // which of length is longer as 1 number_of_keys in internal_page.
    temp_branchs = (branch_t*)malloc(ORDER * sizeof(branch_t));
    if (temp_branchs == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    // Find the insertion index.
    // The first index where new key is smaller than key in the internal page.
    for (insertion_index = 0; insertion_index < internal_page->header.number_of_keys; insertion_index++) 
        if (new_right_key < internal_page->branchs[insertion_index].key)
            break;

    // Move internal_page->branchs into temp_branchs. Hop at insertion_index.
    // branch_first_pagenum would not be moved, so does not need to be changed.
    for (branch_index = 0, temp_branch_index = 0; branch_index < internal_page->header.number_of_keys; branch_index++, temp_branch_index++) {
        if (temp_branch_index == insertion_index)
            temp_branch_index++;
        temp_branchs[temp_branch_index] = internal_page->branchs[branch_index];
    }
    // Insert the new key and pagenum at the insertion index.
    temp_branchs[insertion_index].key = new_right_key;
    temp_branchs[insertion_index].pagenum = new_branch_pagenum;
    // Modify the metadata in internal page.
    internal_page->header.number_of_keys = 0;

    // Find the split index of temp_branchs.
    // half of max number of keys. (ORDER-1)
    split_index = ((ORDER-1) % 2 == 0? (ORDER-1)/2 : (ORDER-1)/2 + 1);

    // Move the first half into internal_page.
    for (temp_branch_index = 0; temp_branch_index < split_index - 1; temp_branch_index++) {
        // Append temp_branchs.
        internal_page->branchs[temp_branch_index] = temp_branchs[temp_branch_index];
        // Modify the metadata.
        internal_page->header.number_of_keys++;
    }

    // Create a new internal page.
    new_internal_pagenum = make_node_page(table_id, 0);
    // Request the new internal page.
    buffer_request_page(table_id, new_internal_pagenum, new_internal_page, &new_internal_bufnum);

    // Set the metadata.
    new_internal_page->header.parent_pagenum = internal_page->header.parent_pagenum;
    
    // Get new_key to be inserted into parent.
    new_key = temp_branchs[split_index - 1].key;
    // Move the branch_first_pagenum into new_internal_page.
    new_internal_page->header.branch_first_pagenum = temp_branchs[split_index - 1].pagenum;

    // Move the second half into new_internal_page.
    for (temp_branch_index = split_index, branch_index = 0; temp_branch_index < ORDER; temp_branch_index++, branch_index++) {
        // Append temp_branchs.
        new_internal_page->branchs[branch_index] = temp_branchs[temp_branch_index];
        // Modify the metadata.
        new_internal_page->header.number_of_keys++;
    }

    // Deallocate the temp_branchs.
    free(temp_branchs);

    // Update the parent pagent number of moved branch pages. (Read, modify, write)
    buffer_request_page(table_id, new_internal_page->header.branch_first_pagenum, branch_page, &branch_bufnum);
    branch_page->header.parent_pagenum = new_internal_pagenum;
    buffer_release_page(branch_bufnum, true);
    for (branch_index = 0; branch_index < new_internal_page->header.number_of_keys; branch_index++) {
        // Update the metadata of moved branch pages. (Read, modify, write)
        buffer_request_page(table_id, new_internal_page->branchs[branch_index].pagenum, branch_page, &branch_bufnum);
        branch_page->header.parent_pagenum = new_internal_pagenum;
        buffer_release_page(branch_bufnum, true);
    }

    // Release the pages. Write the pages.
    buffer_release_page(internal_bufnum, true);
    buffer_release_page(new_internal_bufnum, true);

    if (verbose) {
        print_page(table_id, internal_pagenum);
        print_page(table_id, new_internal_pagenum);
        printf("->");
    }
    // Insert new_right_key and new_internal_pagenum into parent.
    return insert_into_parent_page(table_id, internal_pagenum, new_key, new_internal_pagenum);
}

int insert_into_parent_page(table_id_t table_id, pagenum_t branch_pagenum, page::key_t new_right_key, pagenum_t new_branch_pagenum) {

    pagenum_t parent_pagenum;
    int branch_bufnum, parent_bufnum;
    node_page_t *branch_page, *parent_page;
    bool split_flag = false;

    if (verbose)
        printf("\n|insert_into_parent_page->");

    // Get the parent page number in the branch page.
    buffer_request_page(table_id, branch_pagenum, branch_page, &branch_bufnum);
    parent_pagenum = branch_page->header.parent_pagenum;
    buffer_release_page(branch_bufnum, false);

    // I. new_branch is root.
    if (parent_pagenum == 0) {
        return insert_into_new_root_page(table_id, branch_pagenum, new_right_key, new_branch_pagenum);
    }

    // Get the split flag in the parent page
    // Check if the parent page is full.
    buffer_request_page(table_id, parent_pagenum, parent_page, &parent_bufnum);
    if (parent_page->header.number_of_keys == ORDER - 1)
        split_flag = true;
    buffer_release_page(parent_bufnum, false);

    // II. Parent has room for insertion.
    if (split_flag == false)
        return insert_into_internal_page(table_id, parent_pagenum, new_right_key, new_branch_pagenum);
    // III. Parent does not have room for insertion.
    else   
        return insert_into_internal_page_after_splitting(table_id, parent_pagenum, new_right_key, new_branch_pagenum);

}

int insert_into_new_root_page(table_id_t table_id, pagenum_t branch_pagenum, page::key_t new_right_key, pagenum_t new_branch_pagenum) {

    pagenum_t root_pagenum;
    int root_bufnum, branch_bufnum, header_bufnum;
    node_page_t *root_page;
    node_page_t *branch_page;
    header_page_t * header_page;

    if (verbose) {
        printf("\n|insert_into_new_root_page");
        print_page(table_id, branch_pagenum);
        print_page(table_id, new_branch_pagenum);
    }

    // Make a new root page which is a internal page.
    root_pagenum = make_node_page(table_id, 0);

    // Request the root page.
    buffer_request_page(table_id, root_pagenum, root_page, &root_bufnum);

    // Set the metadata.
    root_page->header.parent_pagenum = 0;

    // Insert the branchs.
    root_page->header.branch_first_pagenum = branch_pagenum;
    root_page->branchs[0].key = new_right_key;
    root_page->branchs[0].pagenum = new_branch_pagenum;
    // Modify the metadata in the root page.
    root_page->header.number_of_keys++;

    // Release the root page. Write the root page.
    buffer_release_page(root_bufnum, true);

    // Update parent_pagenum of moved branchs. (Read, modify, write)
    buffer_request_page(table_id, branch_pagenum, branch_page, &branch_bufnum);
    branch_page->header.parent_pagenum = root_pagenum;
    buffer_release_page(branch_bufnum, true);

    buffer_request_page(table_id, new_branch_pagenum, branch_page, &branch_bufnum);
    branch_page->header.parent_pagenum = root_pagenum;
    buffer_release_page(branch_bufnum, true);

    // Update the metadata in header_page. (Read, modify, write)
    buffer_request_page(table_id, 0, header_page, &header_bufnum);
    header_page->root_pagenum = root_pagenum;
    buffer_release_page(header_bufnum, true);

    if (verbose) {
        printf("\tafter insertion ");
        print_page(table_id, root_pagenum);
        print_page(table_id, branch_pagenum);
        print_page(table_id, new_branch_pagenum);
        printf("|");
    }

    return OP_SUCCESS;    
}

int start_new_page_tree(table_id_t table_id, slot_t slot, char* value) {

    pagenum_t root_pagenum;
    int root_bufnum, header_bufnum;
    node_page_t* root_page;
    header_page_t* header_page;
    
    if(verbose)
        printf(" |start_new_page_tree");

    // Make a new root page which is a leaf page.
    root_pagenum = make_node_page(table_id, 1);

    // Request the root page.
    buffer_request_page(table_id, root_pagenum, root_page, &root_bufnum);

    // Set the metadata (parent and right sibling).
    root_page->header.parent_pagenum = 0;
    root_page->header.right_sibling_pagenum = 0;

    // Insert the slot and value.
    slot.offset = PAGE_HEADER_SIZE + INITIAL_FREE_SPACE - slot.size;
    root_page->slots[0] = slot;
    memcpy(&root_page->values[VALUE_OFFSET(slot.offset)], value, slot.size);
    // Modify the metadata.
    root_page->header.number_of_keys++;
    root_page->header.amount_of_free_space -= (SLOT_SIZE + slot.size);

    // Release the root page. Write the root page.
    buffer_release_page(root_bufnum, true);

    // Update the metadata in header_page. (Read, modify, write)
    buffer_request_page(table_id, 0, header_page, &header_bufnum);
    header_page->root_pagenum = root_pagenum;
    buffer_release_page(header_bufnum, true);
    
    if (verbose) {
        printf("\n\tafter insertion");
        print_page(table_id, root_pagenum);
        printf("|");
    }

    return OP_SUCCESS;
}

int db_insert(int64_t table_id, int64_t key, char* value, uint16_t val_size) {

    slot_t slot;
    pagenum_t root_pagenum, leaf_pagenum;
    int header_bufnum, leaf_bufnum;
    header_page_t* header_page;
    node_page_t* leaf_page;
    int slot_index;
    bool split_flag = false;

    if (verbose) {
        printf("|db_insert");
    }
    // Check the table_id is valid.
    if (file_is_valid_table_id(table_id) == false)
        return OP_FAILURE;

    // Get the root page number in header_page.
    buffer_request_page(table_id, 0, header_page, &header_bufnum);
    root_pagenum = header_page->root_pagenum;
    buffer_release_page(header_bufnum, false);
    
    // Make a new slot.
    slot = make_slot(key, val_size);

    // Get the leaf page number which may contain the key.
    leaf_pagenum = find_leaf_pagenum(table_id, root_pagenum, key);
    
    // I. the tree does not exist.
    if (leaf_pagenum == 0) {
        if (verbose) {
            printf("->");
        }
        return start_new_page_tree(table_id, slot, value);
    }
    
    // II. the tree exists.
    // Request the leaf page.
    buffer_request_page(table_id ,leaf_pagenum, leaf_page, &leaf_bufnum);

    if (verbose) {
        printf("(num_keys:%d, free: %d)", leaf_page->header.number_of_keys, leaf_page->header.amount_of_free_space);
    }
    // Find the key in the leaf page.
    for (slot_index = 0; slot_index < leaf_page->header.number_of_keys; slot_index++)
        // Ignore duplicates if the key exists in the leaf page.
        if (key == leaf_page->slots[slot_index].key) {
            buffer_release_page(leaf_bufnum, false);
            return OP_FAILURE;
        }

    // Set the split flag if the leaf page does not have enough room.
    if (leaf_page->header.amount_of_free_space < (SLOT_SIZE + val_size))
        split_flag = true;

    // Release the leaf page.
    buffer_release_page(leaf_bufnum, false);

    // II-1. leaf has room for insertion.
    if (split_flag == false)
        return insert_into_leaf_page(table_id, leaf_pagenum, slot, value);
    // II-2. leaf does not have room for insertion.
    else   
        return insert_into_leaf_page_after_splitting(table_id, leaf_pagenum, slot, value);
}

// Deletion
int remove_entry_from_leaf_page(table_id_t table_id, pagenum_t leaf_pagenum, page::key_t key) {
    
    int deletion_index, slot_index;
    int leaf_bufnum;
    node_page_t* leaf_page;
    uint16_t min_offset;
    slot_t removed_slot;

    if (verbose) {
        printf(" | remove_entry_from_leaf_page ");
    }
    // Request the leaf page.
    buffer_request_page(table_id, leaf_pagenum, leaf_page, &leaf_bufnum);

    // Find the deletion index of the slot containing the key.
    for (deletion_index = 0; deletion_index < leaf_page->header.number_of_keys; deletion_index++)
        if (key == leaf_page->slots[deletion_index].key) {
            removed_slot = leaf_page->slots[deletion_index];
            break;
        }
    
    // Remove the slot. Move all data behind the deletion index.
    for (slot_index = deletion_index + 1; slot_index < leaf_page->header.number_of_keys; slot_index++) 
        leaf_page->slots[slot_index - 1] = leaf_page->slots[slot_index];
    // Modify the metadata in leaf_page.
    leaf_page->header.number_of_keys--;
    leaf_page->header.amount_of_free_space += (SLOT_SIZE + removed_slot.size);

    // Remove the pointed value. Compact the leaf page.
    min_offset = PAGE_HEADER_SIZE + leaf_page->header.amount_of_free_space + SLOT_SIZE * leaf_page->header.number_of_keys - removed_slot.size;
    memmove(&leaf_page->values[min_offset + removed_slot.size - PAGE_HEADER_SIZE], &leaf_page->values[min_offset - PAGE_HEADER_SIZE], removed_slot.offset - min_offset);
    // Modify the offset of compacted values in the slot.
    for (slot_index = 0; slot_index < leaf_page->header.number_of_keys; slot_index++)
        if (leaf_page->slots[slot_index].offset < removed_slot.offset)
            leaf_page->slots[slot_index].offset += removed_slot.size;
    // Reset the compacted values for tidiness.
    memset(&leaf_page->values[min_offset - PAGE_HEADER_SIZE], 0, removed_slot.size);

    // Release the leaf page. Write the leaf page.
    buffer_release_page(leaf_bufnum, true);

    if (verbose) {
        printf("|");
    }
    return OP_SUCCESS;
}

int remove_entry_from_internal_page(table_id_t table_id, pagenum_t internal_pagenum, page::key_t key) {
    
    int deletion_index, branch_index;
    int internal_bufnum;
    node_page_t* internal_page;

    if (verbose) {
        printf(" | remove_entry_from_internal_page ");
    }

    // Reqest the internal page.
    buffer_request_page(table_id, internal_pagenum, internal_page, &internal_bufnum);

    // Find the deltion index of the branch containing the key.
    for (deletion_index = 0; deletion_index < internal_page->header.number_of_keys; deletion_index++)
        if (key == internal_page->branchs[deletion_index].key)
            break;
    
    // TODO: maybe there will be change of this implementation.. one argu will be added: branch_pagenum
    // Remove the branch. Move all data behind the deletion index.
    for (branch_index = deletion_index + 1; branch_index < internal_page->header.number_of_keys; branch_index++)
        internal_page->branchs[branch_index - 1] = internal_page->branchs[branch_index];
    // Modify the metadata in internal_page.
    internal_page->header.number_of_keys--;

    // Release the page. Write the internal page.
    buffer_release_page(internal_bufnum, true);
    
    if (verbose) {
        printf("|");
    }

    return OP_SUCCESS;
}

int adjust_root_page(table_id_t table_id, pagenum_t root_pagenum) {

    int root_bufnum, new_root_bufnum, header_bufnum;
    node_page_t* root_page, *new_root_page;
    header_page_t* header_page;
    pagenum_t new_root_pagenum = 0;

    if (verbose) {
        printf(" |adjust root page ");
        printf("\n\t before ");
        print_page(table_id, root_pagenum);
    }

    // Request the root page.
    buffer_request_page(table_id, root_pagenum, root_page, &root_bufnum);

    // I. nonempty root.
    if (root_page->header.number_of_keys > 0) {
        // Release the root page.
        buffer_release_page(root_bufnum, false);
        return OP_SUCCESS;
    }


    // II. empty root, it has one branch page.
    // Promote the only branch page as a new root page.
    if (root_page->header.is_leaf == 0) {

        // Get the new root page number.
        new_root_pagenum = root_page->header.branch_first_pagenum;
        // Release and free the root page.
        buffer_release_page(root_bufnum, false);
        buffer_free_page(table_id, root_pagenum);

        // Update the metadata in the header page. (Read, modify, write)
        buffer_request_page(table_id, 0, header_page, &header_bufnum);
        header_page->root_pagenum = new_root_pagenum;
        buffer_release_page(header_bufnum, true);

        // Update the metadata in the new root_page. (Read, modify, write)
        buffer_request_page(table_id, new_root_pagenum, new_root_page, &new_root_bufnum);
        new_root_page->header.parent_pagenum = 0;
        buffer_release_page(new_root_bufnum, true);

    }
    // III. empty root, it is leaf (has no child).
    // Tree is empty.
    else {

        // Release and free the root page.
        buffer_release_page(root_bufnum, false);
        buffer_free_page(table_id, root_pagenum);

        // Update the metadata in the header pag e. (Read, modify, write)
        buffer_request_page(table_id, 0, header_page, &header_bufnum);
        header_page->root_pagenum = 0;
        buffer_release_page(header_bufnum, true);

    }

    if (verbose) {
        printf("\n\t after ");
        if (new_root_pagenum != 0)
            print_page(table_id, new_root_pagenum);
        printf("|");
    }
    return OP_SUCCESS;
}

int coalesce_leaf_pages(table_id_t table_id, pagenum_t leaf_pagenum, pagenum_t neighbor_pagenum, page::key_t key_between_two) {

    int leaf_bufnum, neighbor_bufnum, temp_bufnum;
    node_page_t *leaf_page, *neighbor_page;
    pagenum_t temp_pagenum;
    int appending_index, neighbor_index, leaf_index;
    slot_t temp_slot;
    pagenum_t parent_pagenum;

    if (verbose) {
        printf("\n|coalesce_leaf_pages(nei -key- leaf) %lld -%lld- %lld \n", neighbor_pagenum, key_between_two, leaf_pagenum);
        printf("\tbefore");
        print_page(table_id, neighbor_pagenum);
        print_page(table_id, leaf_pagenum);
    }

    // Request the pages.
    buffer_request_page(table_id, neighbor_pagenum, neighbor_page, &neighbor_bufnum);
    buffer_request_page(table_id, leaf_pagenum, leaf_page, &leaf_bufnum);

    if (verbose2) {
        printf("\n1. req fin");
        getchar();
    }

    // Make as neighbor --> leaf, if the leaf page is leftmost branch in the parent page.
    if (leaf_page->slots[0].key < neighbor_page->slots[0].key) {
        temp_pagenum = leaf_pagenum;
        leaf_pagenum = neighbor_pagenum;
        neighbor_pagenum = temp_pagenum;
        
        // Renew the pages.
        buffer_release_page(neighbor_bufnum, false);
        buffer_release_page(leaf_bufnum, false);
        buffer_request_page(table_id, neighbor_pagenum, neighbor_page, &neighbor_bufnum);
        buffer_request_page(table_id, leaf_pagenum, leaf_page, &leaf_bufnum);
    }
    
    // Find the appending index in the neighbor page.
    appending_index = neighbor_page->header.number_of_keys;

    // Move all data from leaf page to neighbor page.
    for (neighbor_index = appending_index, leaf_index = 0; leaf_index < leaf_page->header.number_of_keys; neighbor_index++, leaf_index++) {
        temp_slot = leaf_page->slots[leaf_index];
        // Append temp_slot and pointed value to neighbor_page.
        temp_slot.offset = PAGE_HEADER_SIZE + SLOT_SIZE * neighbor_page->header.number_of_keys + neighbor_page->header.amount_of_free_space - temp_slot.size;
        neighbor_page->slots[neighbor_index] = temp_slot;
        memcpy(&neighbor_page->values[VALUE_OFFSET(temp_slot.offset)], &leaf_page->values[VALUE_OFFSET(leaf_page->slots[leaf_index].offset)], temp_slot.size);
        // Modify the metadata in neighbor_page.
        neighbor_page->header.number_of_keys++;
        neighbor_page->header.amount_of_free_space -= (SLOT_SIZE + temp_slot.size);
    }

    // Modify the metadata in the neighbor page.
    neighbor_page->header.right_sibling_pagenum = leaf_page->header.right_sibling_pagenum;

    // Release the neighbor page. Write the neighbor page.
    buffer_release_page(neighbor_bufnum, true);

    if (verbose2) {
        printf("\n2. nei rel fin.");
        getchar();
    }

    // Get the parent page number.
    parent_pagenum = leaf_page->header.parent_pagenum;

    // Release and free the leaf page.
    buffer_release_page(leaf_bufnum, false);

    if (verbose2) {
        printf("\n3. leaf rel fin.");
        getchar();
    }

    buffer_free_page(table_id, leaf_pagenum);

    if (verbose2) {
        printf("\n4. leaf free fin.");
        getchar();
    }

    if (verbose) {
        printf("\n\tafter");
        print_page(table_id, neighbor_pagenum);
        printf("->");
    }

    // Delete the key pointing the leaf page in the parent page.
    return delete_entry_in_page(table_id, parent_pagenum, 0, key_between_two);
}


int coalesce_internal_pages(table_id_t table_id, pagenum_t internal_pagenum, pagenum_t neighbor_pagenum, page::key_t key_between_two) {
    
    int internal_bufnum, neighbor_bufnum, temp_bufnum, branch_bufnum;
    node_page_t *internal_page, *neighbor_page;
    pagenum_t temp_pagenum;
    int appending_index, neighbor_index, internal_index;
    node_page_t *branch_page;
    pagenum_t parent_pagenum;

    if (verbose) {
        printf("\n|coalesce_internal_pages (nei -key- intern) %lld -%lld- %lld\n", neighbor_pagenum, key_between_two, internal_pagenum);
        printf("\tbefore");
        print_page(table_id, neighbor_pagenum);
        print_page(table_id, internal_pagenum);
    }

    

    // Request the pages.
    buffer_request_page(table_id, internal_pagenum, internal_page, &internal_bufnum);
    buffer_request_page(table_id, neighbor_pagenum, neighbor_page, &neighbor_bufnum);

    // Make as neighbor --> internal if the internal page is leftmost branch in the parent page.
    if (internal_page->slots[0].key < neighbor_page->slots[0].key) {
        temp_pagenum = internal_pagenum;
        internal_pagenum = neighbor_pagenum;
        neighbor_pagenum = temp_pagenum;

        // Renew the pages.
        buffer_release_page(neighbor_bufnum, false);
        buffer_release_page(internal_bufnum, false);
        buffer_request_page(table_id, neighbor_pagenum, neighbor_page, &neighbor_bufnum);
        buffer_request_page(table_id, internal_pagenum, internal_page, &internal_bufnum);
    }

    // Find the appending index in the neighbor page.
    appending_index = neighbor_page->header.number_of_keys;

    // Move all data from internal page to neighbor page.
    // Append key_pointg_internal_page and branch_first_pagenum.
    neighbor_page->branchs[appending_index].key = key_between_two;
    neighbor_page->branchs[appending_index].pagenum = internal_page->header.branch_first_pagenum;
    // Update parent_pagenum of the moved branch. (Read, modify, write)
    buffer_request_page(table_id, neighbor_page->branchs[appending_index].pagenum, branch_page, &branch_bufnum);
    branch_page->header.parent_pagenum = neighbor_pagenum;
    buffer_release_page(branch_bufnum, true);
    // Modify the metadata.
    neighbor_page->header.number_of_keys++;
    for (neighbor_index = appending_index + 1, internal_index = 0; internal_index < internal_page->header.number_of_keys; neighbor_index++, internal_index++) {
        // Append branch to neighbor_page.
        neighbor_page->branchs[neighbor_index] = internal_page->branchs[internal_index];
        // Update parent_pagenum of moved branchs. (Read, modify, write)
        buffer_request_page(table_id, neighbor_page->branchs[neighbor_index].pagenum, branch_page, &branch_bufnum);
        branch_page->header.parent_pagenum = neighbor_pagenum;
        buffer_release_page(branch_bufnum, true);

        // Modify the metadata in neighbor_page.
        neighbor_page->header.number_of_keys++;
    }
    
    // Release the neighbor page. Write the neighbor page.
    buffer_release_page(neighbor_bufnum, true);

    // Get the parent page number.
    parent_pagenum = internal_page->header.parent_pagenum;

    // Release and free the internal page.
    buffer_release_page(internal_bufnum, false);
    buffer_free_page(table_id, internal_pagenum);

    if (verbose) {
        printf("\tafter");
        print_page(table_id, neighbor_pagenum);
        printf("->");
    }

    // Delete the key pointing the internal page in the parent page.
    return delete_entry_in_page(table_id, parent_pagenum, 0, key_between_two);
}

// Move some entries from the neighbor page to the leaf page.
int redistribute_leaf_pages(table_id_t table_id, pagenum_t leaf_pagenum, pagenum_t neighbor_pagenum, int branch_index_between_two) {

    int leaf_bufnum, neighbor_bufnum, parent_bufnum;
    node_page_t *leaf_page, *neighbor_page;
    pagenum_t parent_pagenum;
    node_page_t* parent_page;
    slot_t temp_slot;
    char* temp_value;
    
    if (verbose) {
        printf("\n|redistribute_leaf_pages(nei -idx- leaf) %lld -%d- %lld \n", neighbor_pagenum, branch_index_between_two, leaf_pagenum);
        printf("\tbefore redist");
        print_page(table_id, neighbor_pagenum);
        print_page(table_id, leaf_pagenum);
    }

    // Request the pages.
    buffer_request_page(table_id, leaf_pagenum, leaf_page, &leaf_bufnum);
    buffer_request_page(table_id, neighbor_pagenum, neighbor_page, &neighbor_bufnum);

    // I. neighbor -branch- leaf
    if (neighbor_page->slots[0].key < leaf_page->slots[0].key) {
        // Pull some entries from the neighbor page to the leaf page.
        while (leaf_page->header.amount_of_free_space >= THRESHOLD) {
            // Insert the neighbor's last slots and pointed value into the leaf page.
            temp_slot = neighbor_page->slots[neighbor_page->header.number_of_keys - 1];
            temp_value = (char*)malloc(temp_slot.size);
            memcpy(temp_value, &neighbor_page->values[VALUE_OFFSET(temp_slot.offset)], temp_slot.size);
            insert_into_leaf_page(table_id, leaf_pagenum, temp_slot, temp_value);

            // Remove the neighbor's last slots and pointed value in the neighbor page.
            remove_entry_from_leaf_page(table_id, neighbor_pagenum, temp_slot.key);
        }
        
        // Update the branch in the parent page. (Read, modify, write)
        parent_pagenum = leaf_page->header.parent_pagenum;
        buffer_request_page(table_id, parent_pagenum, parent_page, &parent_bufnum);
        parent_page->branchs[branch_index_between_two].key = leaf_page->slots[0].key;
        buffer_release_page(parent_bufnum, true);
    }
    // II. leaf -branch- neighbor : leaf_pagenum == branch_first_pagenum in parent_page.
    else {
        // Pull some entries from the neighbor page to the leaf page.
        while (leaf_page->header.amount_of_free_space >= THRESHOLD) {
            // Insert the neighbor's first slots and pointed value into the leaf page.
            temp_slot = neighbor_page->slots[0];
            temp_value = (char*)malloc(temp_slot.size);
            memcpy(temp_value, &neighbor_page->values[VALUE_OFFSET(temp_slot.offset)], temp_slot.size);
            insert_into_leaf_page(table_id, leaf_pagenum, temp_slot, temp_value);

            // Remove the neighbor's last slots and pointed value in the neighbor page.
            remove_entry_from_leaf_page(table_id, neighbor_pagenum, temp_slot.key);
        }

        // Update the branch in the parent page. (Read, modify, write)
        parent_pagenum = leaf_page->header.parent_pagenum;
        buffer_request_page(table_id, parent_pagenum, parent_page, &parent_bufnum);
        parent_page->branchs[branch_index_between_two].key = neighbor_page->slots[0].key;
        buffer_release_page(parent_bufnum, true);
    }

    // Release and write the leaf and neighbor pages.
    buffer_release_page(leaf_bufnum, true);
    buffer_release_page(neighbor_bufnum, true);

    if (verbose) {
        printf("\tafter redist");
        print_page(table_id, neighbor_pagenum);
        print_page(table_id, leaf_pagenum);
        printf("|\n");
    }

    return OP_SUCCESS;    
}

int redistribute_internal_pages(table_id_t table_id, pagenum_t internal_pagenum, pagenum_t neighbor_pagenum, int branch_index_between_two) {
    
    int internal_bufnum, neighbor_bufnum, parent_bufnum, branch_bufnum;
    node_page_t *internal_page, *neighbor_page;
    pagenum_t parent_pagenum;
    node_page_t *parent_page, *branch_page;
    int neighbor_index, internal_index;

    if (verbose) {
        printf("\n|redistribute_internal_pages(nei -idx- leaf) %lld -%d- %lld \n", neighbor_pagenum, branch_index_between_two, internal_pagenum);
        printf("\n\tbefore redist");
        print_page(table_id, neighbor_pagenum);
        print_page(table_id, internal_pagenum);
    }

    // Request the pages.
    buffer_request_page(table_id, internal_pagenum, internal_page, &internal_bufnum);
    buffer_request_page(table_id, neighbor_pagenum, neighbor_page, &neighbor_bufnum);

    // Read the parent page.
    parent_pagenum = internal_page->header.parent_pagenum;
    buffer_request_page(table_id, parent_pagenum, parent_page, &parent_bufnum);

    // I. neighbor -branch- internal
    if (neighbor_page->branchs[0].key < internal_page->branchs[0].key) {

        // Make the room of new branch at first position. in internal_page.
        for (internal_index = internal_page->header.number_of_keys; internal_index > 0; internal_index--) 
            internal_page->branchs[internal_index] = internal_page->branchs[internal_index - 1];
        internal_page->branchs[0].pagenum = internal_page->header.branch_first_pagenum;

        // Pull a key parent -> internal
        internal_page->branchs[0].key = parent_page->branchs[branch_index_between_two].key;
        
        // Pull a key neighbor -> parent.
        parent_page->branchs[branch_index_between_two].key = neighbor_page->branchs[neighbor_page->header.number_of_keys - 1].key;

        // Pull a pagenum neighbor -> internal.
        internal_page->header.branch_first_pagenum = neighbor_page->branchs[neighbor_page->header.number_of_keys - 1].pagenum;
        // Update parent_pagenum of moved pagenum. (Read, modify, write)
        buffer_request_page(table_id, internal_page->header.branch_first_pagenum, branch_page, &branch_bufnum);
        branch_page->header.parent_pagenum = internal_pagenum;
        buffer_release_page(branch_bufnum, true);

        // Modify the metadata in internal_page.
        internal_page->header.number_of_keys++;
        // Modify the metadata in neighbor_page.
        neighbor_page->header.number_of_keys--;
    }
    // II. internal -branch- neighbor
    else {

        // Pull a key internal <- parent.
        internal_page->branchs[internal_page->header.number_of_keys].key = parent_page->branchs[branch_index_between_two].key;

        // Pull a key parent <- neighbor.
        parent_page->branchs[branch_index_between_two].key = neighbor_page->branchs[0].key;

        // Pull a pagenum internal <- neighbor.
        internal_page->branchs[internal_page->header.number_of_keys].pagenum = neighbor_page->header.branch_first_pagenum;
        // Update parent_pagenum of moved pagenum. (Read, modify, write)
        buffer_request_page(table_id, internal_page->branchs[internal_page->header.number_of_keys].pagenum, branch_page, &branch_bufnum);
        branch_page->header.parent_pagenum = internal_pagenum;
        buffer_release_page(branch_bufnum, true);

        // Modify the metadata in internal_page.
        internal_page->header.number_of_keys++;
        // Modify the metadata in neighbor_page.
        neighbor_page->header.number_of_keys--;

        // Fill the room of moved branch at first position in neighbor_page.
        neighbor_page->header.branch_first_pagenum = neighbor_page->branchs[0].pagenum;
        for (neighbor_index = 0; neighbor_index < neighbor_page->header.number_of_keys; neighbor_index++)
            neighbor_page->branchs[neighbor_index] = neighbor_page->branchs[neighbor_index + 1];
    }

    // Release and write the pages.
    buffer_release_page(parent_bufnum, true);
    buffer_release_page(internal_bufnum, true);
    buffer_release_page(neighbor_bufnum, true);

    if (verbose) {
        printf("\n\tafter redist");
        print_page(table_id, neighbor_pagenum);
        print_page(table_id, internal_pagenum);
        printf("|");
    }

    return OP_SUCCESS;
}

// Delete the key in the page.
int delete_entry_in_page(table_id_t table_id, pagenum_t pagenum, int is_leaf, page::key_t key) {

    int bufnum, header_bufnum, neighbor_bufnum, parent_bufnum;
    node_page_t *page;
    header_page_t *header_page;
    pagenum_t root_pagenum;
    pagenum_t neighbor_pagenum, parent_pagenum;
    node_page_t *neighbor_page, *parent_page;
    int branch_index, branch_index_between_two;
    page::key_t key_between_two;

    if (verbose) {
        printf("\n|delete_entry_in_page %lld in %lld ", key, pagenum);
    }
    // Remove the entry from page.
    if (is_leaf == 1)
        remove_entry_from_leaf_page(table_id, pagenum, key);
    else 
        remove_entry_from_internal_page(table_id, pagenum, key);
    
    // Get the root page number from the header page.
    buffer_request_page(table_id, 0, header_page, &header_bufnum);
    root_pagenum = header_page->root_pagenum;
    buffer_release_page(header_bufnum, false);

    // I. the page is root page.
    if (pagenum == root_pagenum) 
        return adjust_root_page(table_id, root_pagenum);

    // II. the page is not the root page. (nothing | merge | redistribution)
    // Request the page.
    buffer_request_page(table_id, pagenum, page, &bufnum);

    // II-1. The page is a leaf page.
    if (is_leaf == 1) {
        // i. free_space < threshold, do nothing.
        if (page->header.amount_of_free_space < THRESHOLD) {
            // Release the page.
            buffer_release_page(bufnum, false);
            return OP_SUCCESS;
        }
        // ii. free_space >= threshold, structure modification needed.
        else {
            // Request the parent page for getting neighbor_pagenum.
            parent_pagenum = page->header.parent_pagenum;
            buffer_request_page(table_id, parent_pagenum, parent_page, &parent_bufnum);

            // Get the neighbor page number. (left branch pagenum except the case the page is leftmost page.)
            if (pagenum == parent_page->header.branch_first_pagenum) {
                neighbor_pagenum = parent_page->branchs[0].pagenum;
                branch_index_between_two = 0;
            } else if (pagenum == parent_page->branchs[0].pagenum){
                neighbor_pagenum = parent_page->header.branch_first_pagenum;
                branch_index_between_two = 0;
            } else {
                for (branch_index = 1; branch_index < parent_page->header.number_of_keys; branch_index++) 
                    if (pagenum == parent_page->branchs[branch_index].pagenum) {
                        neighbor_pagenum = parent_page->branchs[branch_index - 1].pagenum;
                        branch_index_between_two = branch_index;
                    }
            }
            // Get the key in branch between two pages.
            key_between_two = parent_page->branchs[branch_index_between_two].key;

            // Release the parent page.
            buffer_release_page(parent_bufnum, false);

            // Reqest the neighbor page.
            buffer_request_page(table_id, neighbor_pagenum, neighbor_page, &neighbor_bufnum);
            
            // ii-1. Merge
            if ((INITIAL_FREE_SPACE - page->header.amount_of_free_space) + (INITIAL_FREE_SPACE - neighbor_page->header.amount_of_free_space) <= INITIAL_FREE_SPACE) {
                // Release the pages.
                buffer_release_page(bufnum, false);
                buffer_release_page(neighbor_bufnum, false);
                return coalesce_leaf_pages(table_id, pagenum, neighbor_pagenum, key_between_two);
            }
            // ii-2. Redistribute
            else {
                // Release the pages.
                buffer_release_page(bufnum, false);
                buffer_release_page(neighbor_bufnum, false);
                return redistribute_leaf_pages(table_id, pagenum, neighbor_pagenum, branch_index_between_two);
            }
        }
    }
    // II-2. The page is a internal page.
    else {
        // i. enough keys. (249+1)/2 + -1 = 124
        if (page->header.number_of_keys > ((ORDER % 2 == 0 ? ORDER/2 : (ORDER + 1)/2) - 1)) {
            // Release the page.
            buffer_release_page(bufnum, false);
            return OP_SUCCESS;        
        }
        // ii. few keys, structure modification needed.
        else {
            // Request the parent page for getting neighbor_pagenum.
            parent_pagenum = page->header.parent_pagenum;
            buffer_request_page(table_id, parent_pagenum, parent_page, &parent_bufnum);

            // Get the neighbor page number. (left branch pagenum except the case the page is leftmost page.)
            if (pagenum == parent_page->header.branch_first_pagenum) {
                neighbor_pagenum = parent_page->branchs[0].pagenum;
                branch_index_between_two = 0;
            } else if (pagenum == parent_page->branchs[0].pagenum){
                neighbor_pagenum = parent_page->header.branch_first_pagenum;
                branch_index_between_two = 0;
            } else {
                for (branch_index = 1; branch_index < parent_page->header.number_of_keys; branch_index++) 
                    if (pagenum == parent_page->branchs[branch_index].pagenum) {
                        neighbor_pagenum = parent_page->branchs[branch_index - 1].pagenum;
                        branch_index_between_two = branch_index;
                    }
            }
            // Get the key in branch between two pages.
            key_between_two = parent_page->branchs[branch_index_between_two].key;

            // Release the parent page.
            buffer_release_page(parent_bufnum, false);

            // Reqest the neighbor page.
            buffer_request_page(table_id, neighbor_pagenum, neighbor_page, &neighbor_bufnum);

            // ii-1. Merge
            // If merged, the key between two pages will be appended. Therefore -1.
            if ((page->header.number_of_keys + neighbor_page->header.number_of_keys) < ORDER - 1) {
                // Release the pages.
                buffer_release_page(bufnum, false);
                buffer_release_page(neighbor_bufnum, false);
                return coalesce_internal_pages(table_id, pagenum, neighbor_pagenum, key_between_two);
            }
            // ii-2. Redistribute
            else {
                // Release the pages.
                buffer_release_page(bufnum, false);
                buffer_release_page(neighbor_bufnum, false);
                return redistribute_internal_pages(table_id, pagenum, neighbor_pagenum, branch_index_between_two);
            }
        }
    }
}

int db_delete(int64_t table_id, int64_t key) {

    int header_bufnum, leaf_bufnum;
    header_page_t* header_page;
    pagenum_t root_pagenum, leaf_pagenum;
    node_page_t* leaf_page;
    int slot_index;
    bool delete_flag = false;

    if (verbose) {
        printf("|db_delete %lld ", key);
    }
    // Check the table_id is valid.
    if (file_is_valid_table_id(table_id) == false)
        return OP_FAILURE;

    // Get the root page number from the header page.
    buffer_request_page(table_id, 0, header_page, &header_bufnum);
    root_pagenum = header_page->root_pagenum;
    buffer_release_page(header_bufnum, false);

    // Get the leaf page number which may contain the key.
    leaf_pagenum = find_leaf_pagenum(table_id, root_pagenum, key);

    // I. the tree does not exist.
    if (leaf_pagenum == 0)
        return OP_FAILURE;
    
    // Request the leaf page.
    buffer_request_page(table_id, leaf_pagenum, leaf_page, &leaf_bufnum);
    // Find the key in the tree.
    for (slot_index = 0; slot_index < leaf_page->header.number_of_keys; slot_index++)
        if (key == leaf_page->slots[slot_index].key) {
            delete_flag = true;
            break;
        }
    // Release the leaf page. 
    buffer_release_page(leaf_bufnum, false);   

    if (verbose) {
        printf("\n\tbefore deletion, delete_flag: %d", delete_flag);
        print_page(table_id, leaf_pagenum);
        if (delete_flag == false) 
            printf("|(find failure)");
        else 
            printf("->");
    }

    // II. the key does not exist in the tree.
    if (delete_flag == false)
        return OP_FAILURE;
    // III. the key exists in the tree.
    else 
        return delete_entry_in_page(table_id, leaf_pagenum, 1, key);
}

int init_db(int num_buf) {
    buffer_init(num_buf);
    file_init();
    verbose = false;
    verbose2 = false;
    return OP_SUCCESS;
}
int shutdown_db(void) {
    buffer_close_table_files();
    file_close_table_files();
    return OP_SUCCESS;
}

void print_page(table_id_t table_id, pagenum_t pagenum) {

    int bufnum;
    node_page_t* page;
    int i;

    buffer_request_page(table_id, pagenum, page, &bufnum);
    
    printf("\n[page %lld buffer %d]-------num_keys: %d----------parent_pn: %lld-----------\n", pagenum, bufnum, page->header.number_of_keys, page->header.parent_pagenum);
    if (pagenum <= 0 || bufnum == INVALID_BUFNUM) {
        printf("(wrong pn or bn");
        getchar();
        return;
    }
    if (page->header.is_leaf == 1) {
        printf("(key, size)");
        for (i = 0; i < page->header.number_of_keys; i++) {
            printf("(%lld, %d)", page->slots[i].key, page->slots[i].size);
        }
    }
    else {
        printf("(key, pagenum)");
        printf("(%lld)", page->header.branch_first_pagenum);
        for (i = 0; i < page->header.number_of_keys; i++) {
            printf("(%lld, %lld)", page->branchs[i].key, page->branchs[i].pagenum);
        }
    }
    printf("\n-----------------------------------------------------------------------");

    buffer_release_page(bufnum, false);
}

int64_t get_root_pagenum(int64_t table_id) {
    
    int header_bufnum;
    header_page_t *header_page;
    pagenum_t root_pagenum;

    buffer_request_page(table_id, 0, header_page, &header_bufnum);
    root_pagenum = header_page->root_pagenum;
    buffer_release_page(header_bufnum, false);

    return root_pagenum;    
}