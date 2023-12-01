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

/*
static int comp (const void * elem1, const void * elem2) {
    int f = *((int*)elem1);
    int s = *((int*)elem2);
    if (f > s) return  1;
    if (f < s) return -1;
    return 0;
}*/

static struct wfs_log_entry* find_by_inode(int inode){
    char* temp = (char*)disk_map + sizeof(struct wfs_sb);
    int offset = 0;

    while (offset < ((struct wfs_sb*)disk_map)->head - sizeof(struct wfs_sb)){
        struct wfs_log_entry* curr = (struct wfs_log_entry*)(temp + offset);
        if (curr->inode.inode_number == inode && curr->inode.deleted == 0){
            return curr;
        }
        offset += curr->inode.size;
    }
    printf("Inode number %d Not Found\n" , inode);
    return NULL;
}

static int find_in_directory(struct wfs_log_entry* location, char* file_name){
    if (location == NULL){
        printf("Null Pointer Passed\n");
        return -1;
    }
    int data_size = location->inode.size - sizeof(struct wfs_inode);
    for (int i = 0; i < data_size; i += sizeof(struct wfs_dentry)){
        struct wfs_dentry* curr = (struct wfs_dentry*) location->data + i;
        if (strcmp(curr->name, file_name) == 0){
            return curr->inode_number;
        }
    }
    printf("File/Directory %s Not Found in inode %d\n" , file_name, location->inode.inode_number);
    return -1;
}

char** str_split(char* a_str, int* num) {
    char** result;
    size_t count = 0;
    char* tmp = a_str;
    char* last = 0;
    char delim[2];
    delim[0] = '/';
    delim[1] = '\0';

    // Count how many elements will be extracted
    while (*tmp) {
        if (*tmp == '/') {
            count++;
            last = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token and terminating null string*/
    count += last < (a_str + strlen(a_str) - 1);
    count++;  
    result = malloc(sizeof(char*) * count);
    if (result) {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token) {
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        *(result + idx) = 0;
        *num = count;
    }
    return result;
}

static int wfs_getattr(const char *path, struct stat *stbuf) { // Stat
    printf("getattr with path arg = %s\n", path);
    printf("End Val: %ld\n", ((struct wfs_sb*)disk_map)->head - sizeof(struct wfs_sb));
    memset(stbuf, 0, sizeof(struct stat));

    struct wfs_log_entry* root =  find_by_inode(0);
    if (strcmp(path,"/") == 0){ // If is root
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 1;
        stbuf->st_size = ((struct wfs_sb*)disk_map)->head;
        stbuf->st_uid = root->inode.uid;
        stbuf->st_gid = root->inode.gid;
        return 0;
    }

    char* temppath = malloc(sizeof(char) * 512);
    strcpy(temppath, path);

    int count = -1;
    char** tokens = str_split(temppath, &count);

    if (tokens) {
        for (int i = 0; i < count; i++) {
            printf("Looking for dir[%s]\n", *(tokens + i));
            int nextinode = find_in_directory(root, *(tokens + i));
            root = find_by_inode(nextinode);
            free(*(tokens + i));
            if (!root){
                i++;
                for (; i < count; i++) {
                    free(*(tokens + i));
                }
                free(tokens);
                free(temppath);
                return 0;
            }
        }
        free(tokens);
    }

    stbuf->st_mode = root->inode.mode;
    stbuf->st_nlink = root->inode.links;
    stbuf->st_size = root->inode.size;
    stbuf->st_uid = root->inode.uid;
    stbuf->st_gid = root->inode.gid;

    free(temppath);
    return 0;

}

static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) { // Echo
    printf("mknod\n");
    return 0; 
}

static int wfs_mkdir(const char* path, mode_t mode) {
    printf("mkdir\n");
    printf("End Val: %ld\n", ((struct wfs_sb*)disk_map)->head - sizeof(struct wfs_sb));

/* TODO: Replace with new functions
    char *lastSlash = strrchr(path, '/');
    char parentname[256];

    if (lastSlash != NULL) {
        strncpy(parentname, path, lastSlash - path);
        parentname[lastSlash - path] = '\0';
    } else {
        printf("Invalid Path\n");
        return 0;
    }

    struct wfs_log_entry* parent = NULL;
    char* temp = (char*)disk_map + sizeof(struct wfs_sb);
    int offset = 0;
    int* nums = (int*) malloc(512 * sizeof(int));
    int inode = 0;


    // Iterate to find the necessary things
    while (offset < ((struct wfs_sb*)disk_map)->head - sizeof(struct wfs_sb)){
        struct wfs_log_entry* curr = (struct wfs_log_entry*)(temp + offset);
        if (curr->inode.deleted == 0 && strcmp(path,curr->path) == 0){ TODO: 
            printf("Already Exists\n");
            return 0;
        } else if (curr->inode.deleted == 0 && strcmp(path,parentname) == 0){
            parent = curr;
        }
        nums[inode++] = curr->inode.inode_number;
        offset += curr->inode.size;
    }
    

    if (strlen(parentname) > 0 && parent == NULL){
        printf("Specified Path Doesn't Exist\n");
        return 0;
    }

    // Find first free inode
    qsort(nums,inode,sizeof(int),comp);
    for (int i = 0; i < inode; i++){
        if (i != nums[i]){
            inode = i;
            break;
        }
    }
    free(nums);

    // Create new entry for old 
    if (parent){
        struct wfs_log_entry newp;
        memcpy(&newp, parent, parent->inode.size - sizeof(struct wfs_inode));
        newp.inode.size += sizeof(struct wfs_dentry);
        struct wfs_dentry new_dir = {
            .inode_number = inode,
        };
        strncpy(new_dir.name, path[lastSlash - path], strlen(path) - (lastSlash - path));
        new_dir.name[strlen(path) - (lastSlash - path)] = '\0';

        parent->inode.deleted = 1;

        memcpy(((struct wfs_sb*)disk_map)->head, &newp, newp.inode.size);
        ((struct wfs_sb*)disk_map)->head += newp.inode.size;
    }

    // Create entry for new directory
    struct wfs_inode i = {
        .inode_number = inode,
        .deleted = 0,
        .size = sizeof(struct wfs_log_entry),
        .mode = S_IFDIR | 0755,
        .uid = getuid(),
        .gid = getgid(),
        .atime = time(NULL),
        .mtime = time(NULL),
    };
    struct wfs_log_entry new = {
        .inode = i,
    };

    memcpy(((struct wfs_sb*)disk_map)->head, &new, new.inode.size);
    ((struct wfs_sb*)disk_map)->head += new.inode.size;

*/
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

    return 0;
    /*
    for (int iter = 0; iter < curr_dir->inode.size / sizeof(struct wfs_dentry); iter++){
        printf("Dir Contains: %s\n",curr_dir->data[iter].path);

        //if (filler(buf, curr_dir->data[iter].name, &st, 0, FUSE_FILL_DIR_PLUS))
          //      break;
    }
    return 0;

    
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
