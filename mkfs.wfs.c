#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "wfs.h"

//This C program initializes a file to an empty filesystem. The program receives a path to the disk image file as an argument, 
//i.e., mkfs.wfs disk_path initializes the existing file disk_path to an empty filesystem (Fig. a).

int main(int argc, char* argv[]){
    //Error check arguments
    char* disk_path = argv[1];

    struct wfs_sb superblock;

    FILE* fp = fopen(disk_path, "w");
    superblock.head = sizeof(superblock);
    superblock.magic = WFS_MAGIC;

    fwrite(&superblock, sizeof(superblock), 1, fp);
    //write inode for superblock


    return 0;
}