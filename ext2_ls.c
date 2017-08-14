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

void print_directory(const struct ext2_inode *inode, unsigned int print_dots){

    int i_blk_idx;
    for (i_blk_idx = 0; i_blk_idx < 12; i_blk_idx++){
        if (inode->i_block[i_blk_idx]){
            struct ext2_dir_entry_2 *dir_entry;
            unsigned char *data_block = disk + (inode->i_block[i_blk_idx] * EXT2_BLOCK_SIZE);
            unsigned char *curr_pos = data_block;
            while (curr_pos < (data_block + EXT2_BLOCK_SIZE)){
                dir_entry = (struct ext2_dir_entry_2 *)curr_pos;

                if (dir_entry->name_len > 0){
                    if (print_dots){
                        printf("%.*s\n", dir_entry->name_len, dir_entry->name);
                    } else {
                        printf("%.*s\n", dir_entry->name_len, dir_entry->name);
                    }
                }

                curr_pos += dir_entry->rec_len;
            }
        }

    }
}

int main(int argc, char **argv) {
    unsigned int inode_idx, print_dots, disk_path_len = 0;
    char target_dir[EXT2_NAME_LEN];
    char *disk_path;

    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: ext2_ls <image file name> <absolute path on disk> <[-a]>\n");
        exit(1);
    } else {
        if (disk_init(argv[1]) == -1) {
            return ENOENT;
        }

        /*
        * Assume there is no [-a] option entered if 3 args being passed in
        * i.e. argv[1] => image path ; argv[2] => disk path
        * If 4 args are passed in:
        * i.e. argv[1] => image path ; argv[2] => -a argv[3] => disk path
        */
        if (argc == 3) {
            disk_path_len = strlen(argv[2]);
            disk_path = malloc(disk_path_len);
            strcpy(disk_path, argv[2]);
        } else { // Check if the [-a] option is entered
            if (strcmp(argv[2], "-a") == 0){
                disk_path_len = strlen(argv[3]);
                disk_path = malloc(disk_path_len);
                strcpy(disk_path, argv[3]);
                // flag for print dir function
                print_dots = 1;
            } else {
                return ENOENT;
            }

        }

        /*
        * scrap the the target directory name from the absolute disk path
        */
        // strip the trailing slash and shrink path length by 1 (if any)
        if (disk_path_len > 1 && disk_path[disk_path_len - 1] == '/') {
            disk_path[disk_path_len - 1] = '\0';
            disk_path_len -= 1;
        }
        // loop the path backwards to find the starting position of the dir
        int target_dir_pos = disk_path_len - 1;
        while (target_dir_pos > 0 && disk_path[target_dir_pos] != '/'){
            target_dir_pos--;
        }
        int target_dir_len = (disk_path_len - 1) - target_dir_pos;
        strncpy(target_dir, disk_path + target_dir_pos, target_dir_len);
        target_dir[target_dir_len] = '\0';

        //DEBUG MSG
        printf("target file path: %s\n", target_dir);

        inode_idx = get_inode_idx_by_path(disk_path);
    }

    if (inode_idx > 0){
        struct ext2_inode *inode = get_inode_by_idx(inode_idx);
        // make sure it's a directory type inode
        if (inode && (inode->i_mode & EXT2_S_IFDIR)){
            print_directory(inode, print_dots);
        } else if (inode) {
            printf("%s\n", target_dir);
        } else {
            printf("file or dir not found: %s\n", target_dir);
            return ENOENT;
        }
    }

    return 0;
}
