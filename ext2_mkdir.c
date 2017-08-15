#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "ext2.h"
#include "shared.h"

unsigned char *disk;

void link_entry_to_inode(struct ext2_dir_entry_2 dir_entry, struct ext2_inode *inode,
    char *dir_entry_name){

    unsigned int i;
    for (i = 0; i < 12; i++){
        if (inode->i_block[i]){
            unsigned char *curr = disk + (inode->i_block[i] * EXT2_BLOCK_SIZE);
            unsigned char *end = (curr + EXT2_BLOCK_SIZE);

            struct ext2_dir_entry *curr_dir_entry;
            while (curr < end) {
                curr_dir_entry = (struct ext2_dir_entry *)curr;

                if (curr + curr_dir_entry->rec_len >= end) {
                    int actual_size = sizeof(struct ext2_dir_entry_2)
                        + curr_dir_entry->name_len;

                    if (curr + actual_size + sizeof(struct ext2_dir_entry_2) < end){
                        curr_dir_entry->rec_len = actual_size;
                        curr += actual_size;
                        break;
                    }
                }
                curr += curr_dir_entry->rec_len;
            }

            if (curr != end){
                dir_entry.rec_len = end - curr;
                memcpy(curr + sizeof(struct ext2_dir_entry_2), dir_entry_name, dir_entry.name_len);
                (*(struct ext2_dir_entry_2 *) curr) = dir_entry;
            } else {
                continue;
            }
        } else {
            inode->i_block[i] = create_block();
            dir_entry.rec_len = EXT2_BLOCK_SIZE;
            unsigned char *curr = disk + (inode->i_block[i] * EXT2_BLOCK_SIZE);
            memcpy(curr + sizeof(struct ext2_dir_entry_2), dir_entry_name, dir_entry.name_len);
            (*(struct ext2_dir_entry_2 *) curr) = dir_entry;
        }
        break;
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: ext2_mkdir <image file name> <absolute path of directory>\n");
        exit(1);
    }

    if (disk_init(argv[1]) == -1) {
        return ENOENT;
    }

    // validate the proposed disk path for a directory
    if (strlen(argv[2]) < 2 || argv[2][0] != '/'){ // must start with '/'
        return ENOENT;
    }

    char *disk_path = (char *) malloc(strlen(argv[2]));
    strcpy(disk_path, argv[2]);
    int disk_path_len = strlen(disk_path);
    /*
    * scrap the the target directory name from the absolute disk path
    */
    // strip the trailing slash and shrink path length by 1 (if any)
    if (disk_path_len > 1 && disk_path[disk_path_len - 1] == '/') {
        disk_path[disk_path_len - 1] = '\0';
        disk_path_len -= 1;
    }
    // loop the path backwards to find the starting position of the dir
    int offset_len = disk_path_len;
    while (offset_len > 0 && disk_path[offset_len - 1] != '/'){
        offset_len--;
    }

    // make two new strings: one for the parent path of the target dir, and one
    // for the target dir name
    char parent_path[offset_len + 1];
    char dir_name[disk_path_len - offset_len + 1];
    strncpy(parent_path, (char *) disk_path, offset_len);
    parent_path[offset_len] = '\0';
    strcpy(dir_name, (char *) disk_path + offset_len);
    if (strlen(dir_name) == 0){
        return ENOENT;
    }
    free(disk_path);

    // strip the trailing slash and shrink path length by 1 (if any)
    if (offset_len > 1 && parent_path[offset_len - 1] == '/') {
        disk_path[offset_len - 1] = '\0';
        offset_len -= 1;
    }
    // get the inode of the parent dir of the new dir
    unsigned int parent_inode_idx = get_inode_idx_by_path(parent_path);
    struct ext2_inode *parent_inode;

    if (parent_inode_idx){
        parent_inode = get_inode_by_idx(parent_inode_idx);
        if (parent_inode && !(parent_inode->i_mode & EXT2_S_IFDIR)){
            // the parent inode is not a dir
            printf("The parent is not a directory\n");
            return ENOENT;
        } else if (get_dir_entry_in_inode(parent_inode, dir_name)) {
            // the dir already exist in the parent dir
            printf("The directory or file already exist\n");
            return EEXIST;
        }
    } else {
        printf("No such file or directory\n");
        return ENOENT;
    }

    // create a new inode for the new dir entry
    unsigned int dir_inode_idx = create_inode();
    if (dir_inode_idx > 2){
        struct ext2_inode *dir_inode = get_inode_by_idx(dir_inode_idx);
        dir_inode->i_mode = EXT2_S_IFDIR;
        dir_inode->i_size = EXT2_BLOCK_SIZE;
        dir_inode->i_links_count = 1;
        dir_inode->i_blocks = 2;

        int i;
        for (i = 0; i < 15; ++i){
            dir_inode->i_block[i] = 0;
        }

        // create a new entry for the new dir entry
        struct ext2_dir_entry_2 dir_entry;
        dir_entry.inode = dir_inode_idx;
        dir_entry.name_len = strlen(dir_name);
        dir_entry.file_type = EXT2_FT_DIR;

        link_entry_to_inode(dir_entry, parent_inode, dir_name);

        struct ext2_dir_entry_2 curr_dir, prev_dir;
        curr_dir.inode = dir_inode_idx;
        curr_dir.name_len = 1;
        curr_dir.file_type = EXT2_FT_DIR;
        prev_dir.inode = parent_inode_idx;
        prev_dir.name_len = 2;
        prev_dir.file_type = EXT2_FT_DIR;

        link_entry_to_inode(curr_dir, dir_inode, ".");
        link_entry_to_inode(prev_dir, dir_inode, "..");

    }

    return 0;
}
