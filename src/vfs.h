#ifndef VFS_H
#define VFS_H

void vfs_init(void);
void vfs_add_file(const char* filename, const char* content, int length);
char* vfs_get_file(const char* filename);

// Used by packager to read the appended bundle at runtime
void vfs_load_bundle(const char* bundle_data, int bundle_size);

#endif
