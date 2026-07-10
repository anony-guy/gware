import sys
import re

def update_eval(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # 1. createObject
    if 'static Value createObject' not in content:
        create_obj_func = """
static Value createObject(int capacity) {
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
"""
        content = content.replace('static Value createArray', create_obj_func + '\nstatic Value createArray')

    # 2. Value_free
    if 'v.type == VAL_OBJECT' not in content.split('void Value_free')[1]:
        free_obj = """
    if (v.type == VAL_OBJECT && v.as.obj_val) {
        for (int i = 0; i < v.as.obj_val->count; i++) {
            free(v.as.obj_val->keys[i]);
            Value_free(v.as.obj_val->values[i]);
        }
        free(v.as.obj_val->keys);
        free(v.as.obj_val->values);
        free(v.as.obj_val);
    }
"""
        content = content.replace('if (v.type == VAL_ARRAY', free_obj + '    if (v.type == VAL_ARRAY')

    # 3. Value_copy
    if 'v.type == VAL_OBJECT' not in content.split('Value Value_copy')[1]:
        copy_obj = """
    } else if (v.type == VAL_OBJECT && v.as.obj_val) {
        copy.as.obj_val = (ValueObject*)malloc(sizeof(ValueObject));
        copy.as.obj_val->count = v.as.obj_val->count;
        copy.as.obj_val->capacity = v.as.obj_val->capacity;
        copy.as.obj_val->keys = (char**)malloc(sizeof(char*) * copy.as.obj_val->capacity);
        copy.as.obj_val->values = (Value*)malloc(sizeof(Value) * copy.as.obj_val->capacity);
        for (int i = 0; i < copy.as.obj_val->count; i++) {
            copy.as.obj_val->keys[i] = strdup(v.as.obj_val->keys[i]);
            copy.as.obj_val->values[i] = Value_copy(v.as.obj_val->values[i]);
        }
"""
        content = content.replace('} else if (v.type == VAL_ARRAY', copy_obj + '    } else if (v.type == VAL_ARRAY')

    # 4. AST_SET_STATEMENT
    set_obj = """
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
"""
    content = re.sub(
        r'if \(!ref \|\| ref->type != VAL_ARRAY\).*?return newVal;',
        set_obj.strip(),
        content,
        flags=re.DOTALL
    )

    # 5. AST_INDEX_EXPRESSION
    idx_obj = """
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
"""
    content = re.sub(
        r'Value arrVal = Eval_node\(node->left, env\);.*?if \(idxVal.type != VAL_INT\) \{\s*throw_error\("Index must be an integer"\);\s*\}',
        idx_obj.strip(),
        content,
        flags=re.DOTALL
    )

    # 6. AST_CALL_EXPRESSION for show
    if 'VAL_OBJECT' not in content.split('AST_CALL_EXPRESSION')[1]:
        show_obj = """
                else if (val.type == VAL_OBJECT) printf("[Object keys=%d]\\n", val.as.obj_val->count);
"""
        content = content.replace('else if (val.type == VAL_ARRAY) printf("[Array count=%d]\\n", val.as.arr_val->count);', 'else if (val.type == VAL_ARRAY) printf("[Array count=%d]\\n", val.as.arr_val->count);\n' + show_obj)

    with open(filepath, 'w') as f:
        f.write(content)

update_eval('src/eval.c')
