#ifndef JSON_API_H
#define JSON_API_H

#include "eval.h"

Value native_json_parse(int argCount, Value* args);
Value native_json_stringify(int argCount, Value* args);

#endif // JSON_API_H
