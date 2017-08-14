#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "shared.h"

unsigned char *disk;
struct ext2_super_block *sb;

// Initialize the ext2 disk by the disk image path
int disk_init(const char *image_path){
  int fd = open(image_path, O_RDWR);

  disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if(disk == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  sb = (struct ext2_super_block *)(disk + 1024);

  return 0;
}

// Find the inode by inode index in the inode table
struct ext2_inode *get_inode_by_idx(unsigned int inode_idx){
  struct ext2_group_desc *gd = (struct ext2_group_desc *)(disk + 2 * 1024);
  char *inode_table = (char *)(disk + EXT2_BLOCK_SIZE *gd->bg_inode_table);
  int inode_size = sizeof(struct ext2_inode);
  return (struct ext2_inode *)(inode_table + inode_size * (inode_idx - 1));
}

// Find inode index by the absolute disk path
unsigned int get_inode_idx_by_path(const char *disk_path){
  unsigned inode_idx = EXT2_ROOT_INO; // start from the inode of root
  struct ext2_dir_entry_2 *dir_entry;
  char dir_name[256];
  int disk_path_len = strlen(disk_path);

  int curr = 0;
  while (curr < disk_path_len){
    int idx = 0;
    while (curr < disk_path_len && disk_path[curr] != '/'){
      dir_name[idx] = disk_path[curr];
      curr++;
      idx++;
    }
    dir_name[idx] = '\0'; // end the string

    if (strlen(dir_name) != 0){
      struct ext2_inode *curr_inode = get_inode_by_idx(inode_idx);
      dir_entry = get_dir_entry_in_inode(curr_inode, dir_name);

      if (dir_entry != NULL){
        inode_idx = dir_entry->inode;

        if ((curr < disk_path_len) && (curr_inode->i_mode & EXT2_S_IFDIR) == 0){
          printf("get_inode_idx_by_path encountered a file instead of a dir.");
          return 0;
        }
      } else {
        return 0;
      }
    }
    curr++;
  }
  return inode_idx;
}

// Find the dir_entry by name inside an inode
struct ext2_dir_entry_2 *get_dir_entry_in_inode(const struct ext2_inode *inode,
  const char *dir_entry_name){

  struct ext2_dir_entry_2 *dir_entry;
  unsigned int indirect_len = EXT2_BLOCK_SIZE / sizeof(unsigned int);

  // go through all of the block pointers and try to find the entry
  int i_blk_idx, blk_ptr_idx;
  for (i_blk_idx = 0; i_blk_idx < 14; i_blk_idx++){
    // first 12 direct block pointer
    if (i_blk_idx < 12 && inode->i_block[i_blk_idx]){
      unsigned char *data_block = disk + (inode->i_block[i_blk_idx] * EXT2_BLOCK_SIZE);
      dir_entry = get_entry_in_block(data_block, dir_entry_name);
      if (dir_entry && strncmp(dir_entry->name, dir_entry_name, dir_entry->name_len) == 0){
        return dir_entry;
      }
    }

    // one single direct block pointer
    if (i_blk_idx == 12 && inode->i_block[i_blk_idx]){
      unsigned int *data_blocks = (unsigned int *)(disk + (inode->i_block[i_blk_idx] * EXT2_BLOCK_SIZE));

      for (blk_ptr_idx = 0; blk_ptr_idx < indirect_len; blk_ptr_idx++){
        unsigned char *data_block = disk + (data_blocks[blk_ptr_idx] * EXT2_BLOCK_SIZE);
        dir_entry = get_entry_in_block(data_block, dir_entry_name);
        if (dir_entry && strncmp(dir_entry->name, dir_entry_name, dir_entry->name_len) == 0){
          return dir_entry;
        }
      }
    }
  }
}

struct ext2_dir_entry_2 *get_entry_in_block(const unsigned char *data_block,
  const char *dir_entry_name){
  struct ext2_dir_entry_2 *dir_entry;
  const unsigned char *curr = data_block;
  const unsigned char *end = (data_block + EXT2_BLOCK_SIZE);

  while (curr < end){
    dir_entry = (struct ext2_dir_entry_2 *) curr;

    if (strncmp(dir_entry_name, dir_entry->name, dir_entry->name_len) == 0){
      return dir_entry;
    }

    curr += dir_entry->rec_len;
  }

  return NULL;
}