#ifndef EVAL_H
#define EVAL_H

#include "ast.h"

typedef enum {
    VAL_INT,
    VAL_STRING,
    VAL_FUNCTION,
    VAL_ARRAY,
    VAL_NATIVE_FUNCTION
} ValueType;

struct ValueArray;
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
        NativeFn native_fn;
    } as;
} Value;

typedef struct ValueArray {
    Value* elements;
    int count;
    int capacity;
} ValueArray;

typedef struct Environment {
    char** names;
    char** types;
    Value* values;
    int count;
    int capacity;
    struct Environment* parent;
} Environment;

Environment* Environment_create(Environment* parent);
void Environment_set(Environment* env, char* name, Value value, char* typeAnnotation);
Value Environment_get(Environment* env, char* name, int* found);
void Environment_destroy(Environment* env);

Value Eval_node(ASTNode* node, Environment* env);

#endif // EVAL_H
