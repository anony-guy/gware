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
    Value v; 
    v.type = VAL_INT; 
    v.is_return = 0; 
    v.is_break = 0; 
    v.is_continue = 0; 
    v.as.ptr_val = db; // We use VAL_INT for type check but ptr_val for storage
    return v;
}

static Value native_sqlite_exec(int argCount, Value* args) {
    if (argCount < 2 || args[0].type != VAL_INT || args[1].type != VAL_STRING) {
        throw_error("sqlite_exec expects (db_handle, query_string, [bindings])");
    }
    sqlite3* db = (sqlite3*)args[0].as.ptr_val;
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, args[1].as.str_val, -1, &stmt, NULL) != SQLITE_OK) {
        throw_error("SQL error: %s", sqlite3_errmsg(db));
    }
    
    if (argCount >= 3 && args[2].type == VAL_ARRAY) {
        ValueArray* bind_args = args[2].as.arr_val;
        for (int i = 0; i < bind_args->count; i++) {
            Value val = bind_args->elements[i];
            if (val.type == VAL_INT) sqlite3_bind_int(stmt, i + 1, val.as.int_val);
            else if (val.type == VAL_STRING && val.as.str_val) sqlite3_bind_text(stmt, i + 1, val.as.str_val, -1, SQLITE_TRANSIENT);
            else if (val.type == VAL_STRING && !val.as.str_val) sqlite3_bind_null(stmt, i + 1);
        }
    }
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        char err[512];
        snprintf(err, sizeof(err), "SQL step error: %s", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        throw_error(err);
    }
    
    sqlite3_finalize(stmt);
    return createNull();
}

static Value native_sqlite_query(int argCount, Value* args) {
    if (argCount < 2 || args[0].type != VAL_INT || args[1].type != VAL_STRING) {
        throw_error("sqlite_query expects (db_handle, query_string, [bindings])");
    }
    sqlite3* db = (sqlite3*)args[0].as.ptr_val;
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, args[1].as.str_val, -1, &stmt, NULL) != SQLITE_OK) {
        throw_error("Failed to execute query: %s", sqlite3_errmsg(db));
    }
    
    if (argCount >= 3 && args[2].type == VAL_ARRAY) {
        ValueArray* bind_args = args[2].as.arr_val;
        for (int i = 0; i < bind_args->count; i++) {
            Value val = bind_args->elements[i];
            if (val.type == VAL_INT) sqlite3_bind_int(stmt, i + 1, val.as.int_val);
            else if (val.type == VAL_STRING && val.as.str_val) sqlite3_bind_text(stmt, i + 1, val.as.str_val, -1, SQLITE_TRANSIENT);
            else if (val.type == VAL_STRING && !val.as.str_val) sqlite3_bind_null(stmt, i + 1);
        }
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
    Value sqliteObj = createObject(4);
    
    Value open_val; open_val.type = VAL_NATIVE_FUNCTION; open_val.is_return = 0; open_val.as.native_fn = native_sqlite_open;
    Object_set_value(sqliteObj.as.obj_val, "open", open_val);
    
    Value exec_val; exec_val.type = VAL_NATIVE_FUNCTION; exec_val.is_return = 0; exec_val.as.native_fn = native_sqlite_exec;
    Object_set_value(sqliteObj.as.obj_val, "exec", exec_val);
    
    Value query_val; query_val.type = VAL_NATIVE_FUNCTION; query_val.is_return = 0; query_val.as.native_fn = native_sqlite_query;
    Object_set_value(sqliteObj.as.obj_val, "query", query_val);
    
    Value close_val; close_val.type = VAL_NATIVE_FUNCTION; close_val.is_return = 0; close_val.as.native_fn = native_sqlite_close;
    Object_set_value(sqliteObj.as.obj_val, "close", close_val);
    
    Environment_set(env, "sqlite", sqliteObj, NULL);
}
