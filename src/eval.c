#include "eval.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Value createNull() { Value v; v.type = VAL_STRING; v.is_return = 0; v.as.str_val = NULL; return v; }
static Value createInt(int i) { Value v; v.type = VAL_INT; v.is_return = 0; v.as.int_val = i; return v; }
static Value createString(char* s) { Value v; v.type = VAL_STRING; v.is_return = 0; v.as.str_val = strdup(s); return v; }
static Value createFunction(ASTNode* func) { Value v; v.type = VAL_FUNCTION; v.is_return = 0; v.as.func_val = func; return v; }

static Value createArray(int capacity) {
    Value v; 
    v.type = VAL_ARRAY; 
    v.is_return = 0; 
    v.as.arr_val = (ValueArray*)malloc(sizeof(ValueArray));
    v.as.arr_val->count = 0;
    v.as.arr_val->capacity = capacity > 0 ? capacity : 4;
    v.as.arr_val->elements = (Value*)malloc(sizeof(Value) * v.as.arr_val->capacity);
    return v;
}

static Value createNativeFunction(NativeFn fn) {
    Value v;
    v.type = VAL_NATIVE_FUNCTION;
    v.is_return = 0;
    v.as.native_fn = fn;
    return v;
}

Value native_read_file(int argCount, Value* args) {
    if (argCount != 1 || args[0].type != VAL_STRING) {
        printf("Runtime Error: read_file expects 1 string argument\n"); exit(1);
    }
    FILE* f = fopen(args[0].as.str_val, "r");
    if (!f) return createNull();
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    Value res = createString(buf);
    free(buf);
    return res;
}

Value native_write_file(int argCount, Value* args) {
    if (argCount != 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        printf("Runtime Error: write_file expects 2 string arguments\n"); exit(1);
    }
    FILE* f = fopen(args[0].as.str_val, "w");
    if (!f) return createInt(0);
    fputs(args[1].as.str_val, f);
    fclose(f);
    return createInt(1);
}

void Value_free(Value v) {
    if (v.type == VAL_STRING && v.as.str_val) free(v.as.str_val);
    if (v.type == VAL_ARRAY && v.as.arr_val) {
        for (int i = 0; i < v.as.arr_val->count; i++) {
            Value_free(v.as.arr_val->elements[i]);
        }
        free(v.as.arr_val->elements);
        free(v.as.arr_val);
    }
}

Value Value_copy(Value v) {
    Value copy = v;
    if (v.type == VAL_STRING && v.as.str_val) {
        copy.as.str_val = strdup(v.as.str_val);
    } else if (v.type == VAL_ARRAY && v.as.arr_val) {
        copy.as.arr_val = (ValueArray*)malloc(sizeof(ValueArray));
        copy.as.arr_val->count = v.as.arr_val->count;
        copy.as.arr_val->capacity = v.as.arr_val->capacity;
        copy.as.arr_val->elements = (Value*)malloc(sizeof(Value) * copy.as.arr_val->capacity);
        for (int i = 0; i < copy.as.arr_val->count; i++) {
            copy.as.arr_val->elements[i] = Value_copy(v.as.arr_val->elements[i]);
        }
    }
    return copy;
}

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

Environment* Environment_create(Environment* parent) {
    Environment* env = (Environment*)malloc(sizeof(Environment));
    env->names = NULL;
    env->types = NULL;
    env->values = NULL;
    env->count = 0;
    env->capacity = 0;
    env->parent = parent;
    
    // Register native functions in the global environment
    if (parent == NULL) {
        Environment_set(env, "read_file", createNativeFunction(native_read_file), NULL);
        Environment_set(env, "write_file", createNativeFunction(native_write_file), NULL);
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
    for (int i = 0; i < env->count; i++) {
        free(env->names[i]);
        if (env->types[i]) free(env->types[i]);
        Value_free(env->values[i]);
    }
    if (env->names) free(env->names);
    if (env->types) free(env->types);
    if (env->values) free(env->values);
    free(env);
}

static int isTruthy(Value v) {
    if (v.type == VAL_INT) return v.as.int_val != 0;
    if (v.type == VAL_STRING) return v.as.str_val && strlen(v.as.str_val) > 0;
    if (v.type == VAL_ARRAY) return v.as.arr_val && v.as.arr_val->count > 0;
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
                if (result.is_return) {
                    return result;
                }
            }
            return result;
        }
            
        case AST_SET_STATEMENT: {
            if (node->left->type == AST_INDEX_EXPRESSION) {
                if (node->left->left->type != AST_IDENTIFIER) {
                    printf("Runtime Error: Complex array assignments not supported yet\n"); exit(1);
                }
                char* varName = node->left->left->value;
                Value* ref = Environment_get_ref(env, varName);
                if (!ref || ref->type != VAL_ARRAY) {
                    printf("Runtime Error: Cannot index non-array variable '%s'\n", varName); exit(1);
                }
                Value idxVal = Eval_node(node->left->right, env);
                if (idxVal.type != VAL_INT) {
                    printf("Runtime Error: Array index must be an integer\n"); exit(1);
                }
                int idx = idxVal.as.int_val;
                if (idx < 0 || idx >= ref->as.arr_val->count) {
                    printf("Runtime Error: Array assignment out of bounds\n"); exit(1);
                }
                Value newVal = Eval_node(node->right, env);
                Value_free(ref->as.arr_val->elements[idx]);
                ref->as.arr_val->elements[idx] = Value_copy(newVal);
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
                if (result.is_return) return result;
            }
            return result;
        }
        
        case AST_FUNCTION_DECLARATION: {
            Value funcVal = createFunction(node);
            Environment_set(env, node->value, funcVal, NULL);
            return funcVal;
        }
        
        case AST_RETURN_STATEMENT: {
            Value val = createNull();
            if (node->left) val = Eval_node(node->left, env);
            val.is_return = 1;
            return val;
        }
        
        case AST_ARRAY_LITERAL: {
            Value arr = createArray(node->statementCount);
            for (int i = 0; i < node->statementCount; i++) {
                Value val = Eval_node(node->statements[i], env);
                arr.as.arr_val->elements[arr.as.arr_val->count++] = Value_copy(val);
            }
            return arr;
        }
        
        case AST_INDEX_EXPRESSION: {
            Value arrVal = Eval_node(node->left, env);
            if (arrVal.type != VAL_ARRAY && arrVal.type != VAL_STRING) {
                printf("Runtime Error: Cannot index a non-array/string\n"); exit(1);
            }
            Value idxVal = Eval_node(node->right, env);
            if (idxVal.type != VAL_INT) {
                printf("Runtime Error: Index must be an integer\n"); exit(1);
            }
            int idx = idxVal.as.int_val;
            
            if (arrVal.type == VAL_ARRAY) {
                if (idx < 0 || idx >= arrVal.as.arr_val->count) {
                    printf("Runtime Error: Array index %d out of bounds (count: %d)\n", idx, arrVal.as.arr_val->count); exit(1);
                }
                return Value_copy(arrVal.as.arr_val->elements[idx]);
            } else {
                if (idx < 0 || idx >= strlen(arrVal.as.str_val)) {
                    printf("Runtime Error: String index %d out of bounds\n", idx); exit(1);
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
                    return createInt(strcmp(leftVal.as.str_val, rightVal.as.str_val) == 0);
                }
                printf("Runtime Error: Cannot compare differing types with ==\n");
                exit(1);
            }
            
            if (leftVal.type != VAL_INT || rightVal.type != VAL_INT) {
                printf("Runtime Error: Math operations (+, -, *, /, <, >) require 'int' types.\n");
                exit(1);
            }
            
            if (strcmp(node->value, "+") == 0) return createInt(leftVal.as.int_val + rightVal.as.int_val);
            if (strcmp(node->value, "-") == 0) return createInt(leftVal.as.int_val - rightVal.as.int_val);
            if (strcmp(node->value, "*") == 0) return createInt(leftVal.as.int_val * rightVal.as.int_val);
            if (strcmp(node->value, "/") == 0) {
                if (rightVal.as.int_val == 0) {
                    printf("Runtime Error: Division by zero.\n");
                    exit(1);
                }
                return createInt(leftVal.as.int_val / rightVal.as.int_val);
            }
            if (strcmp(node->value, "<") == 0) return createInt(leftVal.as.int_val < rightVal.as.int_val);
            if (strcmp(node->value, ">") == 0) return createInt(leftVal.as.int_val > rightVal.as.int_val);
            
            return createNull();
        }
        
        case AST_CALL_EXPRESSION: {
            if (strcmp(node->left->value, "show") == 0) {
                Value val = Eval_node(node->right, env);
                if (val.type == VAL_INT) printf("%d\n", val.as.int_val);
                else if (val.type == VAL_STRING) printf("%s\n", val.as.str_val ? val.as.str_val : "null");
                else if (val.type == VAL_ARRAY) printf("[Array count=%d]\n", val.as.arr_val->count);
                return val;
            }
            printf("Runtime Error: unknown function '%s'\n", node->left->value);
            exit(1);
        }
        
        case AST_FUNCTION_CALL: {
            int found = 0;
            Value funcVal = Environment_get(env, node->value, &found);
            if (!found || (funcVal.type != VAL_FUNCTION && funcVal.type != VAL_NATIVE_FUNCTION)) {
                printf("Runtime Error: Undefined function '%s'\n", node->value);
                exit(1);
            }
            
            if (funcVal.type == VAL_NATIVE_FUNCTION) {
                Value* args = malloc(sizeof(Value) * node->parameterCount);
                for (int i = 0; i < node->parameterCount; i++) {
                    args[i] = Eval_node(node->parameters[i], env);
                }
                Value res = funcVal.as.native_fn(node->parameterCount, args);
                for (int i = 0; i < node->parameterCount; i++) {
                    Value_free(args[i]);
                }
                free(args);
                return res;
            }
            
            ASTNode* funcNode = funcVal.as.func_val;
            if (node->parameterCount != funcNode->parameterCount) {
                printf("Runtime Error: Function '%s' expects %d arguments, but got %d\n", node->value, funcNode->parameterCount, node->parameterCount);
                exit(1);
            }
            
            Environment* funcEnv = Environment_create(env);
            for (int i = 0; i < node->parameterCount; i++) {
                Value argVal = Eval_node(node->parameters[i], env);
                Environment_set(funcEnv, funcNode->parameters[i]->value, argVal, funcNode->parameters[i]->typeAnnotation);
            }
            
            Value result = Eval_node(funcNode->right, funcEnv);
            result.is_return = 0; // Unwind the return flag
            
            Environment_destroy(funcEnv);
            return result;
        }
    }
    return createNull();
}
