#include "sha1.h"
#include <mbedtls/sha1.h>
#include <cstdio>
#include "../app/filesystem.h"

std::vector<unsigned char> calculateSHA1(const std::string& path) {
    FILE* file = fileFopen(path.c_str(), "rb");
    if (!file) {
        return {};
    }

    mbedtls_sha1_context ctx;
    mbedtls_sha1_init(&ctx);
    mbedtls_sha1_starts(&ctx);

    unsigned char buffer[4096];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        mbedtls_sha1_update(&ctx, buffer, bytesRead);
    }

    unsigned char hash[20];
    mbedtls_sha1_finish(&ctx, hash);
    mbedtls_sha1_free(&ctx);
    fclose(file);

    return std::vector<unsigned char>(hash, hash + 20);
}
