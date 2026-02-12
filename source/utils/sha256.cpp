#include "sha256.h"
#include <mbedtls/sha256.h>
#include <cstdio>
#include <vector>
#include <iomanip>
#include <sstream>
#include "../app/gui.h"

#include "../app/common.h"

std::string calculateSHA256(const std::string& path) {
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        WHBLogFreetypePrintf(L"Failed to open %S", toWstring(path).c_str());
        WHBLogFreetypeDrawScreen();
        return "";
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 for SHA-256

    unsigned char buffer[4096];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        mbedtls_sha256_update(&ctx, buffer, bytesRead);
    }

    unsigned char hash[32];
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    fclose(file);

    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return ss.str();
}
