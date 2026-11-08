#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "http_client.h"

#ifdef _WIN32
/* Ensure _popen / _pclose prototypes are available */
FILE *_popen(const char *command, const char *mode);
int _pclose(FILE *stream);
#endif

#ifdef USE_LIBCURL
#include <curl/curl.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

char* http_get(const char *url, const char *user_agent) {
    if (!url) return NULL;

    CURL *curl_handle = curl_easy_init();
    if (!curl_handle) return NULL;

    struct MemoryStruct chunk;
    chunk.memory = (char*)malloc(1);
    chunk.size = 0;

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 15L);

    if (user_agent) {
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, user_agent);
    } else {
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "OptimalDeliveryRoutePlanner/1.0");
    }

    CURLcode res = curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);

    if (res != CURLE_OK) {
        if (chunk.memory) free(chunk.memory);
        return NULL;
    }

    return chunk.memory;
}

#else

/* Portable fallback using the system curl executable. */
char* http_get(const char *url, const char *user_agent) {
    if (!url) return NULL;

    const char *ua = user_agent ? user_agent : "OptimalDeliveryRoutePlanner/1.0 (academic DAA project)";

    /* Format command line with quotes and timeouts */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "%s -s --max-time 15 -A \"%s\" \"%s\"",
#ifdef _WIN32
             "curl.exe",
#else
             "curl",
#endif
             ua, url);

#ifdef _WIN32
    FILE *fp = _popen(cmd, "rb");
#else
    FILE *fp = popen(cmd, "r");
#endif
    if (!fp) {
        return NULL;
    }

    size_t capacity = 4096;
    size_t length = 0;
    char *buffer = (char*)malloc(capacity);
    if (!buffer) {
#ifdef _WIN32
        _pclose(fp);
#else
        pclose(fp);
#endif
        return NULL;
    }

    char chunk[2048];
    size_t bytes_read;
    while ((bytes_read = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        if (length + bytes_read + 1 > capacity) {
            capacity = (capacity * 2 > length + bytes_read + 1) ? capacity * 2 : length + bytes_read + 2048;
            char *new_buf = (char*)realloc(buffer, capacity);
            if (!new_buf) {
                free(buffer);
#ifdef _WIN32
                _pclose(fp);
#else
                pclose(fp);
#endif
                return NULL;
            }
            buffer = new_buf;
        }
        memcpy(buffer + length, chunk, bytes_read);
        length += bytes_read;
    }

#ifdef _WIN32
    _pclose(fp);
#else
    pclose(fp);
#endif

    buffer[length] = '\0';
    return buffer;
}
#endif

char* url_encode(const char *str) {
    if (!str) return NULL;

    size_t len = strlen(str);
    /* Worst case: every char becomes %XX (3 chars) + null */
    char *encoded = (char*)malloc(len * 3 + 1);
    if (!encoded) return NULL;

    char *p = encoded;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            *p++ = c;
        } else if (c == ' ') {
            *p++ = '%';
            *p++ = '2';
            *p++ = '0';
        } else {
            snprintf(p, 4, "%%%02X", c);
            p += 3;
        }
    }
    *p = '\0';
    return encoded;
}
