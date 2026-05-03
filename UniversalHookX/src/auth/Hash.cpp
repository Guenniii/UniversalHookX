#include "Hash.h"
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")

std::string Sha256(const std::string& input)
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[32];
    DWORD hashLen = 32;

    CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash);
    CryptHashData(hHash, (BYTE*)input.c_str(), input.length(), 0);
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    static const char* hex = "0123456789abcdef";
    std::string result;
    for (int i = 0; i < 32; i++)
    {
        result += hex[(hash[i] >> 4) & 0xF];
        result += hex[hash[i] & 0xF];
    }
    return result;
}
