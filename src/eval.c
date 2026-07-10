#include "eval.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "color.h"

static jmp_buf jmp_stack[64];
static int jmp_stack_ptr = -1;
static char last_error_msg[1024];

void throw_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error_msg, sizeof(last_error_msg), fmt, args);
    va_end(args);
    if (jmp_stack_ptr >= 0) {
        longjmp(jmp_stack[jmp_stack_ptr], 1);
    } else {
        printf(ANSI_COLOR_RED "Runtime Error: %s\n" ANSI_COLOR_RESET, last_error_msg);
        exit(1);
    }
}

#include "net.h"
#include <string.h>
#include "json_api.h"
#include "sqlite_api.h"
#include "tcp_api.h"
#include "lexer.h"
#include "parser.h"

int has_error = 0;
Value createNull() { Value v; v.type = VAL_STRING; v.is_return = 0; v.as.str_val = NULL; return v; }
Value createInt(int i) { Value v; v.type = VAL_INT; v.is_return = 0; v.as.int_val = i; return v; }
Value createString(char* s) { Value v; v.type = VAL_STRING; v.is_return = 0; v.as.str_val = strdup(s); return v; }
Value createFunction(ASTNode* func) { Value v; v.type = VAL_FUNCTION; v.is_return = 0; v.as.func_val = func; return v; }


Value createObject(int capacity) {
    Value v; 
    v.type = VAL_OBJECT; 
    v.is_return = 0; 
    v.as.obj_val = (ValueObject*)malloc(sizeof(ValueObject));
    v.as.obj_val->count = 0;
    v.as.obj_val->capacity = capacity > 0 ? capacity : 4;
    v.as.obj_val->keys = (char**)malloc(sizeof(char*) * v.as.obj_val->capacity);
    v.as.obj_val->values = (Value*)malloc(sizeof(Value) * v.as.obj_val->capacity);
    return v;
}

Value createArray(int capacity) {
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
        throw_error("read_file expects 1 string argument");
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
        throw_error("write_file expects 2 string arguments");
    }
    FILE* f = fopen(args[0].as.str_val, "w");
    if (!f) return createInt(0);
    fputs(args[1].as.str_val, f);
    fclose(f);
    return createInt(1);
}

Value native_fetch(int argCount, Value* args) {
    if (argCount != 1 || args[0].type != VAL_STRING) {
        throw_error("fetch expects (url)");
    }
    char* res = fetch_url(args[0].as.str_val);
    if (!res) return createNull();
    Value v = createString(res);
    free(res);
    return v;
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
    if (v.type == VAL_STRING && v.as.str_val) free(v.as.str_val);
    
    if (v.type == VAL_OBJECT && v.as.obj_val) {
        for (int i = 0; i < v.as.obj_val->count; i++) {
            free(v.as.obj_val->keys[i]);
            Value_free(v.as.obj_val->values[i]);
        }
        free(v.as.obj_val->keys);
        free(v.as.obj_val->values);
        free(v.as.obj_val);
    }
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
    } else 
    if (v.type == VAL_OBJECT && v.as.obj_val) {
        copy.as.obj_val = (ValueObject*)malloc(sizeof(ValueObject));
        copy.as.obj_val->count = v.as.obj_val->count;
        copy.as.obj_val->capacity = v.as.obj_val->capacity;
        copy.as.obj_val->keys = (char**)malloc(sizeof(char*) * copy.as.obj_val->capacity);
        copy.as.obj_val->values = (Value*)malloc(sizeof(Value) * copy.as.obj_val->capacity);
        for (int i = 0; i < copy.as.obj_val->count; i++) {
            copy.as.obj_val->keys[i] = strdup(v.as.obj_val->keys[i]);
            copy.as.obj_val->values[i] = Value_copy(v.as.obj_val->values[i]);
        }
    }
    if (v.type == VAL_ARRAY && v.as.arr_val) {
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
    
    // Register native functions in the global environment
    if (parent == NULL) {
        global_env = env;
        Environment_set(env, "read_file", createNativeFunction(native_read_file), NULL);
        Environment_set(env, "write_file", createNativeFunction(native_write_file), NULL);
        Environment_set(env, "fetch", createNativeFunction(native_fetch), NULL);
        Environment_set(env, "json_parse", createNativeFunction(native_json_parse), NULL);
        Environment_set(env, "json_stringify", createNativeFunction(native_json_stringify), NULL);
        
        // String Stdlib
        Environment_set(env, "string_length", createNativeFunction(native_string_length), NULL);
        Environment_set(env, "string_concat", createNativeFunction(native_string_concat), NULL);
        Environment_set(env, "string_split", createNativeFunction(native_string_split), NULL);
        Environment_set(env, "string_replace", createNativeFunction(native_string_replace), NULL);

        register_sqlite_api(env);
        register_tcp_api(env);
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
    
    if (v.type == VAL_OBJECT) return v.as.obj_val && v.as.obj_val->count > 0;
    if (v.type == VAL_ARRAY) return v.as.arr_val && v.as.arr_val->count > 0;
    return 1;
}

Value invokeFunction(Value funcVal, int argCount, Value* args, Environment* parentEnv) {
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
            
        case AST_IMPORT_STATEMENT: {
            if (!node->value) return createNull();
            char* filename = node->value;
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
            char* source = malloc(fsize + 1);
            fread(source, 1, fsize, fp);
            source[fsize] = 0;
            fclose(fp);

            Lexer* l = Lexer_create(source);
            Parser* p = Parser_create(l);
            ASTNode* program = Parser_parseProgram(p);
            
            Value result = createNull();
            if (program) {
                result = Eval_node(program, env); // Evaluate in the current environment
                // No ASTNode_destroy since eval doesn't clean it up
            }
            
            free(source);
            // Parser/Lexer are leaked here since no proper free functions, which is fine for now
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
        
        case AST_TRY_STATEMENT: {
            jmp_stack_ptr++;
            if (setjmp(jmp_stack[jmp_stack_ptr]) == 0) {
                Value res = Eval_node(node->left, env);
                jmp_stack_ptr--;
                return res;
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
                    return createInt(strcmp(leftVal.as.str_val, rightVal.as.str_val) == 0);
                }
                throw_error("Cannot compare differing types with ==");
            }
            
            if (leftVal.type != VAL_INT || rightVal.type != VAL_INT) {
                throw_error("Math operations (+, -, *, /, <, >) require 'int' types.");
            }
            
            if (strcmp(node->value, "+") == 0) return createInt(leftVal.as.int_val + rightVal.as.int_val);
            if (strcmp(node->value, "-") == 0) return createInt(leftVal.as.int_val - rightVal.as.int_val);
            if (strcmp(node->value, "*") == 0) return createInt(leftVal.as.int_val * rightVal.as.int_val);
            if (strcmp(node->value, "/") == 0) {
                if (rightVal.as.int_val == 0) {
                    throw_error("Division by zero.");
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

                else if (val.type == VAL_OBJECT) printf("[Object keys=%d]\n", val.as.obj_val->count);
                fflush(stdout);
                return val;
            }
            throw_error("unknown function '%s'", node->left->value);
        }
        
        case AST_FUNCTION_CALL: {
            int found = 0;
            Value funcVal = Environment_get(env, node->value, &found);
            if (!found || (funcVal.type != VAL_FUNCTION && funcVal.type != VAL_NATIVE_FUNCTION)) {
                throw_error("Undefined function '%s'", node->value);
            }
            
            Value* args = malloc(sizeof(Value) * node->parameterCount);
            for (int i = 0; i < node->parameterCount; i++) {
                args[i] = Eval_node(node->parameters[i], env);
            }
            Value res = invokeFunction(funcVal, node->parameterCount, args, env);
            for (int i = 0; i < node->parameterCount; i++) {
                Value_free(args[i]);
            }
            free(args);
            return res;
        }
    }
    return createNull();
}
