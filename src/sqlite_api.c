#include "sqlite_api.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper to convert sqlite3 column type to Gware Value
static Value sqlite_column_to_value(sqlite3_stmt* stmt, int col) {
    int type = sqlite3_column_type(stmt, col);
    if (type == SQLITE_INTEGER) {
        return createInt(sqlite3_column_int(stmt, col));
    } else if (type == SQLITE_TEXT) {
        return createString((char*)sqlite3_column_text(stmt, col));
    } else if (type == SQLITE_NULL) {
        return createNull();
    }
    // Fallback to string for FLOAT/BLOB for now
    return createString((char*)sqlite3_column_text(stmt, col));
}

static Value native_sqlite_open(int argCount, Value* args) {
    if (argCount != 1 || args[0].type != VAL_STRING) {
        throw_error("sqlite_open expects 1 string argument (db path)");
    }
    sqlite3* db;
    if (sqlite3_open(args[0].as.str_val, &db) != SQLITE_OK) {
        throw_error("Cannot open database: %s", sqlite3_errmsg(db));
    }
    Value v; v.type = VAL_INT; v.is_return = 0; v.as.ptr_val = db; // We use VAL_INT for type check but ptr_val for storage
    return v;
}

static Value native_sqlite_exec(int argCount, Value* args) {
    if (argCount != 2 || args[0].type != VAL_INT || args[1].type != VAL_STRING) {
        throw_error("sqlite_exec expects (db_handle, query_string)");
    }
    sqlite3* db = (sqlite3*)args[0].as.ptr_val;
    char* err_msg = NULL;
    if (sqlite3_exec(db, args[1].as.str_val, 0, 0, &err_msg) != SQLITE_OK) {
        char err[512];
        snprintf(err, sizeof(err), "SQL error: %s", err_msg);
        sqlite3_free(err_msg);
        throw_error(err);
    }
    return createNull();
}

static Value native_sqlite_query(int argCount, Value* args) {
    if (argCount != 2 || args[0].type != VAL_INT || args[1].type != VAL_STRING) {
        throw_error("sqlite_query expects (db_handle, query_string)");
    }
    sqlite3* db = (sqlite3*)args[0].as.ptr_val;
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, args[1].as.str_val, -1, &stmt, NULL) != SQLITE_OK) {
        throw_error("Failed to execute query: %s", sqlite3_errmsg(db));
    }
    
    Value rows = createArray(4);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int cols = sqlite3_column_count(stmt);
        Value row = createObject(cols);
        for (int i = 0; i < cols; i++) {
            row.as.obj_val->keys[row.as.obj_val->count] = strdup(sqlite3_column_name(stmt, i));
            row.as.obj_val->values[row.as.obj_val->count] = sqlite_column_to_value(stmt, i);
            row.as.obj_val->count++;
        }
        
        if (rows.as.arr_val->count >= rows.as.arr_val->capacity) {
            rows.as.arr_val->capacity *= 2;
            rows.as.arr_val->elements = realloc(rows.as.arr_val->elements, sizeof(Value) * rows.as.arr_val->capacity);
        }
        rows.as.arr_val->elements[rows.as.arr_val->count++] = row;
    }
    
    sqlite3_finalize(stmt);
    return rows;
}

static Value native_sqlite_close(int argCount, Value* args) {
    if (argCount != 1 || args[0].type != VAL_INT) {
        throw_error("sqlite_close expects 1 argument (db_handle)");
    }
    sqlite3* db = (sqlite3*)args[0].as.ptr_val;
    sqlite3_close(db);
    return createNull();
}

void register_sqlite_api(Environment* env) {
    Value open_val; open_val.type = VAL_NATIVE_FUNCTION; open_val.is_return = 0; open_val.as.native_fn = native_sqlite_open;
    Environment_set(env, "sqlite_open", open_val, NULL);
    
    Value exec_val; exec_val.type = VAL_NATIVE_FUNCTION; exec_val.is_return = 0; exec_val.as.native_fn = native_sqlite_exec;
    Environment_set(env, "sqlite_exec", exec_val, NULL);
    
    Value query_val; query_val.type = VAL_NATIVE_FUNCTION; query_val.is_return = 0; query_val.as.native_fn = native_sqlite_query;
    Environment_set(env, "sqlite_query", query_val, NULL);
    
    Value close_val; close_val.type = VAL_NATIVE_FUNCTION; close_val.is_return = 0; close_val.as.native_fn = native_sqlite_close;
    Environment_set(env, "sqlite_close", close_val, NULL);
}
