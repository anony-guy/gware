#include "vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct VFSNode {
    char* filename;
    char* content;
    struct VFSNode* next;
} VFSNode;

static VFSNode* vfs_head = NULL;

void vfs_init(void) {
    vfs_head = NULL;
}

void vfs_add_file(const char* filename, const char* content, int length) {
    VFSNode* node = (VFSNode*)malloc(sizeof(VFSNode));
    node->filename = strdup(filename);
    
    node->content = (char*)malloc(length + 1);
    memcpy(node->content, content, length);
    node->content[length] = '\0';
    
    node->next = vfs_head;
    vfs_head = node;
}

char* vfs_get_file(const char* filename) {
    VFSNode* curr = vfs_head;
    while (curr) {
        if (strcmp(curr->filename, filename) == 0) {
            return curr->content;
        }
        curr = curr->next;
    }
    return NULL;
}

// Bundle format:
// [4 bytes filename length]
// [filename]
// [4 bytes content length]
// [content]
// (Repeated until end of bundle)
void vfs_load_bundle(const char* bundle_data, int bundle_size) {
    int offset = 0;
    while (offset < bundle_size) {
        if (offset + 4 > bundle_size) break;
        
        int fname_len = 0;
        memcpy(&fname_len, bundle_data + offset, 4);
        offset += 4;
        
        if (offset + fname_len > bundle_size) break;
        char* fname = (char*)malloc(fname_len + 1);
        memcpy(fname, bundle_data + offset, fname_len);
        fname[fname_len] = '\0';
        offset += fname_len;
        
        if (offset + 4 > bundle_size) { free(fname); break; }
        
        int content_len = 0;
        memcpy(&content_len, bundle_data + offset, 4);
        offset += 4;
        
        if (offset + content_len > bundle_size) { free(fname); break; }
        vfs_add_file(fname, bundle_data + offset, content_len);
        offset += content_len;
        
        free(fname);
    }
}
