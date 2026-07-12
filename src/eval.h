#ifndef EVAL_H
#define EVAL_H

#include "ast.h"

typedef enum {
    VAL_INT,
    VAL_STRING,
    VAL_ARRAY,
    VAL_OBJECT,
    VAL_FUNCTION,
    VAL_NATIVE_FUNCTION
} ValueType;

struct ValueArray;
struct ValueObject;
struct Value;
struct Environment;

typedef struct GCObject {
    ValueType type;
    int marked;
    struct GCObject* next;
} GCObject;

typedef struct Value (*NativeFn)(int argCount, struct Value* args);

typedef struct Value {
    ValueType type;
    int is_return;
    int is_break;
    int is_continue;
    union {
        int int_val;
        char* str_val;
        struct ASTNode* func_val;
        struct ValueArray* arr_val;
        struct ValueObject* obj_val;
        NativeFn native_fn;
        void* ptr_val;
    } as;
} Value;

typedef struct ValueArray {
    GCObject gc;
    struct Value* elements;
    int count;
    int capacity;
} ValueArray;

typedef struct ValueObject {
    GCObject gc;
    char** keys;
    struct Value* values;
    int count;
    int capacity;
} ValueObject;

typedef struct Environment {
    char** names;
    char** types;
    int* is_const;
    struct Value* values;
    int count;
    int capacity;
    struct Environment* parent;
    struct Environment* next_active;
} Environment;

void Value_free(Value v);
Value Value_copy(Value v);

Value createNull();
Value createInt(int i);
Value createString(char* s);
Value createArray(int capacity);
Value createObject(int capacity);
Value createNativeFunction(NativeFn fn);
void Object_set_value(struct ValueObject* obj, const char* key, Value val);

Environment* get_global_env(void);
Value invokeFunction(Value funcVal, int argCount, Value* args, Environment* parentEnv, Value* thisObj);
Environment* Environment_create(Environment* parent);
void Environment_define(Environment* env, char* name, Value value, char* typeAnnotation, int isConst);
void Environment_set(Environment* env, char* name, Value value, char* typeAnnotation);
Value Environment_get(Environment* env, char* name, int* found);
Value* Environment_get_ref(Environment* env, char* name);
void Environment_destroy(Environment* env);
void gc_collect(void);

Value Eval_node(struct ASTNode* node, Environment* env);
Value invokeFunction(Value funcVal, int argCount, Value* args, Environment* parentEnv, Value* thisObj);
void throw_error(const char* fmt, ...);

#endif // EVAL_H
