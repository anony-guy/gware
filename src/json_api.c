#include "json_api.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Value cjson_to_value(cJSON* item) {
    if (!item) return createNull();

    if (cJSON_IsNull(item)) {
        return createNull();
    } else if (cJSON_IsTrue(item)) {
        return createInt(1);
    } else if (cJSON_IsFalse(item)) {
        return createInt(0);
    } else if (cJSON_IsNumber(item)) {
        return createInt(item->valueint);
    } else if (cJSON_IsString(item)) {
        return createString(item->valuestring);
    } else if (cJSON_IsArray(item)) {
        int count = cJSON_GetArraySize(item);
        Value arr = createArray(count);
        cJSON* child = item->child;
        while (child) {
            arr.as.arr_val->elements[arr.as.arr_val->count++] = cjson_to_value(child);
            child = child->next;
        }
        return arr;
    } else if (cJSON_IsObject(item)) {
        int count = cJSON_GetArraySize(item);
        Value obj = createObject(count);
        cJSON* child = item->child;
        while (child) {
            obj.as.obj_val->keys[obj.as.obj_val->count] = strdup(child->string);
            obj.as.obj_val->values[obj.as.obj_val->count] = cjson_to_value(child);
            obj.as.obj_val->count++;
            child = child->next;
        }
        return obj;
    }
    return createNull();
}

static cJSON* value_to_cjson(Value v) {
    if (v.type == VAL_INT) {
        return cJSON_CreateNumber(v.as.int_val);
    } else if (v.type == VAL_STRING) {
        if (!v.as.str_val) return cJSON_CreateNull();
        return cJSON_CreateString(v.as.str_val);
    } else if (v.type == VAL_ARRAY) {
        cJSON* arr = cJSON_CreateArray();
        for (int i = 0; i < v.as.arr_val->count; i++) {
            cJSON_AddItemToArray(arr, value_to_cjson(v.as.arr_val->elements[i]));
        }
        return arr;
    } else if (v.type == VAL_OBJECT) {
        cJSON* obj = cJSON_CreateObject();
        for (int i = 0; i < v.as.obj_val->count; i++) {
            cJSON_AddItemToObject(obj, v.as.obj_val->keys[i], value_to_cjson(v.as.obj_val->values[i]));
        }
        return obj;
    }
    return cJSON_CreateNull();
}

Value native_json_parse(int argCount, Value* args) {
    if (argCount != 1 || args[0].type != VAL_STRING) {
        throw_error("json_parse expects 1 string argument");
    }
    cJSON* json = cJSON_Parse(args[0].as.str_val);
    if (!json) {
        throw_error("Failed to parse JSON string");
    }
    Value result = cjson_to_value(json);
    cJSON_Delete(json);
    return result;
}

Value native_json_stringify(int argCount, Value* args) {
    if (argCount != 1) {
        throw_error("json_stringify expects 1 argument");
    }
    cJSON* json = value_to_cjson(args[0]);
    char* str = cJSON_PrintUnformatted(json);
    Value result = createString(str);
    free(str);
    cJSON_Delete(json);
    return result;
}
