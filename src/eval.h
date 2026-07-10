#ifndef EVAL_H
#define EVAL_H

#include "ast.h"

typedef enum {
    VAL_INT,
    VAL_STRING,
    VAL_NULL
} ValueType;

typedef struct {
    ValueType type;
    union {
        int i;
        char* s;
    } data;
} Value;

typedef struct EnvVar {
    char* name;
    Value value;
    char* typeAnnotation;
    struct EnvVar* next;
} EnvVar;

typedef struct {
    EnvVar* head;
} Environment;

Environment* Environment_create();
void Environment_set(Environment* env, char* name, Value value, char* typeAnnotation);
Value Environment_get(Environment* env, char* name, int* found);
void Environment_destroy(Environment* env);

Value Eval_node(ASTNode* node, Environment* env);

#endif // EVAL_H
