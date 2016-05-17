#ifndef _INCLUDE_SFS_API_H_
#define _INCLUDE_SFS_API_H_

#include <stdint.h>

#define NUM_BLOCKS 10000

#define MAXFILENAME 20
#define MAXEXTENSION 3

#define NUM_DIR_PTRS 12

// Size of the free bit map
#define SIZE (NUM_BLOCKS / 8)

extern uint8_t free_bit_map[SIZE];

typedef struct {
    uint64_t inode;
    char filename[MAXFILENAME];
} dir_entry_t;

typedef struct {
    uint64_t magic;
    uint64_t block_size;
    uint64_t fs_size;
    uint64_t inode_table_len;
    uint64_t root_dir_inode;
    dir_entry_t root_dir_table[];
} superblock_t;

typedef struct {
    unsigned int mode;
    unsigned int link_cnt;
    unsigned int uid;
    unsigned int gid;
    unsigned int size;
    unsigned int data_ptrs[NUM_DIR_PTRS];
    unsigned int ind_ptr;
} inode_t;

/**
 * The file descriptor table
 * 
 * inode    which inode this entry describes
 * rwptr    the read/write pointer
 */
typedef struct {
    uint64_t inode;
    uint64_t rwptr;
} file_descriptor;

/**
 * Marks an index in the free bit map as used
 *
 * @param index     the index to set
 */
void force_set_index(uint32_t index);

/**
 * Marks an index in the free bit map as free
 * 
 * @param index     the index to free
 */
void rm_index(uint32_t index);

/**
 * Find the index of the first free data block
 * and mark it as used
 * 
 * @return the index
 */
uint32_t get_index();

void mksfs(int fresh);
int sfs_getnextfilename(char *fname);
int sfs_getfilesize(const char* path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_fread(int fileID, char *buf, int length);
int sfs_fwrite(int fileID, const char *buf, int length);
int sfs_fseek(int fileID, int loc);
int sfs_remove(char *file);

#endif //_INCLUDE_SFS_API_H_
