#define FUSE_USE_VERSION 30
#include <fuse.h>

#include <stdio.h>
#include <stdlib.h>
#include "wfs.h"

static int wfs_getattr() {
    return 0; 
}

static int wfs_mknod() {
    return 0; 
}

static int wfs_mkdir() {
    return 0; 
}

static int wfs_read() {
    return 0;
}

static int wfs_write() {
    return 0; 
}

static int wfs_readdir(){
    return 0;
}

static int wfs_unlink(){
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

int main(int argc, char *argv[]) {

    //save diskpath arg
    //rewrite diskpath arg in argv to mount_point
    //null terminating therefore old mount_point -> null

    //note quite argv
    return fuse_main(argc, argv, &ops, NULL);
}
