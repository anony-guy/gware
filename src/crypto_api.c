#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include "eval.h"
#include "crypto_api.h"

Value native_crypto_sha1(int argCount, Value* args) {
    if (argCount != 1 || args[0].type != VAL_STRING) return createNull();
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE rgbHash[20];
    DWORD cbHash = 20;

    CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash);
    
    const char* data = args[0].as.str_val;
    CryptHashData(hHash, (BYTE*)data, strlen(data), 0);
    CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0);

    char hex[41];
    for (DWORD i = 0; i < cbHash; i++) {
        sprintf(&hex[i*2], "%02x", rgbHash[i]);
    }
    hex[40] = '\0';

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return createString(hex);
}

Value native_crypto_base64_encode(int argCount, Value* args) {
    if (argCount != 1 || args[0].type != VAL_STRING) return createNull();
    const char* data = args[0].as.str_val;
    DWORD outLen = 0;
    CryptBinaryToStringA((BYTE*)data, strlen(data), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &outLen);
    char* b64 = malloc(outLen);
    CryptBinaryToStringA((BYTE*)data, strlen(data), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, b64, &outLen);
    Value v = createString(b64);
    free(b64);
    return v;
}

Value native_crypto_base64_decode(int argCount, Value* args) {
    if (argCount != 1 || args[0].type != VAL_STRING) return createNull();
    const char* b64 = args[0].as.str_val;
    DWORD outBinLen = 0;
    CryptStringToBinaryA(b64, 0, CRYPT_STRING_BASE64, NULL, &outBinLen, NULL, NULL);
    BYTE* bin = malloc(outBinLen + 1);
    CryptStringToBinaryA(b64, 0, CRYPT_STRING_BASE64, bin, &outBinLen, NULL, NULL);
    bin[outBinLen] = '\0';
    Value v = createString((char*)bin);
    free(bin);
    return v;
}

void register_crypto_api(Environment* env) {
    Value cryptoObj = createObject(3);
    Object_set_value(cryptoObj.as.obj_val, "sha1", createNativeFunction(native_crypto_sha1));
    Object_set_value(cryptoObj.as.obj_val, "base64_encode", createNativeFunction(native_crypto_base64_encode));
    Object_set_value(cryptoObj.as.obj_val, "base64_decode", createNativeFunction(native_crypto_base64_decode));
    Environment_set(env, "crypto", cryptoObj, NULL);
}
