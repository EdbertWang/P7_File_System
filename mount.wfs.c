#define FUSE_USE_VERSION 30
#include <fuse.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "wfs.h"

void* disk_map;

static int wfs_getattr(const char *path, struct stat *stbuf) { // Stat
    printf("getattr with path arg = %s\n", path);
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) { // This part works on stat and ls calls
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    return res;
}

static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) { // Echo
    printf("mknod\n");
    return 0; 
}

static int wfs_mkdir(const char* path, mode_t mode) {
    printf("mkdir\n");
    return 0; 
}

static int wfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) { // Cat
    printf("read\n");
    return 0;
}

static int wfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi) { // Echo -> Write
    printf("write\n");
    return 0; 
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){ // cd or ls
    printf("readdir with %s\n", path);
    struct wfs_log_entry* curr_dir = (struct wfs_log_entry*)((char*)disk_map + sizeof(struct wfs_sb) + offset);

    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_ino = curr_dir->inode.inode_number;
    st.st_mode = curr_dir->inode.mode;

    for (int iter = 0; iter < curr_dir->inode.size / sizeof(struct wfs_dentry); iter++){
        printf("Dir Contains: %s\n",curr_dir->data[iter].name);

        //if (filler(buf, curr_dir->data[iter].name, &st, 0, FUSE_FILL_DIR_PLUS))
          //      break;
    }
    return 0;

    /*
    for (int iter = 0; iter < ((struct wfs_sb*)disk_map)->head; iter += curr->entry_length){
        if (strcmp(path, ) == 0){

        }

        struct wfs_log_entry* curr = (struct wfs_log_entry*)(temp + iter);
        if (curr->inode.deleted == 0 && curr->inode.mode | S_IFDIR){ // This log entry refers to a directory
            printf("Directory Found\n");
        }
    }
    */
}

static int wfs_unlink(const char* path){ // rm
    printf("unlink\n");
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

    // Get rid of disk path before passing in argv
    char* disk_path = argv[argc-2];
    argv[argc-2] = argv[argc-1];
    argv[argc-1] =  NULL;
    argc--;

    int disk_fd = open(disk_path, O_RDWR);
    if (disk_fd < 0){
        printf("Disk Open Error\n");
        close(disk_fd);
        return 1;
    }

    struct stat st;
    stat(disk_path, &st);

    disk_map = mmap(0,st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, disk_fd, 0);

    struct wfs_sb* superblock = disk_map;
    if (superblock->magic != WFS_MAGIC){
        printf("Not a wfs filesystem");
        munmap(disk_map, st.st_size);
        close(disk_fd);
        return 1;
    }

    return fuse_main(argc, argv, &ops, NULL);

}
