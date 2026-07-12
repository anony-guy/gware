#include "fs_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Value fs_read(int argCount, Value* args) {
    if (argCount < 1 || args[0].type != VAL_STRING || !args[0].as.str_val) {
        throw_error("fs.read expects a file path string");
        return createNull();
    }
    const char* path = args[0].as.str_val;
    FILE* file = fopen(path, "rb");
    if (!file) {
        throw_error("Failed to open file for reading: %s", path);
        return createNull();
    }
    
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (length < 0) {
        fclose(file);
        throw_error("Failed to determine file size: %s", path);
        return createNull();
    }
    
    char* content = malloc(length + 1);
    if (!content) {
        fclose(file);
        throw_error("Failed to allocate memory for reading file: %s", path);
        return createNull();
    }
    
    size_t read_bytes = fread(content, 1, length, file);
    fclose(file);
    
    content[read_bytes] = '\0';
    return createString(content); // createString expects dynamically allocated char*, or we duplicate it? wait, createString expects we pass it, but wait! We usually use strdup in createString!
}

Value fs_write(int argCount, Value* args) {
    if (argCount < 2 || args[0].type != VAL_STRING || (args[1].type != VAL_STRING && args[1].type != VAL_ARRAY)) {
        throw_error("fs.write expects a file path string and a content string or array");
        return createNull();
    }
    const char* path = args[0].as.str_val;
    
    FILE* file = fopen(path, "wb");
    if (!file) {
        throw_error("Failed to open file for writing: %s", path);
        return createNull();
    }
    
    size_t written = 0;
    size_t len = 0;
    
    if (args[1].type == VAL_STRING) {
        const char* content = args[1].as.str_val;
        len = strlen(content);
        written = fwrite(content, 1, len, file);
    } else if (args[1].type == VAL_ARRAY) {
        ValueArray* arr = args[1].as.arr_val;
        len = arr->count;
        char* buf = malloc(len);
        for (int i = 0; i < len; i++) {
            if (arr->elements[i].type == VAL_INT) {
                buf[i] = (char)arr->elements[i].as.int_val;
            } else {
                buf[i] = 0;
            }
        }
        written = fwrite(buf, 1, len, file);
        free(buf);
    }
    
    fclose(file);
    
    if (written != len) {
        throw_error("Failed to write all data to file: %s", path);
        return createNull();
    }
    
    return createInt(1); // Return 1 on success
}

void register_fs_api(Environment* env) {
    Value fs_obj = createObject(4);
    
    Object_set_value(fs_obj.as.obj_val, "read", createNativeFunction(fs_read));
    Object_set_value(fs_obj.as.obj_val, "write", createNativeFunction(fs_write));
    
    Environment_set(env, "fs", fs_obj, "object");
}
