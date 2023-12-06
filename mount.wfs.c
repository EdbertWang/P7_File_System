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
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <dirent.h>

#include "wfs.h"
#include <errno.h>

char* disk_map;
int curr_inode = 0;

// Refactored
static struct wfs_log_entry* find_by_inode(int inode){
    char* temp = (char*)disk_map + sizeof(struct wfs_sb);
    int offset = 0;
    struct wfs_log_entry* recent = NULL;

    while (offset < ((struct wfs_sb*)disk_map)->head - sizeof(struct wfs_sb)){
        struct wfs_log_entry* curr = (struct wfs_log_entry*)(temp + offset);
        if (curr->inode.inode_number == inode){
            if (curr->inode.deleted == 0){
                recent = curr;
            } else {
                recent = NULL;
            }
        }
        offset += curr->inode.size + sizeof(struct wfs_inode);
    }
    //printf("Inode number %d Not Found\n" , inode);
    return recent;
}

// Refactored
static int find_in_directory(struct wfs_log_entry* location, char* file_name){
    if (location == NULL){
        printf("Null Pointer Passed\n");
        return -1;
    }
    for (int i = 0; i < location->inode.size; i += sizeof(struct wfs_dentry)){
        struct wfs_dentry* curr = (struct wfs_dentry*) (location->data + i);
        if (strcmp(curr->name, file_name) == 0){
            return curr->inode_number;
        }
    }
    printf("File/Directory %s Not Found in inode %d\n" , file_name, location->inode.inode_number);
    return -1;
}

// No Refactoring Needed
static int wfs_getattr(const char *path, struct stat *stbuf) { // Stat
    printf("getattr with path arg = %s\n", path);
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

    char* temppath = malloc(sizeof(char) * PATH_LEN);
    strcpy(temppath, path);

    char* tokens[PATH_LEN];
    int count = 0;

    char *path_component = strtok(temppath, "/");

    while (path_component != NULL) {
        tokens[count] = path_component;
        count++;
        path_component = strtok(NULL, "/");
    }

    if(count > 0){
        //printf("Count = %d\n", count);
        for (int i = 0; i < count; i++) {
            //printf("Looking for dir %s\n", tokens[i]);
            int nextinode = find_in_directory(root, tokens[i]);
            root = find_by_inode(nextinode);
            if (!root){
                //printf("%s Path Not Found\n", tokens[i]);
                free(temppath);
                return -ENOENT;
            }
        }
    }
    free(temppath);

    stbuf->st_mode = root->inode.mode;
    stbuf->st_nlink = root->inode.links;
    stbuf->st_size = root->inode.size;
    stbuf->st_uid = root->inode.uid;
    stbuf->st_gid = root->inode.gid;

    return 0;
}

// Refactored
static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) { // Echo
    printf("mknod\n");

    if (strcmp(path,"/") == 0){ // If is root
        printf("Cannot remake root\n");
        return -EEXIST;
    }

    struct wfs_log_entry* parent =  find_by_inode(0);
    char* temppath = malloc(sizeof(char) * PATH_LEN);
    strcpy(temppath, path);

    char* tokens[PATH_LEN];
    int count = 0;
    char *path_component = strtok(temppath, "/");

    while(path_component != NULL) {
        tokens[count] = path_component;
        count++;
        path_component = strtok(NULL, "/");
    }

    if (count > 0) { // Find the parent of the directory
        for (int i = 0; i < count - 1; i++) {
            //printf("Looking for dir %s\n", tokens[i]);
            int nextinode = find_in_directory(parent, tokens[i]);
            parent = find_by_inode(nextinode);
            if (!parent){
                //printf("%s Path Not Found\n", tokens[i]);
                free(temppath);
                return -ENOENT;
            }
        }

        // Making new Parent Entry
        int new_inode = ++curr_inode;
        char* headentry = disk_map + ((struct wfs_sb*)disk_map)->head;
        memcpy(headentry, parent, parent->inode.size + sizeof(struct wfs_inode));

        // Build new dentry
        struct wfs_dentry new_dir = {
            .inode_number = new_inode,
        };
        strcpy(new_dir.name, tokens[count - 1]);
 
        // Insert New dentry
        memcpy(headentry + ((struct wfs_log_entry*) headentry)->inode.size + sizeof(struct wfs_inode), &new_dir, sizeof(struct wfs_dentry));

        // Update indicators
        ((struct wfs_log_entry*) headentry)->inode.size += sizeof(struct wfs_dentry);
        ((struct wfs_sb*)disk_map)->head += ((struct wfs_log_entry*) headentry)->inode.size + sizeof(struct wfs_inode);
        //parent->inode.deleted = 1;

        // Make new Log Entry
        // Create entry for new directory
        struct wfs_inode i = {
            .inode_number = new_inode,
            .deleted = 0,
            .size = 0,
            .mode = S_IFREG | mode,
            .uid = getuid(),
            .gid = getgid(),
            .atime = time(NULL),
            .mtime = time(NULL),
        };
        struct wfs_log_entry new = {
            .inode = i,
        };
        // Move to new insertion location
        headentry = disk_map + ((struct wfs_sb*)disk_map)->head;

        memcpy(headentry, &new, + sizeof(struct wfs_inode));
        ((struct wfs_sb*)disk_map)->head += + sizeof(struct wfs_inode);

        if (((struct wfs_sb*)disk_map)->head > CAP){
            free(temppath);
            return -ENOSPC;
        }

    }
    free(temppath);
    return 0; 
}


static int wfs_mkdir(const char* path, mode_t mode) {
    printf("mkdir\n");

    if (strcmp(path,"/") == 0){ // If is root
        printf("Cannot remake root\n");
        return -EEXIST;
    }

    struct wfs_log_entry* parent =  find_by_inode(0);
    char* temppath = malloc(sizeof(char) * PATH_LEN);
    strcpy(temppath, path);

    char* tokens[PATH_LEN];
    int count = 0;
    char *path_component = strtok(temppath, "/");

    while(path_component != NULL) {
        tokens[count] = path_component;
        count++;
        path_component = strtok(NULL, "/");
    }

    //TODO Changed from if(tokens) -> if(count > 0) as in anything exists there
    if (count > 0) { // Find the parent of the directory
        for (int i = 0; i < count - 1; i++) {
            //printf("Looking for dir %s\n", tokens[i]);
            int nextinode = find_in_directory(parent, tokens[i]);
            parent = find_by_inode(nextinode);
            if (!parent){
                //printf("%s Path Not Found\n", tokens[i]);
                free(temppath);
                return -ENOENT;
            }
        }

        // Making new Parent Entry
        int new_inode = ++curr_inode;
        char* headentry = disk_map + ((struct wfs_sb*)disk_map)->head;
        memcpy(headentry, parent, parent->inode.size + sizeof(struct wfs_inode));

        // Build new dentry
        struct wfs_dentry new_dir = {
            .inode_number = new_inode,
        };
        strcpy(new_dir.name, *(tokens + count - 1));
 
        // Insert New dentry
        memcpy(headentry + ((struct wfs_log_entry*) headentry)->inode.size + sizeof(struct wfs_inode), &new_dir, sizeof(struct wfs_dentry));

        // Update indicators
        ((struct wfs_log_entry*) headentry)->inode.size += sizeof(struct wfs_dentry);
        ((struct wfs_sb*)disk_map)->head += ((struct wfs_log_entry*) headentry)->inode.size + sizeof(struct wfs_inode);
        //parent->inode.deleted = 1;

        // Make new Log Entry
        // Create entry for new directory
        struct wfs_inode i = {
            .inode_number = new_inode,
            .deleted = 0,
            .size = 0,
            .mode = S_IFDIR | mode,
            .uid = getuid(),
            .gid = getgid(),
            .atime = time(NULL),
            .mtime = time(NULL),
        };
        struct wfs_log_entry new = {
            .inode = i,
        };
        // Move to new insertion location
        headentry = disk_map + ((struct wfs_sb*)disk_map)->head;

        memcpy(headentry, &new, sizeof(struct wfs_inode));
        ((struct wfs_sb*)disk_map)->head += sizeof(struct wfs_inode);

        if (((struct wfs_sb*)disk_map)->head > CAP){
            free(temppath);
            return -ENOSPC;
        }

    }
    free(temppath);
    return 0; 
}


// Refactored
// TODO: Might have errors with buffer
static int wfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) { // Cat
    if (strcmp(path,"/") == 0){
        printf("Cannot read root\n");
        return -EEXIST;
    }

    struct wfs_log_entry* currLog =  find_by_inode(0); //First is always root
    char* temppath = malloc(sizeof(char) * PATH_LEN);
    strcpy(temppath, path);

    char* tokens[PATH_LEN];
    int count = 0;
    char *path_component = strtok(temppath, "/");

    while(path_component != NULL) {
        tokens[count] = path_component;
        count++;
        path_component = strtok(NULL, "/");
    }

    for (int i = 0; i < count; i++) {
        //printf("Looking for dir %s\n", tokens[i]);
        int nextinode = find_in_directory(currLog, tokens[i]);
        currLog = find_by_inode(nextinode);
        if (!currLog){
            //printf("%s Path Not Found\n", tokens[i]);
            free(temppath);
            return -ENOENT;
        }
    }
    //currLog should be @ right log now.
    char* data = currLog->data;
    data += offset;

    if(data > ((char* )currLog + currLog->inode.size + sizeof(struct wfs_inode))){
        return 0;
    }

    memcpy(buf, data, size);

    //Returns the number of bytes transferred, or 0 if offset was at or beyond the end of the file. 
    return size;
}

// Refactored
// TODO: Might have errors with buffer
static int wfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi) { // Echo -> Write
    printf("writing %s to %s\n", buf, path);

    struct wfs_log_entry* file =  find_by_inode(0);
    char* temppath = malloc(sizeof(char) * PATH_LEN);
    strcpy(temppath, path);

    char* tokens[PATH_LEN];
    int count = 0;
    char *path_component = strtok(temppath, "/");

    while(path_component != NULL) {
        tokens[count] = path_component;
        count++;
        path_component = strtok(NULL, "/");
    }
    char newfile = 0;
    if (count > 0) { // Find the file
        for (int i = 0; i < count; i++) {
            //printf("Looking for dir %s\n", tokens[i]);
            int nextinode = find_in_directory(file, tokens[i]);
            file = find_by_inode(nextinode);
            if (!file){
                if (i != count - 1){
                    printf("%s Path Not Found\n", tokens[i]);
                    free(temppath);
                    return -ENOENT;
                } else {
                    newfile=1;
                }
            }
        }

        // Making new Entry
        char* headentry = disk_map + ((struct wfs_sb*)disk_map)->head;
        if (!newfile){ // Old file to reference first
            memcpy(headentry, file, file->inode.size + sizeof(struct wfs_inode));
            // Insert New Data
            memcpy(headentry + sizeof(struct wfs_inode) + offset, buf, size);
            // Update indicators
            ((struct wfs_log_entry*) headentry)->inode.size += size - offset;
            ((struct wfs_sb*)disk_map)->head += ((struct wfs_log_entry*) headentry)->inode.size + sizeof(struct wfs_inode);
            //file->inode.deleted = 1;
        } else{
            // Make new Log Entry
            struct wfs_inode i = {
                .inode_number = ++curr_inode,
                .deleted = 0,
                .size = offset + size,
                .mode = S_IFREG | 444,
                .uid = getuid(),
                .gid = getgid(),
                .atime = time(NULL),
                .mtime = time(NULL),
            };
            struct wfs_log_entry new = {
                .inode = i,
            };

            memcpy(headentry, &new, new.inode.size + sizeof(struct wfs_inode));
            memcpy(headentry + sizeof(struct wfs_inode) + offset, buf, size);
            ((struct wfs_sb*)disk_map)->head += new.inode.size + sizeof(struct wfs_inode);
        }
        if (((struct wfs_sb*)disk_map)->head > CAP){
            free(temppath);
            return -ENOSPC;
        }
    }
    free(temppath);

    return size; 
}

// Refactored
static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){ // cd or ls
    printf("readdir with %s\n", path);

    char* temppath = malloc(sizeof(char) * PATH_LEN);
    strcpy(temppath, path);

    char* tokens[PATH_LEN];
    int count = 0;
    char *path_component = strtok(temppath, "/");

    while(path_component != NULL) {
        tokens[count] = path_component;
        count++;
        path_component = strtok(NULL, "/");
    }

    struct wfs_log_entry* dir =  find_by_inode(0);
    for (int i = 0; i < count; i++) {
        //printf("Looking for dir %s\n", tokens[i]);
        int nextinode = find_in_directory(dir, tokens[i]);
        dir = find_by_inode(nextinode);
        if (!dir){
            printf("%s Path Not Found\n", tokens[i]);
            free(temppath);
            return -ENOENT;
        }
    }
    if ((dir->inode.mode & S_IFREG) == S_IFREG){
        printf("Not a directory\n");
        free(temppath);
        return 0;
    }
    for (int bufiter = 0; bufiter < dir->inode.size; bufiter += sizeof(struct wfs_dentry)){
        struct wfs_dentry* curr_item = (struct wfs_dentry*)((char*) dir->data + bufiter);
        //printf("%s\n", curr_item->name);
        filler(buf, curr_item->name, NULL, offset);
    }
    
    free(temppath);
    return 0;
}

static int wfs_unlink(const char* path){ // rm
    if(strcmp(path,"/") == 0){ // If is root
        printf("Cannot remake root\n");
        return -EEXIST;
    }

    char* headentry = disk_map + ((struct wfs_sb*)disk_map)->head;

    struct wfs_log_entry* parent =  find_by_inode(0); //Always start @ root
    char* temppath = malloc(sizeof(char) * PATH_LEN);
    strcpy(temppath, path);

    char* tokens[PATH_LEN];
    int count = 0;
    char *path_component = strtok(temppath, "/");

    while(path_component != NULL) {
        tokens[count] = path_component;
        count++;
        path_component = strtok(NULL, "/");
    }

    if (count > 0) { // Find the parent of the file to remove
        for (int i = 0; i < count - 1; i++) {
            //printf("Looking for dir %s\n", tokens[i]);
            int nextinode = find_in_directory(parent, tokens[i]);
            parent = find_by_inode(nextinode);
            if (!parent){
                //printf("%s Path Not Found\n", tokens[i]);
                free(temppath);
                return -ENOENT;
            }
        }

        //set the file to be removed
        int removedInode = find_in_directory(parent, tokens[count-1]);
        if(!removedInode){
            printf("Couldn't find file to remove\n");
            free(temppath);
            return -ENOENT;
        }

        //TODO CHECK IF ITS A FILE VS DIR
        struct wfs_log_entry* removed = find_by_inode(removedInode);
        removed->inode.deleted = 1;
        
        //Update the new parent inode & specifically log entry size.
        struct wfs_log_entry* updatedParent = (struct wfs_log_entry*)headentry;

        memcpy(updatedParent, parent, sizeof(struct wfs_inode));
        updatedParent->inode.size -= sizeof(struct wfs_dentry);

        //Run through parent data entries
        int entries = 0;
        for (int i = 0; i < parent->inode.size; i += sizeof(struct wfs_dentry)){
            struct wfs_dentry* curr = (struct wfs_dentry*) ((char*)parent->data + i);
            if (strcmp(curr->name, tokens[count-1]) != 0){//If the actual file to delete, don't copy
                memcpy((struct wfs_dentry*)updatedParent->data + entries, curr, sizeof(struct wfs_dentry));
                entries++;
            }
            
        }

        //actually put updatedParent @ the head
        memcpy(headentry, updatedParent, updatedParent->inode.size + sizeof(struct wfs_inode));

        //Update headentry & superblock's head
        ((struct wfs_sb*)disk_map)->head += updatedParent->inode.size + sizeof(struct wfs_inode);

        if (((struct wfs_sb*)disk_map)->head > CAP){
            free(temppath);
            return -ENOSPC;
        }
    }
    free(temppath);
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

    int disk_fd = open(disk_path, O_RDWR);
    if (disk_fd < 0){
        printf("Disk Open Error\n");
        close(disk_fd);
        return 1;
    }

    struct stat st;
    stat(disk_path, &st);

    disk_map = (char*) mmap(0,st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, disk_fd, 0);

    struct wfs_sb* superblock = (struct wfs_sb*)disk_map;
    if (superblock->magic != WFS_MAGIC){
        printf("Not a wfs filesystem");
        munmap(disk_map, st.st_size);
        close(disk_fd);
        return 1;
    }

    return fuse_main(argc, argv, &ops, NULL);
}
