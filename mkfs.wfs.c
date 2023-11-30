#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include "wfs.h"

//This C program initializes a file to an empty filesystem. The program receives a path to the disk image file as an argument, 
//i.e., mkfs.wfs disk_path initializes the existing file disk_path to an empty filesystem (Fig. a).

int main(int argc, char* argv[]){
    //Error check arguments
    char* disk_path = argv[1];

    struct wfs_sb superblock;
    struct wfs_inode i = {
        .inode_number = 0,
        .deleted = 0,
        .mode = S_IFDIR | 0755, // Unsure bout this one TODO:
        .uid = getuid(),
        .gid = getgid(),
        .atime = time(NULL),
        .mtime = time(NULL),
    };
    struct wfs_log_entry root = {
        .inode = i,
    };

    FILE* fp = fopen(disk_path, "w");
    superblock.head = sizeof(superblock) + sizeof(root);
    superblock.magic = WFS_MAGIC;

    fwrite(&superblock, sizeof(superblock), 1, fp);
    fwrite(&root, sizeof(root), 1, fp);
    //write inode for superblock

    fclose(fp);
    return 0;
}