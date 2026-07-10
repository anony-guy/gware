#ifndef EVAL_H
#define EVAL_H

#include "ast.h"

typedef enum {
    VAL_INT,
    VAL_STRING,
    VAL_FUNCTION,
    VAL_ARRAY,
    VAL_OBJECT,
    VAL_NATIVE_FUNCTION
} ValueType;

struct ValueArray;
struct ValueObject;
struct Value;

typedef struct Value (*NativeFn)(int argCount, struct Value* args);

typedef struct Value {
    ValueType type;
    int is_return;
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
    Value* elements;
    int count;
    int capacity;
} ValueArray;

typedef struct ValueObject {
    char** keys;
    Value* values;
    int count;
    int capacity;
} ValueObject;

typedef struct Environment {
    char** names;
    char** types;
    Value* values;
    int count;
    int capacity;
    struct Environment* parent;
} Environment;

void Value_free(Value v);
Value Value_copy(Value v);

Value createNull();
Value createInt(int i);
Value createString(char* s);
Value createArray(int capacity);
Value createObject(int capacity);

Environment* get_global_env(void);
Value invokeFunction(Value funcVal, int argCount, Value* args, Environment* parentEnv);
Environment* Environment_create(Environment* parent);
void Environment_set(Environment* env, char* name, Value value, char* typeAnnotation);
Value Environment_get(Environment* env, char* name, int* found);
Value* Environment_get_ref(Environment* env, char* name);
void Environment_destroy(Environment* env);

Value Eval_node(struct ASTNode* node, Environment* env);
Value invokeFunction(Value funcVal, int argCount, Value* args, Environment* parentEnv);
void throw_error(const char* fmt, ...);

#endif // EVAL_H
