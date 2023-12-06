#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wfs.h"

#define MAXENTRIES 256

int main(int argc, char *argv[]) {
    //printf("Not Implemented\n");
    //return 1;
    if (argc != 2){
        printf("Invalid Number of arguments.\n");
        return 1;
    }
    FILE* file = fopen(argv[1], "rb");
    if (file == NULL) {
        printf("Error opening file");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Allocate memory to store the file content
    char* buffer = (char *)malloc(file_size);
    if (buffer == NULL) {
        printf("Error allocating memory");
        fclose(file);
        return 1;
    }

    if (fread(buffer, 1, file_size, file) != file_size) {
        printf("Error reading file");
        fclose(file);
        free(buffer);
        return 1;
    }

    fclose(file);

    // Now 'buffer' contains the contents of the file
    struct wfs_log_entry* entries[MAXENTRIES] = {NULL};
    struct wfs_sb* superblock = (struct wfs_sb*)buffer;

    int offset = 0;
    int maxnode = 0;
    while (offset < (superblock->head - sizeof(struct wfs_sb))){
        struct wfs_log_entry* curr = (struct wfs_log_entry*)(buffer + sizeof(struct wfs_sb) + offset);
        int inode = curr->inode.inode_number;

        if (curr->inode.deleted == 0){
            printf("Found Inode %d\n", inode);
            entries[inode] = curr;
        } else {
            printf("Removed Inode %d\n", inode);
            entries[inode] = NULL;
        }
        offset += curr->inode.size + sizeof(struct wfs_inode);
    }

    char* outbuffer = (char *)malloc(file_size);
    offset = sizeof(struct wfs_sb);
    for (int i = 0; i < MAXENTRIES; i++){
        if (entries[i]){
            memcpy(outbuffer + offset, entries[i], sizeof(struct wfs_inode) + entries[i]->inode.size);
            offset += sizeof(struct wfs_inode) + entries[i]->inode.size;
        }
    }
    superblock->head = offset;
    memcpy(outbuffer, buffer, sizeof(struct wfs_sb));
    memset(outbuffer + offset, 0, file_size - (long) offset);

    free(buffer);

    // Open the file in binary mode for writing
    file = fopen(argv[1], "wb");
    if (file == NULL) {
        printf("Error opening file for writing");
        free(outbuffer);
        return 1;
    }

    // Write the modified content back to the file
    if (fwrite(outbuffer, 1, file_size, file) != file_size) {
        printf("Error writing to file");
        fclose(file);
        free(outbuffer);
        return 1;
    }

    fclose(file);
    free(outbuffer);

    return 0;   
}
