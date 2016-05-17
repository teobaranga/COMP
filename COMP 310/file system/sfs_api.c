#include "sfs_api.h"
#include "disk_emu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISK "sfs_disk.disk"
#define BLOCK_SZ 1024       // block size
#define NUM_INODES 1024     // number of files (because there are no directories)
#define NUM_INODE_BLOCKS (sizeof(inode_t) * NUM_INODES / BLOCK_SZ + 1)          // the number of inode blocks
#define NUM_ROOT_DIR_BLOCKS (sizeof(dir_entry_t) * NUM_INODES / BLOCK_SZ + 1)   // the number of blocks used by the root directory
#define NUM_IND_PTRS (BLOCK_SZ / sizeof(unsigned int))  // the number of pointers in the ind ptr block
#define LAST_AVAILABLE_DATA_BLOCK NUM_BLOCKS - 1        // NUM_BLOCKS - the number of blocks used by the free bit map

// superblock
superblock_t sb;

// inode table
inode_t table[NUM_INODES];

// file descriptor table
file_descriptor fdt[NUM_INODES];

// root directory table
dir_entry_t root_directory[NUM_INODES];

int current_dir_pos = 0;

void init_superblock() {
    sb.magic = 0xACBD0005;
    sb.block_size = BLOCK_SZ;
    sb.fs_size = NUM_BLOCKS * BLOCK_SZ;
    sb.inode_table_len = NUM_INODE_BLOCKS;
    sb.root_dir_inode = 0;
}

void mksfs(int fresh) {
    if (fresh) {
        // create super block
        init_superblock();

        // init root directory & fdt
        for (int i = 0; i < NUM_INODES; ++i) {
            root_directory[i].inode = 0;
            fdt[i].inode = 0;
        }

        init_fresh_disk(DISK, BLOCK_SZ, NUM_BLOCKS);

        // write the super block, the inode table and the root dir table
        write_blocks(0, 1, &sb);
        write_blocks(1, sb.inode_table_len, table);
        write_blocks(1 + sb.inode_table_len, NUM_ROOT_DIR_BLOCKS, root_directory);

        // populate the free bitmap
        int free_index = 0;
        force_set_index(free_index++);                              // super block
        for (int i = 0; i < sb.inode_table_len; ++i, ++free_index)  // inode table
            force_set_index(free_index);
        for (int i = 0; i < NUM_ROOT_DIR_BLOCKS; ++i, ++free_index) // root dir table
            force_set_index(free_index);
        force_set_index(LAST_AVAILABLE_DATA_BLOCK);                 // free bit map

        // write the free bitmap to the last block
        write_blocks(LAST_AVAILABLE_DATA_BLOCK, 1, free_bit_map);
    } else {
        // reopening file system
        init_disk(DISK, BLOCK_SZ, NUM_BLOCKS);

        // read super block, inode table, root dir table, and free bit map
        read_blocks(0, 1, &sb);
        read_blocks(1, sb.inode_table_len, table);
        read_blocks(1 + sb.inode_table_len, NUM_ROOT_DIR_BLOCKS, root_directory);
        read_blocks(LAST_AVAILABLE_DATA_BLOCK, 1, free_bit_map);
    }
    return;
}

/**
 * Gets the next file name from the root directory,
 * used when retrieving all the file names
 *
 * @param  fname    where the name will be stored
 * @return          0 if reached the end of the root directory and there are no files left
 *                  1 if there might be files left
 */
int sfs_getnextfilename(char *fname) {
    if (current_dir_pos != NUM_INODES)
        do {
            // check if it's a valid file and not the root node
            if (root_directory[current_dir_pos].inode != 0) {
                strcpy(fname, root_directory[current_dir_pos].filename);
                current_dir_pos++;
                return 1;
            }
            current_dir_pos++;
            current_dir_pos %= NUM_INODES;
        } while (current_dir_pos != 0);

    // all files have been returned
    current_dir_pos = 0;
    return 0;
}

/**
 * Gets the size of a file
 *
 * @param  path the file name
 * @return      the size of the file or -1 if the file was not found
 */
int sfs_getfilesize(const char* path) {
    // look for the file
    for (int i = 1; i < NUM_INODES; ++i)
        if (root_directory[i].inode != 0 && strcmp(root_directory[i].filename, path) == 0)
            // return its size
            return table[root_directory[i].inode].size;
    // file does not exist
    return -1;
}

/**
 * Open an already existing file, or create one and then open it
 * if it doesn't already exist
 *
 * @param  name the file name
 * @return      the index of the opened file in the file descriptor table
 */
int sfs_fopen(char *name) {
    // reject files with filenames longer than the maximum
    if (strlen(name) > MAXFILENAME)
        return -1;

    // look for the file
    for (int i = 1; i < NUM_INODES; ++i)
        if (root_directory[i].inode != 0 && strcmp(root_directory[i].filename, name) == 0) {
            // file found
            if (fdt[i].inode != 0)
                // file already open
                return -2;

            // open file,
            fdt[i].inode = root_directory[i].inode;

            // open in append mode (set the r/w pointer to the end of the file)
            sfs_fseek(i, table[i].size);

            return i;
        }

    // file not found, create it

    // find new index
    int file_index = -1;
    for (int i = 1; i < NUM_INODES; ++i)
        if (root_directory[i].inode == 0) {
            // found free index
            file_index = i;
            break;
        }

    if (file_index == -1)
        // maximum file number reached
        return -3;

    // find a free block
    int free_block_index = get_index();

    // create/initialize the inode
    inode_t* n = &table[file_index];
    n->size = 0;
    memset(n->data_ptrs, 0, NUM_DIR_PTRS);
    n->ind_ptr = 0;
    n->data_ptrs[0] = free_block_index;

    // update the root directory table
    dir_entry_t* d = &root_directory[file_index];
    d->inode = file_index;
    strcpy(d->filename, name);

    // flush inode table, root dir & free bit map to disk
    write_blocks(1, sb.inode_table_len, table);
    write_blocks(1 + sb.inode_table_len, NUM_ROOT_DIR_BLOCKS, root_directory);
    write_blocks(LAST_AVAILABLE_DATA_BLOCK, 1, free_bit_map);

    // update the fdt
    file_descriptor* f = &fdt[file_index];
    f->inode = d->inode;
    f->rwptr = 0;

    return file_index;
}

/**
 * Close a file
 *
 * @param  fileID the file index in the file descriptor table
 * @return        0 on successful close, -1 otherwise
 */
int sfs_fclose(int fileID) {
    file_descriptor* f = &fdt[fileID];

    if (f->inode == 0)
        // file already closed
        return -1;

    // close the file
    f->inode = 0;

    return 0;
}

/**
 * Gets the block at the specified byte position
 *
 * @param  n            the inode for which to find the block
 * @param  location     the byte position in the file
 * @param  global_index the global index of the block is returned here
 * @param  local_index  the local index of the block (0-12+) is returned here
 * @return              0 on success
 */
int sfs_getblockatpointer(inode_t* n, uint64_t location, unsigned int* global_index, unsigned int* local_index) {
    // check for invalid locations
    if (location < 0 || location > n->size)
        return -1;

    int block_index = location / BLOCK_SZ;
    if (block_index >= NUM_DIR_PTRS) {
        // check if the index is valid
        if (n->ind_ptr == 0)
            return -2;

        // file is larger than 12 blocks
        block_index -= NUM_DIR_PTRS;
        if (block_index > (NUM_IND_PTRS - 1))
            // location points past the maximum file size
            return -3;

        // load the indirect pointer block
        unsigned int ind_ptr_block[NUM_IND_PTRS];
        read_blocks(n->ind_ptr, 1, ind_ptr_block);

        // check if the index is valid
        if (ind_ptr_block[block_index] == 0)
            return -2;

        *local_index = NUM_DIR_PTRS + block_index;
        *global_index = ind_ptr_block[block_index];
    } else {
        // check if the index is valid
        if (n->data_ptrs[block_index] == 0)
            return -2;

        *local_index = block_index;
        *global_index = n->data_ptrs[block_index];
    }
    return 0;
}

/**
 * Read from a file
 *
 * @param  fileID the file index in the file descriptor table
 * @param  buf    the buffer which will store what was read
 * @param  length the size of the content to be read in bytes
 * @return        the number of bytes read
 */
int sfs_fread(int fileID, char *buf, int length) {
    // check the length
    if (length < 0)
        return -1;

    // check if the file exists
    if (root_directory[fileID].inode == 0)
        return -2;

    file_descriptor* f = &fdt[fileID];
    inode_t* n = &table[f->inode];

    // check if the file is open
    if (f->inode == 0)
        return -3;

    // clamp the read length to avoid reading past the file size
    if (length > (n->size - f->rwptr))
        length = n->size - f->rwptr;

    unsigned int read_length = 0;
    unsigned int block_global = 0;   // the global block number
    unsigned int block_local = 0;    // the block index for this file
    char block[BLOCK_SZ];

    // get the index of the required data block and load it
    if (sfs_getblockatpointer(n, f->rwptr, &block_global, &block_local) != 0)
        return -4;
    read_blocks(block_global, 1, block);

    // compute the number of additional blocks required for this read
    unsigned int length_local = length;             // the length to be read in this block
    unsigned int additional_required_blocks = 0;
    unsigned int remaining_space = BLOCK_SZ - f->rwptr % BLOCK_SZ;
    if (length > remaining_space) {
        length_local = remaining_space;
        additional_required_blocks += 1 + (length - remaining_space) / (BLOCK_SZ + 1);
    }

    // read the data from the current block
    unsigned int local_rwptr = f->rwptr % BLOCK_SZ;
    unsigned int buffer_index = 0;
    for (buffer_index = 0; buffer_index < length_local; ++buffer_index, ++local_rwptr, ++f->rwptr, ++read_length)
        buf[buffer_index] = block[local_rwptr];

    // read additional blocks
    int remaining_length = length;
    for (int i = 0; i < additional_required_blocks; ++i) {
        remaining_length -= length_local;

        // load the next block to be read
        ++block_local;
        if (block_local < NUM_DIR_PTRS)
            // next block to read is a direct block
            block_global = n->data_ptrs[block_local];
        else {
            // next block to read is an indirect block
            block_local -= NUM_DIR_PTRS;

            if (block_local > (NUM_IND_PTRS - 1))
                // trying to read past the maximum file size
                return -5;

            if (n->ind_ptr == 0)
                // no indirect block pointer
                return -6;

            // load the indirect block
            unsigned int ind_ptr_block[NUM_IND_PTRS];
            read_blocks(n->ind_ptr, 1, ind_ptr_block);
            block_global = ind_ptr_block[block_local];
            block_local += NUM_DIR_PTRS;
        }

        read_blocks(block_global, 1, block);

        if (remaining_length > BLOCK_SZ)
            length_local = BLOCK_SZ;
        else
            length_local = remaining_length;

        // read
        for (int i = 0; i < length_local; ++i, ++buffer_index, ++f->rwptr, ++read_length)
            buf[buffer_index] = block[i];
    }
    return read_length;
}

/**
 * Write to a file
 *
 * @param  fileID the file index in the file descriptor table
 * @param  buf    the buffer containing the bytes to be written
 * @param  length the size of the content to be written in bytes
 * @return        the number of bytes written
 */
int sfs_fwrite(int fileID, const char *buf, int length) {
    // check the write length
    if (length < 0)
        return -1;

    // check if file exists
    if (root_directory[fileID].inode == 0)
        return -2;

    file_descriptor* f = &fdt[fileID];
    inode_t* n = &table[fileID];

    // check if the file is open
    if (f->inode == 0)
        return -3;

    // load the required data block
    unsigned int write_length = 0;
    unsigned int block_global = 0;   // the global block number
    unsigned int block_local = 0;    // the block index for this file
    char block[BLOCK_SZ];

    // find its index and load the block
    if (sfs_getblockatpointer(n, f->rwptr, &block_global, &block_local) != 0)
        return -4;
    read_blocks(block_global, 1, block);

    // compute the number of additional blocks required for this write
    unsigned int length_local = length;             // the length to be written in this block
    unsigned int additional_required_blocks = 0;
    unsigned int remaining_space = BLOCK_SZ - f->rwptr % BLOCK_SZ;
    if (length > remaining_space) {
        length_local = remaining_space;
        additional_required_blocks += 1 + (length - remaining_space) / (BLOCK_SZ + 1);
    }

    // write the data to the block
    unsigned int local_rwptr = f->rwptr % BLOCK_SZ;
    unsigned int buffer_index = 0;
    for (buffer_index = 0; buffer_index < length_local; ++buffer_index, ++local_rwptr, ++f->rwptr, ++n->size, ++write_length)
        block[local_rwptr] = buf[buffer_index];
    write_blocks(block_global, 1, block);

    int remaining_length = length;
    // write data to other blocks
    for (int i = 0; i < additional_required_blocks; ++i) {
        // calculate data size written to this block
        remaining_length -= length_local;

        // find a free block
        unsigned int block_index = get_index();
        if (block_index > LAST_AVAILABLE_DATA_BLOCK)
            // file system full, abort write
            return write_length;

        ++block_local;
        if (block_local < NUM_DIR_PTRS)
            // direct pointer index
            n->data_ptrs[block_local] = block_index;
        else {
            // indirect pointer index
            block_local -= NUM_DIR_PTRS;

            if (block_local > (NUM_IND_PTRS - 1))
                // trying to write past the maximum file size
                return write_length;

            if (n->ind_ptr == 0) {
                // create the indirect pointer block if it doesn't exist
                n->ind_ptr = get_index();
                if (n->ind_ptr > LAST_AVAILABLE_DATA_BLOCK)
                    // file system full, abort write
                    return write_length;
            }

            // load the indirect block
            unsigned int ind_ptr_block[NUM_IND_PTRS];
            read_blocks(n->ind_ptr, 1, ind_ptr_block);

            // add the block to the ind block ptr
            ind_ptr_block[block_local] = block_index;
            write_blocks(n->ind_ptr, 1, ind_ptr_block);

            block_local += NUM_DIR_PTRS;
        }

        // load the block
        read_blocks(block_index, 1, block);

        if (remaining_length > BLOCK_SZ)
            length_local = BLOCK_SZ;
        else
            length_local = remaining_length;

        // write
        for (int i = 0; i < length_local; ++i, ++buffer_index, ++f->rwptr, ++n->size, ++write_length)
            block[i] = buf[buffer_index];
        write_blocks(block_index, 1, block);
    }

    // update the inode table
    write_blocks(1, sb.inode_table_len, table);

    return write_length;
}

/**
 * Seek to the location specified, which needs to be a number
 * between 0 and the size of the file
 *
 * @param  fileID the index of the file in the file descriptor table
 * @param  loc    the position in the file
 * @return        0 on success
 */
int sfs_fseek(int fileID, int loc) {
    // check if file exists
    if (root_directory[fileID].inode == 0)
        return -1;

    file_descriptor* f = &fdt[fileID];
    inode_t* n = &table[fileID];

    // check if file is open
    if (f->inode == 0)
        return -2;

    // check if the seek location is valid
    if (loc < 0 || loc >= n->size)
        return -3;

    f->rwptr = loc;

    return 0;
}

/**
 * Remove a file
 *
 * @param  file the file name
 * @return      0 on success, -1 on failure
 */
int sfs_remove(char *file) {
    // look for the file
    for (int i = 1; i < NUM_INODES; ++i)
        if (root_directory[i].inode != 0 && strcmp(root_directory[i].filename, file) == 0) {
            // update the free bit map
            inode_t* n = &table[i];
            for (int i = 0; i < NUM_DIR_PTRS; ++i)
                if (n->data_ptrs[i] != 0)
                    rm_index(n->data_ptrs[i]);
            if (n->ind_ptr != 0) {
                unsigned int ind_ptr_block[NUM_IND_PTRS];
                read_blocks(n->ind_ptr, 1, ind_ptr_block);
                for (int i = 0; i < NUM_IND_PTRS; ++i)
                    if (ind_ptr_block[i] != 0 && ind_ptr_block[i] <= LAST_AVAILABLE_DATA_BLOCK)
                        rm_index(ind_ptr_block[i]);
            }

            // remove file
            root_directory[i].inode = 0;
            fdt[i].inode = 0;

            return 0;
        }

    // file to remove not found
    return -1;
}
