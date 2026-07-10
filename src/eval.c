#include "eval.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Value createNull() { Value v; v.type = VAL_NULL; return v; }
static Value createInt(int i) { Value v; v.type = VAL_INT; v.data.i = i; return v; }
static Value createString(char* s) { Value v; v.type = VAL_STRING; v.data.s = strdup(s); return v; }

static void checkTypeMatch(char* annotation, ValueType vtype, char* varName) {
    if (!annotation) return; // Dynamic, anything goes
    if (strcmp(annotation, "int") == 0 && vtype != VAL_INT) {
        printf("Runtime Error: Type mismatch! Variable '%s' is declared as 'int' but received another type.\n", varName);
        exit(1);
    }
    if (strcmp(annotation, "string") == 0 && vtype != VAL_STRING) {
        printf("Runtime Error: Type mismatch! Variable '%s' is declared as 'string' but received another type.\n", varName);
        exit(1);
    }
}

Environment* Environment_create() {
    Environment* env = (Environment*)malloc(sizeof(Environment));
    env->head = NULL;
    return env;
}

void Environment_set(Environment* env, char* name, Value value, char* typeAnnotation) {
    EnvVar* curr = env->head;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            // Check type constraints on existing variable (either its own annotation, or a new one passed)
            char* constraint = curr->typeAnnotation ? curr->typeAnnotation : typeAnnotation;
            checkTypeMatch(constraint, value.type, name);
            
            if (curr->value.type == VAL_STRING) free(curr->value.data.s);
            curr->value = value;
            if (value.type == VAL_STRING) curr->value.data.s = strdup(value.data.s);
            
            // If it had no annotation but we are setting one now (rare in our parser but good practice)
            if (!curr->typeAnnotation && typeAnnotation) {
                curr->typeAnnotation = strdup(typeAnnotation);
            }
            return;
        }
        curr = curr->next;
    }
    
    // New variable
    checkTypeMatch(typeAnnotation, value.type, name);
    
    EnvVar* newVar = (EnvVar*)malloc(sizeof(EnvVar));
    newVar->name = strdup(name);
    newVar->value = value;
    if (value.type == VAL_STRING) newVar->value.data.s = strdup(value.data.s);
    newVar->typeAnnotation = typeAnnotation ? strdup(typeAnnotation) : NULL;
    newVar->next = env->head;
    env->head = newVar;
}

Value Environment_get(Environment* env, char* name, int* found) {
    EnvVar* curr = env->head;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            *found = 1;
            return curr->value;
        }
        curr = curr->next;
    }
    *found = 0;
    return createNull();
}

void Environment_destroy(Environment* env) {
    EnvVar* curr = env->head;
    while (curr) {
        EnvVar* next = curr->next;
        free(curr->name);
        if (curr->value.type == VAL_STRING) free(curr->value.data.s);
        if (curr->typeAnnotation) free(curr->typeAnnotation);
        free(curr);
        curr = next;
    }
    free(env);
}

static int isTruthy(Value v) {
    if (v.type == VAL_NULL) return 0;
    if (v.type == VAL_INT) return v.data.i != 0;
    if (v.type == VAL_STRING) return strlen(v.data.s) > 0;
    return 1;
}

Value Eval_node(ASTNode* node, Environment* env) {
    if (!node) return createNull();
    
    switch (node->type) {
        case AST_PROGRAM:
        case AST_BLOCK_STATEMENT: {
            Value result = createNull();
            for (int i = 0; i < node->statementCount; i++) {
                result = Eval_node(node->statements[i], env);
            }
            return result;
        }
            
        case AST_SET_STATEMENT: {
            Value val = Eval_node(node->right, env);
            Environment_set(env, node->left->value, val, node->typeAnnotation);
            return val;
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
                printf("Runtime Error: Undefined variable '%s'\n", node->value);
                exit(1);
            }
            return val;
        }
        
        case AST_IF_STATEMENT: {
            Value cond = Eval_node(node->left, env);
            if (isTruthy(cond)) {
                return Eval_node(node->right, env);
            }
            return createNull();
        }
        
        case AST_WHILE_STATEMENT: {
            Value result = createNull();
            while (isTruthy(Eval_node(node->left, env))) {
                result = Eval_node(node->right, env);
            }
            return result;
        }
        
        case AST_INFIX_EXPRESSION: {
            Value leftVal = Eval_node(node->left, env);
            Value rightVal = Eval_node(node->right, env);
            
            if (strcmp(node->value, "==") == 0) {
                if (leftVal.type == VAL_INT && rightVal.type == VAL_INT) {
                    return createInt(leftVal.data.i == rightVal.data.i);
                } else if (leftVal.type == VAL_STRING && rightVal.type == VAL_STRING) {
                    return createInt(strcmp(leftVal.data.s, rightVal.data.s) == 0);
                }
                printf("Runtime Error: Cannot compare differing types with ==\n");
                exit(1);
            }
            
            if (leftVal.type != VAL_INT || rightVal.type != VAL_INT) {
                printf("Runtime Error: Math operations (+, -, *, /, <, >) require 'int' types.\n");
                exit(1);
            }
            
            if (strcmp(node->value, "+") == 0) return createInt(leftVal.data.i + rightVal.data.i);
            if (strcmp(node->value, "-") == 0) return createInt(leftVal.data.i - rightVal.data.i);
            if (strcmp(node->value, "*") == 0) return createInt(leftVal.data.i * rightVal.data.i);
            if (strcmp(node->value, "/") == 0) {
                if (rightVal.data.i == 0) {
                    printf("Runtime Error: Division by zero.\n");
                    exit(1);
                }
                return createInt(leftVal.data.i / rightVal.data.i);
            }
            if (strcmp(node->value, "<") == 0) return createInt(leftVal.data.i < rightVal.data.i);
            if (strcmp(node->value, ">") == 0) return createInt(leftVal.data.i > rightVal.data.i);
            
            return createNull();
        }
        
        case AST_CALL_EXPRESSION: {
            if (strcmp(node->left->value, "show") == 0) {
                Value val = Eval_node(node->right, env);
                if (val.type == VAL_INT) printf("%d\n", val.data.i);
                else if (val.type == VAL_STRING) printf("%s\n", val.data.s);
                else if (val.type == VAL_NULL) printf("null\n");
                return val;
            }
            printf("Runtime Error: unknown function '%s'\n", node->left->value);
            exit(1);
        }
    }
    return createNull();
}
