#include "eval.h"
#ifndef GWARE_WASM
#include <setjmp.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "color.h"
#include "vfs.h"

#ifndef GWARE_WASM
jmp_buf jmp_stack[64];
int jmp_stack_ptr = -1;
#endif
char last_error_msg[1024];

typedef struct ExecStackNode {
    ASTNode* ast_node;
    struct ExecStackNode* prev;
} ExecStackNode;

#ifdef GWARE_WASM
#define THREAD_LOCAL
#else
#define THREAD_LOCAL __thread
#endif

ExecStackNode* exec_stack_top = NULL;

void throw_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error_msg, sizeof(last_error_msg), fmt, args);
    va_end(args);
#ifndef GWARE_WASM
    if (jmp_stack_ptr >= 0) {
        longjmp(jmp_stack[jmp_stack_ptr], 1);
    } else {
#endif
        fprintf(stderr, ANSI_COLOR_RED "Runtime Error: %s\n" ANSI_COLOR_RESET, last_error_msg);
        ExecStackNode* cur = exec_stack_top;
        int last_line = -1;
        while (cur) {
            if (cur->ast_node && cur->ast_node->file && cur->ast_node->line > 0) {
                if (cur->ast_node->line != last_line) {
                    fprintf(stderr, "  at %s:%d\n", cur->ast_node->file, cur->ast_node->line);
                    last_line = cur->ast_node->line;
                }
            }
            cur = cur->prev;
        }
        exit(1);
#ifndef GWARE_WASM
    }
#endif
}

#ifndef GWARE_WASM
#include "net.h"
#endif
#include "json_api.h"
#ifndef GWARE_WASM
#include "sqlite_api.h"
#include "tcp_api.h"
#endif
#include "lexer.h"
#include "parser.h"
#ifndef GWARE_WASM
#include "net.h"
#endif
#include <string.h>
#include <pthread.h>

GCObject* global_gc_list = NULL;
Environment* global_active_envs = NULL;
pthread_mutex_t gc_mutex = PTHREAD_MUTEX_INITIALIZER;
static int alloc_count = 0;
#define GC_THRESHOLD 1000

int has_error = 0;
Value createNull() { Value v; v.type = VAL_STRING; v.is_return = 0; v.as.str_val = NULL; return v; }
Value createInt(int i) { Value v; v.type = VAL_INT; v.is_return = 0; v.as.int_val = i; return v; }
Value createString(char* s) { 
    Value v; v.type = VAL_STRING; v.is_return = 0; 
    if (!s) { v.as.str_val = NULL; return v; }
    GCObject* obj = (GCObject*)malloc(sizeof(GCObject) + strlen(s) + 1);
    obj->type = VAL_STRING;
    obj->marked = 0;
    pthread_mutex_lock(&gc_mutex);
    obj->next = global_gc_list;
    global_gc_list = obj;
    pthread_mutex_unlock(&gc_mutex);
    char* str = (char*)(obj + 1);
    strcpy(str, s);
    v.as.str_val = str;
    alloc_count++;
    return v; 
}
Value createFunction(ASTNode* func) { Value v; v.type = VAL_FUNCTION; v.is_return = 0; v.as.func_val = func; return v; }

Value createObject(int capacity) {
    Value v; 
    v.type = VAL_OBJECT; 
    v.is_return = 0; 
    ValueObject* obj_val = (ValueObject*)malloc(sizeof(ValueObject));
    obj_val->gc.type = VAL_OBJECT;
    obj_val->gc.marked = 0;
    pthread_mutex_lock(&gc_mutex);
    obj_val->gc.next = global_gc_list;
    global_gc_list = (GCObject*)obj_val;
    pthread_mutex_unlock(&gc_mutex);
    v.as.obj_val = obj_val;
    v.as.obj_val->count = 0;
    v.as.obj_val->capacity = capacity > 0 ? capacity : 4;
    v.as.obj_val->keys = (char**)malloc(sizeof(char*) * v.as.obj_val->capacity);
    v.as.obj_val->values = (Value*)malloc(sizeof(Value) * v.as.obj_val->capacity);
    alloc_count++;
    return v;
}

void Object_set_value(ValueObject* obj, const char* key, Value val) {
    for (int i = 0; i < obj->count; i++) {
        if (strcmp(obj->keys[i], key) == 0) {
            Value_free(obj->values[i]);
            obj->values[i] = Value_copy(val);
            return;
        }
    }
    if (obj->count >= obj->capacity) {
        obj->capacity = obj->capacity == 0 ? 4 : obj->capacity * 2;
        obj->keys = (char**)realloc(obj->keys, sizeof(char*) * obj->capacity);
        obj->values = (Value*)realloc(obj->values, sizeof(Value) * obj->capacity);
    }
    obj->keys[obj->count] = strdup(key);
    obj->values[obj->count] = Value_copy(val);
    obj->count++;
}

Value createArray(int capacity) {
    Value v; 
    v.type = VAL_ARRAY; 
    v.is_return = 0; 
    ValueArray* arr_val = (ValueArray*)malloc(sizeof(ValueArray));
    arr_val->gc.type = VAL_ARRAY;
    arr_val->gc.marked = 0;
    pthread_mutex_lock(&gc_mutex);
    arr_val->gc.next = global_gc_list;
    global_gc_list = (GCObject*)arr_val;
    pthread_mutex_unlock(&gc_mutex);
    v.as.arr_val = arr_val;
    v.as.arr_val->count = 0;
    v.as.arr_val->capacity = capacity > 0 ? capacity : 4;
    v.as.arr_val->elements = (Value*)malloc(sizeof(Value) * v.as.arr_val->capacity);
    alloc_count++;
    return v;
}

Value createNativeFunction(NativeFn fn) {
    Value v;
    v.type = VAL_NATIVE_FUNCTION;
    v.is_return = 0;
    v.as.native_fn = fn;
    return v;
}



#ifndef GWARE_WASM
Value native_fetch(int argCount, Value* args) {
    if (argCount < 1 || args[0].type != VAL_STRING) return createNull();
    char* res = fetch_url_ext(args[0].as.str_val, "GET", NULL);
    if (!res) return createNull();
    Value v = createString(res);
    free(res);
    return v;
}

typedef struct {
    Value func;
    Value arg;
} SpawnArgs;

void* spawn_thread_func(void* arg) {
    SpawnArgs* sa = (SpawnArgs*)arg;
    Environment* env = Environment_create(get_global_env());
    Value argsArr[1] = { sa->arg };
    invokeFunction(sa->func, (sa->arg.type == VAL_STRING && sa->arg.as.str_val == NULL) ? 0 : 1, argsArr, env, NULL);
    Environment_destroy(env);
    Value_free(sa->func);
    Value_free(sa->arg);
    free(sa);
    return NULL;
}

Value native_spawn(int argCount, Value* args) {
    if (argCount < 1 || (args[0].type != VAL_FUNCTION && args[0].type != VAL_NATIVE_FUNCTION)) {
        throw_error("spawn expects a function");
        return createNull();
    }
    SpawnArgs* sa = malloc(sizeof(SpawnArgs));
    sa->func = Value_copy(args[0]);
    sa->arg = argCount > 1 ? Value_copy(args[1]) : createNull();
    
    pthread_t t;
    pthread_create(&t, NULL, spawn_thread_func, sa);
    pthread_detach(t);
    return createInt(1);
}

#ifdef _WIN32
void __stdcall Sleep(unsigned long dwMilliseconds);
#else
#include <unistd.h>
#endif
Value native_sleep(int argCount, Value* args) {
    if (argCount < 1 || args[0].type != VAL_INT) return createNull();
#ifdef _WIN32
    Sleep(args[0].as.int_val);
#else
    usleep(args[0].as.int_val * 1000);
#endif
    return createNull();
}

typedef struct {
    Value func;
    int ms;
    int is_interval;
} TimerArgs;

void* timer_thread_func(void* arg) {
    TimerArgs* ta = (TimerArgs*)arg;
    while (1) {
#ifdef _WIN32
        Sleep(ta->ms);
#else
        usleep(ta->ms * 1000);
#endif
        Environment* env = Environment_create(get_global_env());
        invokeFunction(ta->func, 0, NULL, env, NULL);
        Environment_destroy(env);
        if (!ta->is_interval) break;
    }
    Value_free(ta->func);
    free(ta);
    return NULL;
}

Value native_set_timeout(int argCount, Value* args) {
    if (argCount < 2 || (args[0].type != VAL_FUNCTION && args[0].type != VAL_NATIVE_FUNCTION) || args[1].type != VAL_INT) {
        throw_error("setTimeout expects (function, ms)");
        return createNull();
    }
    TimerArgs* ta = malloc(sizeof(TimerArgs));
    ta->func = Value_copy(args[0]);
    ta->ms = args[1].as.int_val;
    ta->is_interval = 0;
    
    pthread_t t;
    pthread_create(&t, NULL, timer_thread_func, ta);
    pthread_detach(t);
    return createInt(1);
}

Value native_set_interval(int argCount, Value* args) {
    if (argCount < 2 || (args[0].type != VAL_FUNCTION && args[0].type != VAL_NATIVE_FUNCTION) || args[1].type != VAL_INT) {
        throw_error("setInterval expects (function, ms)");
        return createNull();
    }
    TimerArgs* ta = malloc(sizeof(TimerArgs));
    ta->func = Value_copy(args[0]);
    ta->ms = args[1].as.int_val;
    ta->is_interval = 1;
    
    pthread_t t;
    pthread_create(&t, NULL, timer_thread_func, ta);
    pthread_detach(t);
    return createInt(1);
}
#endif
Value native_uuid(int argCount, Value* args) {
    char uuid[37];
    const char *chars = "0123456789abcdef";
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid[i] = '-';
        } else if (i == 14) {
            uuid[i] = '4';
        } else if (i == 19) {
            uuid[i] = chars[(rand() % 4) + 8];
        } else {
            uuid[i] = chars[rand() % 16];
        }
    }
    uuid[36] = '\0';
    return createString(uuid);
}

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
Value native_base64_encode(int argCount, Value* args) {
    if (argCount < 1 || args[0].type != VAL_STRING) return createNull();
    char* src = args[0].as.str_val;
    int len = strlen(src);
    int out_len = 4 * ((len + 2) / 3);
    char* out = malloc(out_len + 1);
    int i, j;
    for (i = 0, j = 0; i < len;) {
        uint32_t octet_a = i < len ? (unsigned char)src[i++] : 0;
        uint32_t octet_b = i < len ? (unsigned char)src[i++] : 0;
        uint32_t octet_c = i < len ? (unsigned char)src[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        out[j++] = base64_table[(triple >> 18) & 0x3F];
        out[j++] = base64_table[(triple >> 12) & 0x3F];
        out[j++] = base64_table[(triple >> 6) & 0x3F];
        out[j++] = base64_table[triple & 0x3F];
    }
    for (int k = 0; k < (3 - len % 3) % 3; k++) out[out_len - 1 - k] = '=';
    out[out_len] = '\0';
    Value res = createString(out);
    free(out);
    return res;
}

// --- String Stdlib ---
Value native_string_length(int argCount, Value* args) {
    if (argCount != 1 || args[0].type != VAL_STRING) throw_error("string_length expects 1 string");
    return createInt(args[0].as.str_val ? strlen(args[0].as.str_val) : 0);
}

Value native_string_concat(int argCount, Value* args) {
    if (argCount != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) throw_error("string_concat expects 2 strings");
    char* s1 = args[0].as.str_val ? args[0].as.str_val : "";
    char* s2 = args[1].as.str_val ? args[1].as.str_val : "";
    char* res = malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(res, s1);
    strcat(res, s2);
    Value v = createString(res);
    free(res);
    return v;
}

Value native_string_split(int argCount, Value* args) {
    if (argCount != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) throw_error("string_split expects (string, delimiter)");
    char* str = args[0].as.str_val ? args[0].as.str_val : "";
    char* delim = args[1].as.str_val ? args[1].as.str_val : "";
    
    Value arr = createArray(4);
    if (strlen(str) == 0) return arr;
    
    char* temp = strdup(str);
    char* token = strtok(temp, delim);
    while (token) {
        if (arr.as.arr_val->count >= arr.as.arr_val->capacity) {
            arr.as.arr_val->capacity *= 2;
            arr.as.arr_val->elements = realloc(arr.as.arr_val->elements, sizeof(Value) * arr.as.arr_val->capacity);
        }
        arr.as.arr_val->elements[arr.as.arr_val->count++] = createString(token);
        token = strtok(NULL, delim);
    }
    free(temp);
    return arr;
}

Value native_string_replace(int argCount, Value* args) {
    if (argCount != 3 || args[0].type != VAL_STRING || args[1].type != VAL_STRING || args[2].type != VAL_STRING) {
        throw_error("string_replace expects (string, target, replacement)");
    }
    char* str = args[0].as.str_val ? args[0].as.str_val : "";
    char* target = args[1].as.str_val;
    char* replacement = args[2].as.str_val ? args[2].as.str_val : "";
    if (!target || strlen(target) == 0) return createString(str);
    
    char* pos = strstr(str, target);
    if (!pos) return createString(str);
    
    int new_len = strlen(str) - strlen(target) + strlen(replacement);
    char* res = malloc(new_len + 1);
    int prefix_len = pos - str;
    strncpy(res, str, prefix_len);
    res[prefix_len] = '\0';
    strcat(res, replacement);
    strcat(res, pos + strlen(target));
    
    Value v = createString(res);
    free(res);
    return v;
}


void Value_free(Value v) {
    // Handled by GC
}

Value Value_copy(Value v) {
    return v; // Shallow copy
}

static void checkTypeMatch(char* annotation, ValueType vtype, char* varName) {
    if (!annotation) return; // Dynamic, anything goes
    if (strcmp(annotation, "int") == 0 && vtype != VAL_INT) {
        throw_error("Type mismatch! Variable '%s' is declared as 'int' but received another type.", varName);
    }
    if (strcmp(annotation, "string") == 0 && vtype != VAL_STRING) {
        throw_error("Type mismatch! Variable '%s' is declared as 'string' but received another type.", varName);
    }
}

static Environment* global_env = NULL;

Environment* get_global_env(void) {
    return global_env;
}

Environment* Environment_create(Environment* parent) {
    Environment* env = (Environment*)malloc(sizeof(Environment));
    env->names = NULL;
    env->types = NULL;
    env->values = NULL;
    env->count = 0;
    env->capacity = 0;
    env->parent = parent;
    
    pthread_mutex_lock(&gc_mutex);
    env->next_active = global_active_envs;
    global_active_envs = env;
    pthread_mutex_unlock(&gc_mutex);
    
    // Register native functions in the global environment
    if (parent == NULL) {
        global_env = env;
#ifndef GWARE_WASM
        Environment_set(env, "fetch", createNativeFunction(native_fetch), NULL);
        Environment_set(env, "spawn", createNativeFunction(native_spawn), NULL);
        Environment_set(env, "sleep", createNativeFunction(native_sleep), NULL);
#endif
        Environment_set(env, "null", createNull(), NULL);
#ifndef GWARE_WASM
        Environment_set(env, "setTimeout", createNativeFunction(native_set_timeout), NULL);
        Environment_set(env, "setInterval", createNativeFunction(native_set_interval), NULL);
#endif
        Environment_set(env, "generate_uuid", createNativeFunction(native_uuid), NULL);
        Environment_set(env, "base64_encode", createNativeFunction(native_base64_encode), NULL);
        Environment_set(env, "json_parse", createNativeFunction(native_json_parse), NULL);
        Environment_set(env, "json_stringify", createNativeFunction(native_json_stringify), NULL);
        
        // String Stdlib
        Environment_set(env, "string_length", createNativeFunction(native_string_length), NULL);
        Environment_set(env, "string_concat", createNativeFunction(native_string_concat), NULL);
        Environment_set(env, "string_split", createNativeFunction(native_string_split), NULL);
        Environment_set(env, "string_replace", createNativeFunction(native_string_replace), NULL);

#ifndef GWARE_WASM
        register_sqlite_api(env);
        register_tcp_api(env);
#endif
    }
    return env;
}

void Environment_set(Environment* env, char* name, Value value, char* typeAnnotation) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->names[i], name) == 0) {
            char* constraint = env->types[i] ? env->types[i] : typeAnnotation;
            checkTypeMatch(constraint, value.type, name);
            Value_free(env->values[i]);
            env->values[i] = Value_copy(value);
            if (!env->types[i] && typeAnnotation) {
                env->types[i] = strdup(typeAnnotation);
            }
            return;
        }
    }
    checkTypeMatch(typeAnnotation, value.type, name);
    if (env->count >= env->capacity) {
        env->capacity = env->capacity == 0 ? 4 : env->capacity * 2;
        env->names = (char**)realloc(env->names, sizeof(char*) * env->capacity);
        env->types = (char**)realloc(env->types, sizeof(char*) * env->capacity);
        env->values = (Value*)realloc(env->values, sizeof(Value) * env->capacity);
    }
    env->names[env->count] = strdup(name);
    env->types[env->count] = typeAnnotation ? strdup(typeAnnotation) : NULL;
    env->values[env->count] = Value_copy(value);
    env->count++;
}

Value Environment_get(Environment* env, char* name, int* found) {
    Environment* curr = env;
    while (curr) {
        for (int i = 0; i < curr->count; i++) {
            if (strcmp(curr->names[i], name) == 0) {
                *found = 1;
                return curr->values[i];
            }
        }
        curr = curr->parent;
    }
    *found = 0;
    return createNull();
}

Value* Environment_get_ref(Environment* env, char* name) {
    Environment* curr = env;
    while (curr) {
        for (int i = 0; i < curr->count; i++) {
            if (strcmp(curr->names[i], name) == 0) {
                return &curr->values[i];
            }
        }
        curr = curr->parent;
    }
    return NULL;
}

void Environment_destroy(Environment* env) {
    if (!env) return;
    pthread_mutex_lock(&gc_mutex);
    if (global_active_envs == env) {
        global_active_envs = env->next_active;
    } else {
        Environment* curr = global_active_envs;
        while (curr && curr->next_active != env) {
            curr = curr->next_active;
        }
        if (curr) {
            curr->next_active = env->next_active;
        }
    }
    pthread_mutex_unlock(&gc_mutex);
    for (int i = 0; i < env->count; i++) {
        free(env->names[i]);
        if (env->types[i]) free(env->types[i]);
    }
    if (env->names) free(env->names);
    if (env->types) free(env->types);
    if (env->values) free(env->values);
    free(env);
}

void mark_value(Value v) {
    if (v.type == VAL_STRING && v.as.str_val) {
        GCObject* obj = (GCObject*)v.as.str_val - 1;
        if (!obj->marked) obj->marked = 1;
    } else if (v.type == VAL_ARRAY && v.as.arr_val) {
        GCObject* obj = (GCObject*)v.as.arr_val;
        if (!obj->marked) {
            obj->marked = 1;
            for (int i=0; i<v.as.arr_val->count; i++) mark_value(v.as.arr_val->elements[i]);
        }
    } else if (v.type == VAL_OBJECT && v.as.obj_val) {
        GCObject* obj = (GCObject*)v.as.obj_val;
        if (!obj->marked) {
            obj->marked = 1;
            for (int i=0; i<v.as.obj_val->count; i++) mark_value(v.as.obj_val->values[i]);
        }
    }
}

void gc_collect() {
    pthread_mutex_lock(&gc_mutex);
    Environment* env = global_active_envs;
    while (env) {
        for (int i = 0; i < env->count; i++) {
            mark_value(env->values[i]);
        }
        env = env->next_active;
    }
    
    GCObject** curr = &global_gc_list;
    while (*curr) {
        if (!(*curr)->marked) {
            GCObject* unreached = *curr;
            *curr = unreached->next;
            
            if (unreached->type == VAL_STRING) {
            } else if (unreached->type == VAL_OBJECT) {
                ValueObject* obj = (ValueObject*)unreached;
                for (int i=0; i<obj->count; i++) {
                    free(obj->keys[i]);
                    Value_free(obj->values[i]);
                }
                free(obj->keys);
                free(obj->values);
            } else if (unreached->type == VAL_ARRAY) {
                ValueArray* arr = (ValueArray*)unreached;
                for (int i=0; i<arr->count; i++) {
                    Value_free(arr->elements[i]);
                }
                free(arr->elements);
            }
            free(unreached);
        } else {
            (*curr)->marked = 0;
            curr = &(*curr)->next;
        }
    }
    pthread_mutex_unlock(&gc_mutex);
}

static int isTruthy(Value v) {
    if (v.type == VAL_INT) return v.as.int_val != 0;
    if (v.type == VAL_STRING) return v.as.str_val && strlen(v.as.str_val) > 0;
    
    if (v.type == VAL_OBJECT) return v.as.obj_val && v.as.obj_val->count > 0;
    if (v.type == VAL_ARRAY) return v.as.arr_val && v.as.arr_val->count > 0;
    return 1;
}

Value invokeFunction(Value funcVal, int argCount, Value* args, Environment* parentEnv, Value* thisObj) {
    if (funcVal.type == VAL_NATIVE_FUNCTION) {
        return funcVal.as.native_fn(argCount, args);
    }
    if (funcVal.type == VAL_FUNCTION) {
        ASTNode* funcNode = funcVal.as.func_val;
        Environment* funcEnv = Environment_create(parentEnv);
        for (int i = 0; i < funcNode->parameterCount; i++) {
            Value argVal = (i < argCount) ? args[i] : createNull();
            Environment_set(funcEnv, funcNode->parameters[i]->value, argVal, funcNode->parameters[i]->typeAnnotation);
        }
        Value res = Eval_node(funcNode->right, funcEnv);
        Environment_destroy(funcEnv);
        if (res.is_return) {
            res.is_return = 0;
            return res;
        }
        return res;
    }
    throw_error("Attempted to call a non-function");
    return createNull();
}

Value Eval_node_inner(ASTNode* node, Environment* env);
Value Eval_node(ASTNode* node, Environment* env) {
    if (!node) return createNull();
    ExecStackNode stack_node;
    stack_node.ast_node = node;
    stack_node.prev = exec_stack_top;
    exec_stack_top = &stack_node;

    Value res = Eval_node_inner(node, env);

    exec_stack_top = stack_node.prev;
    return res;
}

Value Eval_node_inner(ASTNode* node, Environment* env) {
    if (!node) return createNull();
    
    switch (node->type) {
        case AST_PROGRAM:
        case AST_BLOCK_STATEMENT: {
            Value result = createNull();
            for (int i = 0; i < node->statementCount; i++) {
                if (alloc_count > GC_THRESHOLD) {
                    alloc_count = 0;
                    gc_collect();
                }
                result = Eval_node(node->statements[i], env);
                if (result.is_return) return result;
            }
            return result;
        }
            
        case AST_IMPORT_STATEMENT: {
            if (!node->value) return createNull();
            char* filename = node->value;
            char* source = vfs_get_file(filename);
            int from_vfs = 1;
            
            if (!source) {
                from_vfs = 0;
                FILE* fp = fopen(filename, "r");
                if (!fp) {
                    char err[512];
                    snprintf(err, sizeof(err), "Import error: Could not open file '%s'", filename);
                    throw_error(err);
                    return createNull();
                }
                fseek(fp, 0, SEEK_END);
                long fsize = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                source = malloc(fsize + 1);
                fread(source, 1, fsize, fp);
                source[fsize] = 0;
                fclose(fp);
            }

            Lexer* l = Lexer_create(source, filename);
            Parser* p = Parser_create(l);
            ASTNode* ast = Parser_parseProgram(p);
            Value result = Eval_node(ast, env);
            ASTNode_destroy(ast);
            Parser_destroy(p);
            Lexer_destroy(l);
            if (!from_vfs) {
                free(source);
            }
            return result;
        }
            
        case AST_SET_STATEMENT: {
            if (node->left->type == AST_INDEX_EXPRESSION) {
                if (node->left->left->type != AST_IDENTIFIER) {
                    throw_error("Complex array assignments not supported yet");
                }
                char* varName = node->left->left->value;
                Value* ref = Environment_get_ref(env, varName);
                if (!ref || (ref->type != VAL_ARRAY && ref->type != VAL_OBJECT)) {
                    throw_error("Cannot index non-array/object variable '%s'", varName);
                }
                Value idxVal = Eval_node(node->left->right, env);
                Value newVal = Eval_node(node->right, env);
                if (ref->type == VAL_ARRAY) {
                    if (idxVal.type != VAL_INT) throw_error("Array index must be an integer");
                    int idx = idxVal.as.int_val;
                    if (idx < 0 || idx >= ref->as.arr_val->count) throw_error("Array assignment out of bounds");
                    Value_free(ref->as.arr_val->elements[idx]);
                    ref->as.arr_val->elements[idx] = Value_copy(newVal);
                } else if (ref->type == VAL_OBJECT) {
                    if (idxVal.type != VAL_STRING) throw_error("Object key must be a string");
                    char* key = idxVal.as.str_val;
                    int found = 0;
                    for (int i = 0; i < ref->as.obj_val->count; i++) {
                        if (strcmp(ref->as.obj_val->keys[i], key) == 0) {
                            Value_free(ref->as.obj_val->values[i]);
                            ref->as.obj_val->values[i] = Value_copy(newVal);
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        if (ref->as.obj_val->count >= ref->as.obj_val->capacity) {
                            ref->as.obj_val->capacity *= 2;
                            ref->as.obj_val->keys = realloc(ref->as.obj_val->keys, sizeof(char*) * ref->as.obj_val->capacity);
                            ref->as.obj_val->values = realloc(ref->as.obj_val->values, sizeof(Value) * ref->as.obj_val->capacity);
                        }
                        ref->as.obj_val->keys[ref->as.obj_val->count] = strdup(key);
                        ref->as.obj_val->values[ref->as.obj_val->count] = Value_copy(newVal);
                        ref->as.obj_val->count++;
                    }
                }
                return newVal;
            } else {
                Value val = Eval_node(node->right, env);
                Environment_set(env, node->left->value, val, node->typeAnnotation);
                return val;
            }
        }
        
        case AST_EXPRESSION_STATEMENT:
            return Eval_node(node->left, env);
            
        case AST_INTEGER_LITERAL:
            return createInt(atoi(node->value));
            
        case AST_STRING_LITERAL:
            return createString(node->value);
            
        case AST_IDENTIFIER: {
            int found = 0;
            Value val = Environment_get(env, node->value, &found);
            if (!found) {
                throw_error("Undefined variable '%s'", node->value);
            }
            return Value_copy(val);
        }
        
        case AST_IF_STATEMENT: {
            Value condition = Eval_node(node->left, env);
            if (isTruthy(condition)) {
                return Eval_node(node->right, env);
            }
            return createNull();
        }
        
        case AST_TERNARY_EXPRESSION: {
            Value condition = Eval_node(node->left, env);
            if (isTruthy(condition)) {
                return Eval_node(node->right->statements[0], env);
            } else {
                return Eval_node(node->right->statements[1], env);
            }
        }
        
        case AST_TRY_STATEMENT: {
#ifndef GWARE_WASM
            jmp_stack_ptr++;
            if (setjmp(jmp_stack[jmp_stack_ptr]) == 0) {
#endif
                Value res = Eval_node(node->left, env);
#ifndef GWARE_WASM
                jmp_stack_ptr--;
#endif
                return res;
#ifndef GWARE_WASM
            } else {
                jmp_stack_ptr--;
                if (node->right) {
                    Environment* catchEnv = Environment_create(env);
                    if (node->value) {
                        Environment_set(catchEnv, node->value, createString(last_error_msg), NULL);
                    }
                    Value res = Eval_node(node->right, catchEnv);
                    Environment_destroy(catchEnv);
                    return res;
                }
                return createNull();
            }
#endif
        }
        
        case AST_WHILE_STATEMENT: {
            Value ret = createNull();
            while (1) {
                if (alloc_count > GC_THRESHOLD) {
                    alloc_count = 0;
                    gc_collect();
                }
                Value condition = Eval_node(node->left, env);
                if (!isTruthy(condition)) break;
                ret = Eval_node(node->right, env);
                if (ret.is_return) return ret;
                if (ret.is_break) { ret.is_break = 0; break; }
                if (ret.is_continue) { ret.is_continue = 0; continue; }
            }
            return ret;
        }
        
        case AST_FOR_IN_STATEMENT: {
            Value arrayVal = Eval_node(node->left, env);
            if (arrayVal.type != VAL_ARRAY) {
                throw_error("for-in loop requires an array");
            }
            Value ret = createNull();
            for (int i = 0; i < arrayVal.as.arr_val->count; i++) {
                if (alloc_count > GC_THRESHOLD) {
                    alloc_count = 0;
                    gc_collect();
                }
                Environment* blockEnv = Environment_create(env);
                Environment_set(blockEnv, node->value, arrayVal.as.arr_val->elements[i], NULL);
                ret = Eval_node(node->right, blockEnv);
                if (ret.is_return) {
                    return ret;
                }
                if (ret.is_break) { ret.is_break = 0; break; }
                if (ret.is_continue) { ret.is_continue = 0; continue; }
            }
            return ret;
        }
        
        case AST_FUNCTION_DECLARATION: {
            Value funcVal = createFunction(node);
            Environment_set(env, node->value, funcVal, NULL);
            return funcVal;
        }
        
        case AST_RETURN_STATEMENT: {
            Value ret = createNull();
            if (node->left) {
                ret = Eval_node(node->left, env);
            }
            ret.is_return = 1;
            return ret;
        }
        
        case AST_BREAK_STATEMENT: {
            Value ret = createNull();
            ret.is_break = 1;
            return ret;
        }
        
        case AST_CONTINUE_STATEMENT: {
            Value ret = createNull();
            ret.is_continue = 1;
            return ret;
        }
        
        case AST_ARRAY_LITERAL: {
            Value arr = createArray(node->statementCount);
            for (int i = 0; i < node->statementCount; i++) {
                Value val = Eval_node(node->statements[i], env);
                arr.as.arr_val->elements[arr.as.arr_val->count++] = Value_copy(val);
            }
            return arr;
        }
        
        case AST_OBJECT_LITERAL: {
            Value obj = createObject(node->statementCount / 2);
            for (int i = 0; i < node->statementCount; i += 2) {
                Value keyVal = Eval_node(node->statements[i], env);
                Value valVal = Eval_node(node->statements[i+1], env);
                if (keyVal.type != VAL_STRING) throw_error("Object keys must be strings");
                obj.as.obj_val->keys[obj.as.obj_val->count] = strdup(keyVal.as.str_val);
                obj.as.obj_val->values[obj.as.obj_val->count] = Value_copy(valVal);
                obj.as.obj_val->count++;
            }
            return obj;
        }
        
        case AST_INDEX_EXPRESSION: {
            Value arrVal = Eval_node(node->left, env);
            if (arrVal.type != VAL_ARRAY && arrVal.type != VAL_STRING && arrVal.type != VAL_OBJECT) {
                throw_error("Cannot index a non-array/string/object");
            }
            Value idxVal = Eval_node(node->right, env);
            if (arrVal.type == VAL_OBJECT) {
                if (idxVal.type != VAL_STRING) throw_error("Object index must be a string");
                char* key = idxVal.as.str_val;
                for (int i = 0; i < arrVal.as.obj_val->count; i++) {
                    if (strcmp(arrVal.as.obj_val->keys[i], key) == 0) {
                        return Value_copy(arrVal.as.obj_val->values[i]);
                    }
                }
                return createNull();
            }
            
            if (idxVal.type != VAL_INT) {
                throw_error("Index must be an integer");
            }
            int idx = idxVal.as.int_val;
            
            if (arrVal.type == VAL_ARRAY) {
                if (idx < 0 || idx >= arrVal.as.arr_val->count) {
                    throw_error("Array index %d out of bounds (count: %d)", idx, arrVal.as.arr_val->count);
                }
                return Value_copy(arrVal.as.arr_val->elements[idx]);
            } else {
                if (idx < 0 || idx >= strlen(arrVal.as.str_val)) {
                    throw_error("String index %d out of bounds", idx);
                }
                char buf[2] = { arrVal.as.str_val[idx], '\0' };
                return createString(buf);
            }
        }
        
        case AST_INFIX_EXPRESSION: {
            Value leftVal = Eval_node(node->left, env);
            Value rightVal = Eval_node(node->right, env);
            
            if (strcmp(node->value, "==") == 0) {
                if (leftVal.type == VAL_INT && rightVal.type == VAL_INT) {
                    return createInt(leftVal.as.int_val == rightVal.as.int_val);
                } else if (leftVal.type == VAL_STRING && rightVal.type == VAL_STRING) {
                    if (leftVal.as.str_val == NULL && rightVal.as.str_val == NULL) return createInt(1);
                    if (leftVal.as.str_val == NULL || rightVal.as.str_val == NULL) return createInt(0);
                    return createInt(strcmp(leftVal.as.str_val, rightVal.as.str_val) == 0);
                } else if (leftVal.type == VAL_STRING && leftVal.as.str_val == NULL) {
                    return createInt(0); // comparing null with int or other
                } else if (rightVal.type == VAL_STRING && rightVal.as.str_val == NULL) {
                    return createInt(0);
                }
                throw_error("Cannot compare differing types with ==");
            }
            
            if (strcmp(node->value, "!=") == 0) {
                if (leftVal.type == VAL_INT && rightVal.type == VAL_INT) {
                    return createInt(leftVal.as.int_val != rightVal.as.int_val);
                } else if (leftVal.type == VAL_STRING && rightVal.type == VAL_STRING) {
                    if (leftVal.as.str_val == NULL && rightVal.as.str_val == NULL) return createInt(0);
                    if (leftVal.as.str_val == NULL || rightVal.as.str_val == NULL) return createInt(1);
                    return createInt(strcmp(leftVal.as.str_val, rightVal.as.str_val) != 0);
                } else if (leftVal.type == VAL_STRING && leftVal.as.str_val == NULL) {
                    return createInt(1);
                } else if (rightVal.type == VAL_STRING && rightVal.as.str_val == NULL) {
                    return createInt(1);
                }
                throw_error("Cannot compare differing types with !=");
            }
            
            if (strcmp(node->value, "+") == 0) {
                if (leftVal.type == VAL_STRING && rightVal.type == VAL_STRING) {
                    int len = strlen(leftVal.as.str_val) + strlen(rightVal.as.str_val) + 1;
                    char* newStr = malloc(len);
                    strcpy(newStr, leftVal.as.str_val);
                    strcat(newStr, rightVal.as.str_val);
                    Value res = createString(newStr);
                    free(newStr);
                    return res;
                }
            }
            
            if (leftVal.type != VAL_INT || rightVal.type != VAL_INT) {
                throw_error("Math operations (+, -, *, /, <, >) require matching valid types.");
            }
            
            if (strcmp(node->value, "+") == 0) return createInt(leftVal.as.int_val + rightVal.as.int_val);
            if (strcmp(node->value, "-") == 0) return createInt(leftVal.as.int_val - rightVal.as.int_val);
            if (strcmp(node->value, "*") == 0) return createInt(leftVal.as.int_val * rightVal.as.int_val);
            if (strcmp(node->value, "/") == 0) {
                if (rightVal.as.int_val == 0) throw_error("Division by zero.");
                return createInt(leftVal.as.int_val / rightVal.as.int_val);
            }
            if (strcmp(node->value, "<") == 0) return createInt(leftVal.as.int_val < rightVal.as.int_val);
            if (strcmp(node->value, ">") == 0) return createInt(leftVal.as.int_val > rightVal.as.int_val);
            if (strcmp(node->value, "&") == 0) return createInt(leftVal.as.int_val & rightVal.as.int_val);
            if (strcmp(node->value, "|") == 0) return createInt(leftVal.as.int_val | rightVal.as.int_val);
            if (strcmp(node->value, "^") == 0) return createInt(leftVal.as.int_val ^ rightVal.as.int_val);
            if (strcmp(node->value, "<<") == 0) return createInt(leftVal.as.int_val << rightVal.as.int_val);
            if (strcmp(node->value, ">>") == 0) return createInt(leftVal.as.int_val >> rightVal.as.int_val);
            
            return createNull();
        }
        
        case AST_CALL_EXPRESSION: 
        case AST_FUNCTION_CALL: {
            int is_show = 0;
            char* func_name = NULL;
            if (node->type == AST_FUNCTION_CALL && node->value) {
                func_name = node->value;
            } else if (node->left && node->left->type == AST_IDENTIFIER && node->left->value) {
                func_name = node->left->value;
            }
            
            if (func_name && strcmp(func_name, "show") == 0) {
                Value val = createNull();
                if (node->parameterCount > 0) val = Eval_node(node->parameters[0], env);
                if (val.type == VAL_INT) printf("%d\n", val.as.int_val);
                else if (val.type == VAL_STRING) printf("%s\n", val.as.str_val ? val.as.str_val : "null");
                else if (val.type == VAL_ARRAY) printf("[Array count=%d]\n", val.as.arr_val->count);
                else if (val.type == VAL_OBJECT) printf("[Object keys=%d]\n", val.as.obj_val->count);
                fflush(stdout);
                return val;
            }
            
            if (func_name && strcmp(func_name, "assert") == 0) {
                if (node->parameterCount < 1) throw_error("assert requires at least 1 argument");
                Value condition = Eval_node(node->parameters[0], env);
                if (!isTruthy(condition)) {
                    if (node->parameterCount > 1) {
                        Value msg = Eval_node(node->parameters[1], env);
                        throw_error("Assertion failed: %s", msg.type == VAL_STRING && msg.as.str_val ? msg.as.str_val : "unknown");
                    } else {
                        throw_error("Assertion failed");
                    }
                }
                return createNull();
            }
            
            Value funcVal;
            if (node->type == AST_FUNCTION_CALL && node->value) {
                int found = 0;
                funcVal = Environment_get(env, node->value, &found);
            } else {
                funcVal = Eval_node(node->left, env);
            }
            if (funcVal.type != VAL_FUNCTION && funcVal.type != VAL_NATIVE_FUNCTION) {
                throw_error("Attempted to call a non-function");
                return createNull();
            }
            
            Value* thisObj = NULL;
            Value objVal;
            if (node->left && node->left->type == AST_INDEX_EXPRESSION) {
                objVal = Eval_node(node->left->left, env);
                thisObj = &objVal;
            }
            
            Value* args = malloc(sizeof(Value) * node->parameterCount);
            for (int i = 0; i < node->parameterCount; i++) {
                args[i] = Eval_node(node->parameters[i], env);
            }
            Value res = invokeFunction(funcVal, node->parameterCount, args, env, thisObj);
            for (int i = 0; i < node->parameterCount; i++) {
                Value_free(args[i]);
            }
            free(args);
            return res;
        }
    }
    return createNull();
}
