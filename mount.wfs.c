#define FUSE_USE_VERSION 30
#include <fuse.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>

#include "wfs.h"
#include "mmap.h"


static int wfs_getattr(){
    printf("Called getattr\n");
    return 0;     
}
 

static int wfs_mknod(){
    printf("Called mknod\n");
    return 0; 
}

static int wfs_mkdir(){
    printf("Caleld mkdir\n");
    return 0; 
}

static int wfs_read(){
    printf("Called read\n");
    return 0;
}

static int wfs_write(){
    printf("Called write\n");
    return 0; 
}

static int wfs_readdir(){
    printf("Called readdir\n");
    return 0;
}

static int wfs_unlink(){
    printf("Called unlink\n");
    return 0;
}

static struct fuse_operations ops = {
    .getattr	= wfs_getattr,
    .mknod      = wfs_mknod,
    .mkdir      = wfs_mkdir,
    .read	    = wfs_read,
    .write      = wfs_write,
    .readdir	= wfs_readdir,
    .unlink    	= wfs_unlink,
};


//This program mounts the filesystem to a mount point, which are specifed by the arguments. 
//The usage is mount.wfs [FUSE options] disk_path mount_point

int main(int argc, char* argv[]) {
    // Get rid of disk path before passing in argv
    char* disk_path = argv[argc-2];
    argv[argc-2] = argv[argc-1];
    argv[argc-1] =  NULL;
    argc--;

    FILE* disk_fd = open(disk_path, O_RDWR);
    if (disk_fd < 0){
        printf("Disk Open Error\n");
        return 1;
    }

    struct stat st;
    stat(disk_path, &st);

    void* disk_map = mmap(0,st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, disk_fd, 0);


    struct wfs_sb* superblock = disk_map;
    if (superblock->magic != WFS_MAGIC){
        printf("Not a wfs filesystem");
        munmap(disk_map, st.st_size);
        close(disk_fd);
        return 1;
    }

    return fuse_main(argc, argv, &ops, NULL);
}
