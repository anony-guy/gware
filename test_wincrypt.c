#include <stdio.h>
#include <windows.h>
#include <wincrypt.h>

int main() {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE rgbHash[20];
    DWORD cbHash = 20;

    CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash);
    
    const char* data = "hello world";
    CryptHashData(hHash, (BYTE*)data, strlen(data), 0);
    CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0);

    for (DWORD i = 0; i < cbHash; i++) {
        printf("%02x", rgbHash[i]);
    }
    printf("\n");

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return 0;
}
