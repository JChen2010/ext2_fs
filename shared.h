#ifndef SHARED_H
#define SHARED_H

#include <errno.h>
#include <string.h>
#include "ext2.h"

extern unsigned char *disk;
extern struct ext2_super_block *sb;

int disk_init(const char *image_path);

// get inode
unsigned int get_inode_idx_by_path(const char *disk_path);
struct ext2_inode *get_inode_by_idx(unsigned int inode_idx);
struct ext2_dir_entry_2 *get_dir_entry_in_inode(const struct ext2_inode *inode,
  const char *dir_entry_name);
struct ext2_dir_entry_2 *get_entry_in_block(const unsigned char *data_block,
  const char *dir_entry_name);

// create inode
unsigned int create_inode();

// create block
unsigned int create_block();

#endif
