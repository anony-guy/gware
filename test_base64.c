#include <stdio.h>
#include <windows.h>
#include <wincrypt.h>

int main() {
    const char* data = "hello world";
    DWORD outLen = 0;
    CryptBinaryToStringA((BYTE*)data, strlen(data), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &outLen);
    char* b64 = malloc(outLen);
    CryptBinaryToStringA((BYTE*)data, strlen(data), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, b64, &outLen);
    printf("B64: %s\n", b64);
    
    DWORD outBinLen = 0;
    CryptStringToBinaryA(b64, 0, CRYPT_STRING_BASE64, NULL, &outBinLen, NULL, NULL);
    BYTE* bin = malloc(outBinLen);
    CryptStringToBinaryA(b64, 0, CRYPT_STRING_BASE64, bin, &outBinLen, NULL, NULL);
    printf("Bin: %.*s\n", outBinLen, bin);
    return 0;
}
