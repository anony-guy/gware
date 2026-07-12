#ifndef CRYPTO_API_H
#define CRYPTO_API_H

#include "eval.h"

void register_crypto_api(Environment* env);
Value createNativeFunction(NativeFn fn);
void Object_set_value(struct ValueObject* obj, const char* key, Value val);

#endif // CRYPTO_API_H
