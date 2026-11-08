#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
typedef int socket_error_t;
#else
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define _stricmp strcasecmp
#endif

#include "database.h"
#include "geocoding.h"
#include "routing.h"
#include "http_client.h"
#include "cJSON.h"
#include "../Common/route.h"
#include "../Common/nearest_neighbor.h"
#include "../Common/branch_bound.h"

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 65536

static const char* get_mime_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (_stricmp(dot, ".html") == 0 || _stricmp(dot, ".htm") == 0) return "text/html; charset=utf-8";
    if (_stricmp(dot, ".css") == 0) return "text/css; charset=utf-8";
    if (_stricmp(dot, ".js") == 0) return "application/javascript; charset=utf-8";
    if (_stricmp(dot, ".json") == 0) return "application/json; charset=utf-8";
    if (_stricmp(dot, ".png") == 0) return "image/png";
    if (_stricmp(dot, ".jpg") == 0 || _stricmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (_stricmp(dot, ".svg") == 0) return "image/svg+xml";
    if (_stricmp(dot, ".ico") == 0) return "image/x-icon";
    if (_stricmp(dot, ".pdf") == 0) return "application/pdf";
    return "application/octet-stream";
}

static void send_response(SOCKET sock, int status_code, const char *status_text,
                          const char *content_type, const char *body, size_t body_len) {
    char header[1024];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, content_type, (unsigned long)body_len);

    send(sock, header, header_len, 0);
    if (body && body_len > 0) {
        send(sock, body, (int)body_len, 0);
    }
}

static void send_json_response(SOCKET sock, int status_code, const char *json_str) {
    size_t len = json_str ? strlen(json_str) : 0;
    send_response(sock, status_code, (status_code == 200) ? "OK" : "Error", "application/json", json_str, len);
}

static void send_file_response(SOCKET sock, const char *file_path) {
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        const char *not_found = "{\"error\": \"File not found\"}";
        send_response(sock, 404, "Not Found", "application/json", not_found, strlen(not_found));
        return;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = (char*)malloc((size_t)file_size);
    if (!buf) {
        fclose(fp);
        const char *err = "{\"error\": \"Server memory allocation failed\"}";
        send_response(sock, 500, "Internal Server Error", "application/json", err, strlen(err));
        return;
    }

    size_t read_bytes = fread(buf, 1, (size_t)file_size, fp);
    fclose(fp);

    const char *mime = get_mime_type(file_path);
    send_response(sock, 200, "OK", mime, buf, read_bytes);
    free(buf);
}

/* API Handlers */

static void handle_api_geocode(SOCKET sock, const char *body) {
    cJSON *req = cJSON_Parse(body);
    if (!req) {
        send_json_response(sock, 400, "{\"success\":false,\"error\":\"Invalid JSON payload\"}");
        return;
    }

    cJSON *addr = cJSON_GetObjectItemCaseSensitive(req, "address");
    if (!addr || !cJSON_IsString(addr) || strlen(addr->valuestring) == 0) {
        cJSON_Delete(req);
        send_json_response(sock, 400, "{\"success\":false,\"error\":\"Missing 'address' parameter\"}");
        return;
    }

    double lat = 0.0, lon = 0.0;
    char display_name[512] = {0};

    int success = geocode_location(addr->valuestring, &lat, &lon, display_name, sizeof(display_name));
    cJSON_Delete(req);

    if (success) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "success", 1);
        cJSON_AddStringToObject(res, "address", addr->valuestring);
        cJSON_AddStringToObject(res, "display_name", display_name);
        cJSON_AddNumberToObject(res, "lat", lat);
        cJSON_AddNumberToObject(res, "lon", lon);

        char *res_str = cJSON_PrintUnformatted(res);
        send_json_response(sock, 200, res_str);
        free(res_str);
        cJSON_Delete(res);
    } else {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "success", 0);
        cJSON_AddStringToObject(res, "error", "Location could not be found. Please check spelling, add city/state, or choose a nearby landmark.");
        char *res_str = cJSON_PrintUnformatted(res);
        send_json_response(sock, 404, res_str);
        free(res_str);
        cJSON_Delete(res);
    }
}

static void handle_api_matrix(SOCKET sock, const char *body) {
    cJSON *req = cJSON_Parse(body);
    if (!req) {
        send_json_response(sock, 400, "{\"success\":false,\"error\":\"Invalid JSON request\"}");
        return;
    }

    cJSON *locs = cJSON_GetObjectItemCaseSensitive(req, "locations");
    if (!locs || !cJSON_IsArray(locs)) {
        cJSON_Delete(req);
        send_json_response(sock, 400, "{\"success\":false,\"error\":\"'locations' array is required\"}");
        return;
    }

    int n = cJSON_GetArraySize(locs);
    if (n < 2 || n > MAX_LOCATIONS) {
        cJSON_Delete(req);
        send_json_response(sock, 400, "{\"success\":false,\"error\":\"Location count must be between 2 and 20\"}");
        return;
    }

    double lats[MAX_LOCATIONS];
    double lons[MAX_LOCATIONS];
    char names[MAX_LOCATIONS][MAX_NAME_LEN];

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(locs, i);
        cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON *lat = cJSON_GetObjectItemCaseSensitive(item, "lat");
        cJSON *lon = cJSON_GetObjectItemCaseSensitive(item, "lon");

        if (!name || !lat || !lon || !cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) {
            cJSON_Delete(req);
            send_json_response(sock, 400, "{\"success\":false,\"error\":\"Each location must have name, lat, and lon\"}");
            return;
        }

        strncpy(names[i], name->valuestring, MAX_NAME_LEN - 1);
        names[i][MAX_NAME_LEN - 1] = '\0';
        lats[i] = lat->valuedouble;
        lons[i] = lon->valuedouble;
    }

    cJSON_Delete(req);

    double matrix[MAX_LOCATIONS][MAX_LOCATIONS];
    int ok = calculate_road_distance_matrix(n, lats, lons, (const char (*)[MAX_NAME_LEN])names, matrix);

    if (!ok) {
        send_json_response(sock, 502, "{\"success\":false,\"error\":\"OSRM routing service failed to compute road distances. You may enter travel costs manually.\"}");
        return;
    }

    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "success", 1);
    cJSON_AddNumberToObject(res, "num_locations", n);

    cJSON *names_arr = cJSON_CreateArray();
    cJSON *coords_arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON_AddItemToArray(names_arr, cJSON_CreateString(names[i]));

        cJSON *c = cJSON_CreateObject();
        cJSON_AddNumberToObject(c, "lat", lats[i]);
        cJSON_AddNumberToObject(c, "lon", lons[i]);
        cJSON_AddItemToArray(coords_arr, c);
    }
    cJSON_AddItemToObject(res, "names", names_arr);
    cJSON_AddItemToObject(res, "coordinates", coords_arr);

    cJSON *mat_arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON *row = cJSON_CreateArray();
        for (int j = 0; j < n; j++) {
            if (matrix[i][j] >= INF / 2.0) {
                cJSON_AddItemToArray(row, cJSON_CreateNumber(-1.0)); /* -1 denotes disconnected */
            } else {
                cJSON_AddItemToArray(row, cJSON_CreateNumber(matrix[i][j]));
            }
        }
        cJSON_AddItemToArray(mat_arr, row);
    }
    cJSON_AddItemToObject(res, "matrix", mat_arr);

    char *res_str = cJSON_PrintUnformatted(res);
    send_json_response(sock, 200, res_str);
    free(res_str);
    cJSON_Delete(res);
}

static void handle_api_optimize(SOCKET sock, const char *body) {
    cJSON *req = cJSON_Parse(body);
    if (!req) {
        send_json_response(sock, 400, "{\"success\":false,\"error\":\"Invalid JSON request\"}");
        return;
    }

    cJSON *locs = cJSON_GetObjectItemCaseSensitive(req, "locations");
    cJSON *mat = cJSON_GetObjectItemCaseSensitive(req, "matrix");
    cJSON *start_idx_item = cJSON_GetObjectItemCaseSensitive(req, "startIndex");

    if (!locs || !cJSON_IsArray(locs) || !mat || !cJSON_IsArray(mat)) {
        cJSON_Delete(req);
        send_json_response(sock, 400, "{\"success\":false,\"error\":\"'locations' and 'matrix' arrays are required\"}");
        return;
    }

    int n = cJSON_GetArraySize(locs);
    if (n < 2 || n > MAX_LOCATIONS || cJSON_GetArraySize(mat) != n) {
        cJSON_Delete(req);
        send_json_response(sock, 400, "{\"success\":false,\"error\":\"Location count and matrix dimensions mismatch or out of range [2..20]\"}");
        return;
    }

    int start_index = (start_idx_item && cJSON_IsNumber(start_idx_item)) ? start_idx_item->valueint : 0;
    if (start_index < 0 || start_index >= n) start_index = 0;

    DeliveryNetwork net;
    init_network(&net, n);

    double lats[MAX_LOCATIONS] = {0};
    double lons[MAX_LOCATIONS] = {0};

    for (int i = 0; i < n; i++) {
        cJSON *loc_item = cJSON_GetArrayItem(locs, i);
        cJSON *name = cJSON_GetObjectItemCaseSensitive(loc_item, "name");
        cJSON *lat = cJSON_GetObjectItemCaseSensitive(loc_item, "lat");
        cJSON *lon = cJSON_GetObjectItemCaseSensitive(loc_item, "lon");

        const char *loc_name = (name && cJSON_IsString(name)) ? name->valuestring : "Location";
        set_location_name(&net, i, loc_name);

        if (lat && cJSON_IsNumber(lat)) lats[i] = lat->valuedouble;
        if (lon && cJSON_IsNumber(lon)) lons[i] = lon->valuedouble;

        cJSON *row = cJSON_GetArrayItem(mat, i);
        if (!row || !cJSON_IsArray(row)) continue;

        for (int j = 0; j < n; j++) {
            cJSON *val = cJSON_GetArrayItem(row, j);
            if (i == j) {
                set_cost(&net, i, j, INF);
            } else if (val && cJSON_IsNumber(val)) {
                double cost = val->valuedouble;
                if (cost < 0.0) {
                    set_cost(&net, i, j, INF); /* -1 denotes disconnected */
                } else {
                    set_cost(&net, i, j, cost);
                }
            } else {
                set_cost(&net, i, j, INF);
            }
        }
    }

    char err_buf[256];
    if (!validate_network(&net, err_buf, sizeof(err_buf))) {
        cJSON *res = cJSON_CreateObject();
        cJSON_AddBoolToObject(res, "success", 0);
        cJSON_AddStringToObject(res, "error", err_buf);
        char *res_str = cJSON_PrintUnformatted(res);
        send_json_response(sock, 400, res_str);
        free(res_str);
        cJSON_Delete(res);
        cJSON_Delete(req);
        return;
    }

    /* Execute DAA Algorithms */
    printf("[OPTIMIZE] Running Branch and Bound (LCBB with matrix reduction)...\n");
    RouteResult bb_res = solve_tsp_branch_bound(&net, start_index);

    printf("[OPTIMIZE] Running Nearest Neighbor (Greedy heuristic)...\n");
    RouteResult nn_res = solve_tsp_nearest_neighbor(&net, start_index);

    /* Format route strings */
    char bb_route_str[1024];
    char nn_route_str[1024];
    format_route_string(&net, &bb_res, bb_route_str, sizeof(bb_route_str));
    format_route_string(&net, &nn_res, nn_route_str, sizeof(nn_route_str));

    /* Save to SQLite Database */
    int plan_id = 0;
    char *locs_json_str = cJSON_PrintUnformatted(locs);
    char *mat_json_str = cJSON_PrintUnformatted(mat);
    db_save_delivery_plan(net.names[start_index], n, locs_json_str, mat_json_str, &plan_id);
    free(locs_json_str);
    free(mat_json_str);

    if (plan_id > 0) {
        if (bb_res.is_valid) {
            db_save_route_result(plan_id, "Branch and Bound", bb_route_str, bb_res.total_cost, bb_res.execution_time_ms, bb_res.nodes_explored);
        }
        if (nn_res.is_valid) {
            db_save_route_result(plan_id, "Nearest Neighbor", nn_route_str, nn_res.total_cost, nn_res.execution_time_ms, nn_res.nodes_explored);
        }
    }

    /* Fetch real road-route geometries from OSRM */
    char *bb_geometry_json = NULL;
    char *nn_geometry_json = NULL;

    if (bb_res.is_valid && lats[0] != 0.0) {
        bb_geometry_json = fetch_route_geometry_geojson(n + 1, bb_res.path, lats, lons);
    }
    if (nn_res.is_valid && lats[0] != 0.0) {
        nn_geometry_json = fetch_route_geometry_geojson(n + 1, nn_res.path, lats, lons);
    }

    /* Construct Response JSON */
    cJSON *res = cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "success", 1);
    cJSON_AddNumberToObject(res, "plan_id", plan_id);
    cJSON_AddNumberToObject(res, "num_locations", n);
    cJSON_AddNumberToObject(res, "start_index", start_index);
    cJSON_AddStringToObject(res, "start_location", net.names[start_index]);

    /* Branch and Bound block */
    cJSON *bb_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(bb_obj, "is_valid", bb_res.is_valid);
    cJSON_AddStringToObject(bb_obj, "message", bb_res.message);
    cJSON_AddNumberToObject(bb_obj, "total_cost", bb_res.total_cost);
    cJSON_AddNumberToObject(bb_obj, "execution_time_ms", bb_res.execution_time_ms);
    cJSON_AddNumberToObject(bb_obj, "nodes_explored", bb_res.nodes_explored);
    cJSON_AddStringToObject(bb_obj, "route_string", bb_route_str);

    cJSON *bb_path_arr = cJSON_CreateArray();
    cJSON *bb_names_arr = cJSON_CreateArray();
    if (bb_res.is_valid) {
        for (int i = 0; i <= n; i++) {
            cJSON_AddItemToArray(bb_path_arr, cJSON_CreateNumber(bb_res.path[i]));
            cJSON_AddItemToArray(bb_names_arr, cJSON_CreateString(net.names[bb_res.path[i]]));
        }
    }
    cJSON_AddItemToObject(bb_obj, "path", bb_path_arr);
    cJSON_AddItemToObject(bb_obj, "path_names", bb_names_arr);
    if (bb_geometry_json) {
        cJSON *geom = cJSON_Parse(bb_geometry_json);
        if (geom) cJSON_AddItemToObject(bb_obj, "geometry", geom);
        free(bb_geometry_json);
    }
    cJSON_AddItemToObject(res, "branch_and_bound", bb_obj);

    /* Nearest Neighbor block */
    cJSON *nn_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(nn_obj, "is_valid", nn_res.is_valid);
    cJSON_AddStringToObject(nn_obj, "message", nn_res.message);
    cJSON_AddNumberToObject(nn_obj, "total_cost", nn_res.total_cost);
    cJSON_AddNumberToObject(nn_obj, "execution_time_ms", nn_res.execution_time_ms);
    cJSON_AddNumberToObject(nn_obj, "nodes_explored", nn_res.nodes_explored);
    cJSON_AddStringToObject(nn_obj, "route_string", nn_route_str);

    cJSON *nn_path_arr = cJSON_CreateArray();
    cJSON *nn_names_arr = cJSON_CreateArray();
    if (nn_res.is_valid) {
        for (int i = 0; i <= n; i++) {
            cJSON_AddItemToArray(nn_path_arr, cJSON_CreateNumber(nn_res.path[i]));
            cJSON_AddItemToArray(nn_names_arr, cJSON_CreateString(net.names[nn_res.path[i]]));
        }
    }
    cJSON_AddItemToObject(nn_obj, "path", nn_path_arr);
    cJSON_AddItemToObject(nn_obj, "path_names", nn_names_arr);
    if (nn_geometry_json) {
        cJSON *geom = cJSON_Parse(nn_geometry_json);
        if (geom) cJSON_AddItemToObject(nn_obj, "geometry", geom);
        free(nn_geometry_json);
    }
    cJSON_AddItemToObject(res, "nearest_neighbor", nn_obj);

    /* Comparison block */
    cJSON *cmp = cJSON_CreateObject();
    cJSON_AddBoolToObject(cmp, "bb_valid", bb_res.is_valid);
    cJSON_AddBoolToObject(cmp, "nn_valid", nn_res.is_valid);
    if (bb_res.is_valid && nn_res.is_valid) {
        double diff_km = nn_res.total_cost - bb_res.total_cost;
        double pct_diff = (bb_res.total_cost > 0.0) ? (diff_km / bb_res.total_cost) * 100.0 : 0.0;
        cJSON_AddNumberToObject(cmp, "difference_km", diff_km);
        cJSON_AddNumberToObject(cmp, "cost_difference", diff_km);
        cJSON_AddNumberToObject(cmp, "percent_difference", pct_diff);
        cJSON_AddNumberToObject(cmp, "optimality_gap_percent", pct_diff);
        if (diff_km > 0.001) {
            cJSON_AddStringToObject(cmp, "winner", "Branch and Bound produced a shorter route by optimizing the global permutation.");
        } else if (diff_km < -0.001) {
            cJSON_AddStringToObject(cmp, "winner", "Nearest Neighbor produced equal or shorter route (unusual for non-metric graphs).");
        } else {
            cJSON_AddStringToObject(cmp, "winner", "Both algorithms achieved identical travel distance on this network topology.");
        }
    } else {
        cJSON_AddStringToObject(cmp, "winner", "Direct comparison unavailable: one or both algorithms failed to form a complete Hamiltonian cycle.");
    }
    cJSON_AddItemToObject(res, "comparison", cmp);

    char *res_str = cJSON_PrintUnformatted(res);
    send_json_response(sock, 200, res_str);
    free(res_str);
    cJSON_Delete(res);
    cJSON_Delete(req);
}

static void handle_api_history(SOCKET sock) {
    char *history_json = db_get_history_json();
    send_json_response(sock, 200, history_json);
    free(history_json);
}

static void handle_api_history_item(SOCKET sock, int plan_id) {
    char *plan_json = db_get_plan_details_json(plan_id);
    if (plan_json) {
        send_json_response(sock, 200, plan_json);
        free(plan_json);
    } else {
        send_json_response(sock, 404, "{\"error\":\"Plan not found\"}");
    }
}

static void handle_api_delete_history(SOCKET sock, int plan_id) {
    if (db_delete_plan(plan_id)) {
        send_json_response(sock, 200, "{\"success\":true,\"message\":\"Plan deleted\"}");
    } else {
        send_json_response(sock, 500, "{\"success\":false,\"error\":\"Failed to delete plan\"}");
    }
}

static void handle_api_samples(SOCKET sock) {
    cJSON *arr = cJSON_CreateArray();

    for (int s = 1; s <= 4; s++) {
        DeliveryNetwork net;
        load_sample_network(&net, s);

        cJSON *sample = cJSON_CreateObject();
        cJSON_AddNumberToObject(sample, "id", s);
        cJSON_AddStringToObject(sample, "name", get_sample_name(s));
        cJSON_AddNumberToObject(sample, "num_locations", net.num_locations);

        cJSON *names = cJSON_CreateArray();
        for (int i = 0; i < net.num_locations; i++) {
            cJSON_AddItemToArray(names, cJSON_CreateString(net.names[i]));
        }
        cJSON_AddItemToObject(sample, "names", names);

        cJSON *mat = cJSON_CreateArray();
        for (int i = 0; i < net.num_locations; i++) {
            cJSON *row = cJSON_CreateArray();
            for (int j = 0; j < net.num_locations; j++) {
                double c = net.cost_matrix[i][j];
                cJSON_AddItemToArray(row, cJSON_CreateNumber((c >= INF / 2.0) ? -1.0 : c));
            }
            cJSON_AddItemToArray(mat, row);
        }
        cJSON_AddItemToObject(sample, "matrix", mat);

        cJSON_AddItemToArray(arr, sample);
    }

    char *json_str = cJSON_PrintUnformatted(arr);
    send_json_response(sock, 200, json_str);
    free(json_str);
    cJSON_Delete(arr);
}

/* Connection Handler Thread */
#ifdef _WIN32
static unsigned __stdcall client_worker(void *arg) {
    SOCKET client_sock = (SOCKET)arg;
#else
static void *client_worker(void *arg) {
    SOCKET client_sock = *(SOCKET *)arg;
    free(arg);
#endif

    char buffer[BUFFER_SIZE];
    int total_bytes = 0;
    int received = 0;

    /* Read HTTP headers */
    while ((received = recv(client_sock, buffer + total_bytes, sizeof(buffer) - total_bytes - 1, 0)) > 0) {
        total_bytes += received;
        buffer[total_bytes] = '\0';
        if (strstr(buffer, "\r\n\r\n")) break;
        if (total_bytes >= (int)sizeof(buffer) - 1) break;
    }

    if (total_bytes <= 0) {
        closesocket(client_sock);
        return 0;
    }

    buffer[total_bytes] = '\0';

    char method[16] = {0};
    char uri[512] = {0};
    char version[16] = {0};
    sscanf(buffer, "%15s %511s %15s", method, uri, version);

    /* Handle CORS preflight */
    if (strcmp(method, "OPTIONS") == 0) {
        send_response(client_sock, 200, "OK", "text/plain", "", 0);
        closesocket(client_sock);
        return 0;
    }

    /* Locate body */
    char *body = strstr(buffer, "\r\n\r\n");
    if (body) {
        body += 4;
    } else {
        body = "";
    }

    /* Check Content-Length for POST payloads */
    int content_length = 0;
    char *cl_header = strstr(buffer, "Content-Length:");
    if (!cl_header) cl_header = strstr(buffer, "content-length:");
    if (cl_header) {
        content_length = atoi(cl_header + 15);
    }

    int current_body_len = total_bytes - (int)(body - buffer);
    if (content_length > current_body_len && content_length < (int)sizeof(buffer) - (int)(body - buffer)) {
        int needed = content_length - current_body_len;
        while (needed > 0) {
            int more = recv(client_sock, buffer + total_bytes, needed, 0);
            if (more <= 0) break;
            total_bytes += more;
            needed -= more;
        }
        buffer[total_bytes] = '\0';
    }

    /* Strip query string from URI for routing */
    char clean_uri[512];
    strncpy(clean_uri, uri, sizeof(clean_uri) - 1);
    clean_uri[sizeof(clean_uri) - 1] = '\0';
    char *query = strchr(clean_uri, '?');
    if (query) *query = '\0';

    /* Route Dispatch */
    if (strcmp(clean_uri, "/api/geocode") == 0 && strcmp(method, "POST") == 0) {
        handle_api_geocode(client_sock, body);
    } else if (strcmp(clean_uri, "/api/matrix") == 0 && strcmp(method, "POST") == 0) {
        handle_api_matrix(client_sock, body);
    } else if (strcmp(clean_uri, "/api/optimize") == 0 && strcmp(method, "POST") == 0) {
        handle_api_optimize(client_sock, body);
    } else if (strcmp(clean_uri, "/api/history") == 0 && strcmp(method, "GET") == 0) {
        handle_api_history(client_sock);
    } else if (strncmp(clean_uri, "/api/history/", 13) == 0 && strcmp(method, "GET") == 0) {
        int id = atoi(clean_uri + 13);
        handle_api_history_item(client_sock, id);
    } else if (strncmp(clean_uri, "/api/history/", 13) == 0 && strcmp(method, "DELETE") == 0) {
        int id = atoi(clean_uri + 13);
        handle_api_delete_history(client_sock, id);
    } else if (strcmp(clean_uri, "/api/samples") == 0 && strcmp(method, "GET") == 0) {
        handle_api_samples(client_sock);
    } else if (strcmp(clean_uri, "/api/health") == 0 && strcmp(method, "GET") == 0) {
        send_json_response(client_sock, 200, "{\"status\":\"ok\",\"backend\":\"C99 Portable HTTP Server\"}");
    } else {
        /* Static file serving */
        char file_path[512];
        if (strcmp(clean_uri, "/") == 0 || strcmp(clean_uri, "/index.html") == 0) {
            snprintf(file_path, sizeof(file_path), "frontend/index.html");
        } else {
            /* Prevent directory traversal */
            if (strstr(clean_uri, "..")) {
                const char *forbidden = "{\"error\":\"Access denied\"}";
                send_response(client_sock, 403, "Forbidden", "application/json", forbidden, strlen(forbidden));
                closesocket(client_sock);
                return 0;
            }
            snprintf(file_path, sizeof(file_path), "frontend%s", clean_uri);
        }
        send_file_response(client_sock, file_path);
    }

    closesocket(client_sock);
    return 0;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) port = DEFAULT_PORT;
    }

    printf("===============================================================================\n");
    printf("   OPTIMAL DELIVERY ROUTE PLANNING - C HTTP BACKEND SERVER\n");
    printf("   Travelling Salesperson Problem: Branch & Bound vs Nearest Neighbor\n");
    printf("===============================================================================\n");

    /* Initialize SQLite Database */
    if (!db_init("database/delivery.db")) {
        fprintf(stderr, "[FATAL] Failed to initialize SQLite database.\n");
        return 1;
    }

    /* Initialize Winsock */
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[FATAL] WSAStartup failed. Error: %d\n", WSAGetLastError());
        db_close();
        return 1;
    }
#endif

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
#ifdef _WIN32
        fprintf(stderr, "[FATAL] Socket creation failed. Error: %d\n", WSAGetLastError());
        WSACleanup();
#else
        perror("[FATAL] Socket creation failed");
#endif
        db_close();
        return 1;
    }

    /* Allow address reuse */
    int optval = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((u_short)port);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
#ifdef _WIN32
        fprintf(stderr, "[FATAL] Bind failed on port %d. Error: %d\n", port, WSAGetLastError());
#else
        fprintf(stderr, "[FATAL] Bind failed on port %d: %s\n", port, strerror(errno));
#endif
        closesocket(listen_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        db_close();
        return 1;
    }

    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
#ifdef _WIN32
        fprintf(stderr, "[FATAL] Listen failed. Error: %d\n", WSAGetLastError());
#else
        fprintf(stderr, "[FATAL] Listen failed: %s\n", strerror(errno));
#endif
        closesocket(listen_sock);
#ifdef _WIN32
        WSACleanup();
#endif
        db_close();
        return 1;
    }

    printf("[SERVER] HTTP Backend successfully running at: http://localhost:%d\n", port);
    printf("[SERVER] Open http://localhost:%d in your web browser.\n", port);
    printf("[SERVER] Press Ctrl+C in this console to stop the server.\n");
    printf("-------------------------------------------------------------------------------\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);

        if (client_sock == INVALID_SOCKET) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEINTR) break;
#else
            if (errno == EINTR) break;
#endif
            continue;
        }

        /* Spawn worker thread for each connection */
#ifdef _WIN32
        HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, client_worker, (void*)client_sock, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
        } else {
            closesocket(client_sock);
        }
#else
        SOCKET *client_arg = malloc(sizeof(*client_arg));
        if (!client_arg) {
            closesocket(client_sock);
            continue;
        }
        *client_arg = client_sock;
        pthread_t thread;
        if (pthread_create(&thread, NULL, client_worker, client_arg) == 0) {
            pthread_detach(thread);
        } else {
            free(client_arg);
            closesocket(client_sock);
        }
#endif
    }

    closesocket(listen_sock);
#ifdef _WIN32
    WSACleanup();
#endif
    db_close();
    return 0;
}
