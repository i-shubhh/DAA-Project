#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

/**
 * Performs an HTTP/HTTPS GET request to the specified URL.
 * Supports both libcurl (if compiled with -DUSE_LIBCURL -lcurl)
 * and Windows curl.exe fallback for maximum compatibility.
 * 
 * @param url The full HTTP/HTTPS URL.
 * @param user_agent Optional User-Agent header (pass NULL for default).
 * @return Dynamically allocated response string (must be freed by caller), or NULL on failure.
 */
char* http_get(const char *url, const char *user_agent);

/**
 * URL-encodes a string (e.g. "SRM University, AP" -> "SRM%20University%2C%20AP").
 * @param str The raw input string.
 * @return Dynamically allocated encoded string (must be freed by caller).
 */
char* url_encode(const char *str);

#endif /* HTTP_CLIENT_H */

