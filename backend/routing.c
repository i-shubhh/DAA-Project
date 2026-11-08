#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "routing.h"
#include "database.h"
#include "http_client.h"
#include "cJSON.h"

int calculate_road_distance_matrix(int num_locs, const double lats[], const double lons[],
                                   const char names[][MAX_NAME_LEN],
                                   double out_matrix[MAX_LOCATIONS][MAX_LOCATIONS]) {
    if (num_locs < 2 || num_locs > MAX_LOCATIONS) return 0;

    /* Initialize matrix */
    for (int i = 0; i < MAX_LOCATIONS; i++) {
        for (int j = 0; j < MAX_LOCATIONS; j++) {
            out_matrix[i][j] = INF;
        }
    }

    /* Check if all pairs are already cached in SQLite */
    int all_cached = 1;
    for (int i = 0; i < num_locs; i++) {
        for (int j = 0; j < num_locs; j++) {
            if (i == j) {
                out_matrix[i][j] = INF;
                continue;
            }
            double cached_dist = 0.0;
            if (db_get_cached_distance(names[i], names[j], &cached_dist)) {
                out_matrix[i][j] = cached_dist;
            } else {
                all_cached = 0;
            }
        }
    }

    if (all_cached) {
        printf("[ROUTING] All pairwise road distances retrieved from SQLite cache.\n");
        return 1;
    }

    printf("[ROUTING] Querying OSRM Table API for %d delivery locations...\n", num_locs);

    /* Build OSRM Table API URL: coordinates must be lon,lat separated by semicolons */
    char url[4096];
    int offset = snprintf(url, sizeof(url), "https://router.project-osrm.org/table/v1/driving/");

    for (int i = 0; i < num_locs; i++) {
        int written = snprintf(url + offset, sizeof(url) - offset, "%.6f,%.6f%s",
                               lons[i], lats[i], (i == num_locs - 1) ? "" : ";");
        if (written < 0 || (size_t)written >= sizeof(url) - offset) {
            fprintf(stderr, "[ROUTING ERROR] URL buffer overflow\n");
            return 0;
        }
        offset += written;
    }

    snprintf(url + offset, sizeof(url) - offset, "?annotations=distance");

    const char *user_agent = "OptimalDeliveryRoutePlanner/1.0 (academic DAA student project)";
    char *response = http_get(url, user_agent);
    if (!response) {
        fprintf(stderr, "[ROUTING ERROR] Failed to query OSRM routing service.\n");
        return 0;
    }

    cJSON *json = cJSON_Parse(response);
    free(response);

    if (!json) {
        fprintf(stderr, "[ROUTING ERROR] Failed to parse OSRM response JSON.\n");
        return 0;
    }

    cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
    if (!code || !cJSON_IsString(code) || strcmp(code->valuestring, "Ok") != 0) {
        fprintf(stderr, "[ROUTING ERROR] OSRM returned error code: %s\n", code && cJSON_IsString(code) ? code->valuestring : "unknown");
        cJSON_Delete(json);
        return 0;
    }

    cJSON *distances = cJSON_GetObjectItemCaseSensitive(json, "distances");
    if (!distances || !cJSON_IsArray(distances) || cJSON_GetArraySize(distances) != num_locs) {
        fprintf(stderr, "[ROUTING ERROR] OSRM distances matrix missing or size mismatch.\n");
        cJSON_Delete(json);
        return 0;
    }

    for (int i = 0; i < num_locs; i++) {
        cJSON *row = cJSON_GetArrayItem(distances, i);
        if (!row || !cJSON_IsArray(row)) continue;

        for (int j = 0; j < num_locs; j++) {
            if (i == j) {
                out_matrix[i][j] = INF;
                continue;
            }

            cJSON *val = cJSON_GetArrayItem(row, j);
            if (val && cJSON_IsNumber(val)) {
                double meters = val->valuedouble;
                double km = meters / 1000.0;
                out_matrix[i][j] = km;
                db_save_cached_distance(names[i], names[j], km);
            } else {
                out_matrix[i][j] = INF; /* Unreachable road connection */
            }
        }
    }

    cJSON_Delete(json);
    printf("[ROUTING] Successfully built %dx%d road distance matrix via OSRM.\n", num_locs, num_locs);
    return 1;
}

char* fetch_route_geometry_geojson(int path_len, const int *path, const double lats[], const double lons[]) {
    if (path_len < 2 || !path || !lats || !lons) return NULL;

    /* Build OSRM Route URL with the exact ordered tour sequence */
    char url[4096];
    int offset = snprintf(url, sizeof(url), "https://router.project-osrm.org/route/v1/driving/");

    for (int i = 0; i < path_len; i++) {
        int idx = path[i];
        int written = snprintf(url + offset, sizeof(url) - offset, "%.6f,%.6f%s",
                               lons[idx], lats[idx], (i == path_len - 1) ? "" : ";");
        if (written < 0 || (size_t)written >= sizeof(url) - offset) {
            fprintf(stderr, "[ROUTING ERROR] Route URL buffer overflow\n");
            return NULL;
        }
        offset += written;
    }

    snprintf(url + offset, sizeof(url) - offset, "?overview=full&geometries=geojson");

    const char *user_agent = "OptimalDeliveryRoutePlanner/1.0 (academic DAA student project)";
    char *response = http_get(url, user_agent);
    if (!response) {
        fprintf(stderr, "[ROUTING ERROR] Failed to query OSRM route geometry.\n");
        return NULL;
    }

    cJSON *json = cJSON_Parse(response);
    free(response);

    if (!json) return NULL;

    cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
    if (!code || !cJSON_IsString(code) || strcmp(code->valuestring, "Ok") != 0) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *routes = cJSON_GetObjectItemCaseSensitive(json, "routes");
    if (!routes || !cJSON_IsArray(routes) || cJSON_GetArraySize(routes) == 0) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *first_route = cJSON_GetArrayItem(routes, 0);
    cJSON *geometry = cJSON_GetObjectItemCaseSensitive(first_route, "geometry");
    if (!geometry) {
        cJSON_Delete(json);
        return NULL;
    }

    char *geom_json = cJSON_PrintUnformatted(geometry);
    cJSON_Delete(json);

    return geom_json;
}

