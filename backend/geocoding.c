#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "geocoding.h"
#include "database.h"
#include "http_client.h"
#include "cJSON.h"

static int parse_photon_response(const char *response, const char *place_name, double *out_lat, double *out_lon, char *out_display_name, size_t max_len) {
    if (!response) return 0;
    cJSON *json = cJSON_Parse(response);
    if (!json) return 0;

    cJSON *features = cJSON_GetObjectItemCaseSensitive(json, "features");
    if (!features || !cJSON_IsArray(features) || cJSON_GetArraySize(features) == 0) {
        cJSON_Delete(json);
        return 0;
    }

    cJSON *feat = cJSON_GetArrayItem(features, 0);
    cJSON *geom = cJSON_GetObjectItemCaseSensitive(feat, "geometry");
    cJSON *coords = geom ? cJSON_GetObjectItemCaseSensitive(geom, "coordinates") : NULL;
    if (!coords || !cJSON_IsArray(coords) || cJSON_GetArraySize(coords) < 2) {
        cJSON_Delete(json);
        return 0;
    }

    /* Photon returns coordinates as [lon, lat] */
    cJSON *lon_item = cJSON_GetArrayItem(coords, 0);
    cJSON *lat_item = cJSON_GetArrayItem(coords, 1);
    if (!lon_item || !lat_item || !cJSON_IsNumber(lon_item) || !cJSON_IsNumber(lat_item)) {
        cJSON_Delete(json);
        return 0;
    }

    double lon = lon_item->valuedouble;
    double lat = lat_item->valuedouble;

    /* Build display name from properties */
    char disp_buf[512] = {0};
    cJSON *props = cJSON_GetObjectItemCaseSensitive(feat, "properties");
    if (props && cJSON_IsObject(props)) {
        cJSON *p_name = cJSON_GetObjectItemCaseSensitive(props, "name");
        cJSON *p_city = cJSON_GetObjectItemCaseSensitive(props, "city");
        cJSON *p_state = cJSON_GetObjectItemCaseSensitive(props, "state");
        cJSON *p_country = cJSON_GetObjectItemCaseSensitive(props, "country");

        snprintf(disp_buf, sizeof(disp_buf), "%s%s%s%s%s%s%s",
                 (p_name && cJSON_IsString(p_name)) ? p_name->valuestring : place_name,
                 (p_city && cJSON_IsString(p_city)) ? ", " : "",
                 (p_city && cJSON_IsString(p_city)) ? p_city->valuestring : "",
                 (p_state && cJSON_IsString(p_state)) ? ", " : "",
                 (p_state && cJSON_IsString(p_state)) ? p_state->valuestring : "",
                 (p_country && cJSON_IsString(p_country)) ? ", " : "",
                 (p_country && cJSON_IsString(p_country)) ? p_country->valuestring : "");
    } else {
        strncpy(disp_buf, place_name, sizeof(disp_buf) - 1);
    }

    if (out_lat) *out_lat = lat;
    if (out_lon) *out_lon = lon;
    if (out_display_name && max_len > 0) {
        strncpy(out_display_name, disp_buf, max_len - 1);
        out_display_name[max_len - 1] = '\0';
    }

    cJSON_Delete(json);
    return 1;
}

int geocode_location(const char *place_name, double *out_lat, double *out_lon, char *out_display_name, size_t max_len) {
    if (!place_name || strlen(place_name) == 0) return 0;

    /* 1. Check local SQLite cache first (exact and fuzzy match) */
    if (db_get_location(place_name, out_lat, out_lon, out_display_name, max_len)) {
        printf("[GEOCODE] Cache HIT for '%s' -> (%.6f, %.6f)\n", place_name, *out_lat, *out_lon);
        return 1;
    }

    printf("[GEOCODE] Cache MISS for '%s' -> querying Photon OpenStreetMap API...\n", place_name);

    /* 2. Query Photon Komoot API (no rate limiting, no API keys, fast) */
    char *encoded_query = url_encode(place_name);
    if (!encoded_query) return 0;

    char url[1024];
    snprintf(url, sizeof(url), "https://photon.komoot.io/api/?q=%s&limit=1", encoded_query);

    const char *user_agent = "OptimalDeliveryRoutePlanner/2.0 (SRM AP DAA Project; contact: student.daa.srmap@gmail.com)";
    char *response = http_get(url, user_agent);
    
    int ok = 0;
    if (response) {
        ok = parse_photon_response(response, place_name, out_lat, out_lon, out_display_name, max_len);
        free(response);
    }

    /* 2.b If no result found and query is a short landmark, try appending regional context */
    if (!ok && !strstr(place_name, ",") && !strstr(place_name, "Andhra") && !strstr(place_name, "Vijayawada")) {
        char augmented[512];
        snprintf(augmented, sizeof(augmented), "%s Vijayawada Andhra Pradesh", place_name);
        char *enc_aug = url_encode(augmented);
        if (enc_aug) {
            snprintf(url, sizeof(url), "https://photon.komoot.io/api/?q=%s&limit=1", enc_aug);
            free(enc_aug);
            response = http_get(url, user_agent);
            if (response) {
                ok = parse_photon_response(response, place_name, out_lat, out_lon, out_display_name, max_len);
                free(response);
            }
        }
    }

    /* 3. Fallback to Nominatim OSM if Photon didn't resolve */
    if (!ok) {
        printf("[GEOCODE] Querying Nominatim OSM fallback for '%s'...\n", place_name);
        snprintf(url, sizeof(url), "https://nominatim.openstreetmap.org/search?q=%s&format=json&limit=1", encoded_query);
        response = http_get(url, user_agent);
        if (response) {
            cJSON *nom_json = cJSON_Parse(response);
            free(response);
            if (nom_json && cJSON_IsArray(nom_json) && cJSON_GetArraySize(nom_json) > 0) {
                cJSON *first_item = cJSON_GetArrayItem(nom_json, 0);
                cJSON *lat_item = cJSON_GetObjectItemCaseSensitive(first_item, "lat");
                cJSON *lon_item = cJSON_GetObjectItemCaseSensitive(first_item, "lon");
                cJSON *disp_item = cJSON_GetObjectItemCaseSensitive(first_item, "display_name");
                if (lat_item && lon_item && cJSON_IsString(lat_item) && cJSON_IsString(lon_item)) {
                    double lat = strtod(lat_item->valuestring, NULL);
                    double lon = strtod(lon_item->valuestring, NULL);
                    const char *disp = (disp_item && cJSON_IsString(disp_item)) ? disp_item->valuestring : place_name;

                    if (out_lat) *out_lat = lat;
                    if (out_lon) *out_lon = lon;
                    if (out_display_name && max_len > 0) {
                        strncpy(out_display_name, disp, max_len - 1);
                        out_display_name[max_len - 1] = '\0';
                    }
                    ok = 1;
                }
            }
            if (nom_json) cJSON_Delete(nom_json);
        }
    }

    free(encoded_query);

    /* 4. Cache in SQLite on success */
    if (ok) {
        db_save_location(place_name, *out_lat, *out_lon, (out_display_name && strlen(out_display_name) > 0) ? out_display_name : place_name);
        printf("[GEOCODE] Successfully resolved '%s' -> (%.6f, %.6f) and cached in SQLite.\n", place_name, *out_lat, *out_lon);
        return 1;
    }

    fprintf(stderr, "[GEOCODE ERROR] Location '%s' could not be resolved by any provider.\n", place_name);
    return 0;
}
