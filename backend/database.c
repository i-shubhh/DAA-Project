#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"
#include "cJSON.h"
#include "database.h"

static char* safe_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = (char*)malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len + 1);
    }
    return dup;
}

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir_compat(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#define mkdir_compat(dir) mkdir(dir, 0755)
#endif

static sqlite3 *g_db = NULL;

static void ensure_directory(const char *path) {
    char dir[512];
    strncpy(dir, path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';

    char *last_slash = strrchr(dir, '/');
    if (!last_slash) last_slash = strrchr(dir, '\\');
    if (last_slash) {
        *last_slash = '\0';
        mkdir_compat(dir);
    }
}

int db_init(const char *db_path) {
    if (!db_path) db_path = "database/delivery.db";
    ensure_directory(db_path);

    int rc = sqlite3_open(db_path, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] Failed to open database '%s': %s\n", db_path, sqlite3_errmsg(g_db));
        if (g_db) {
            sqlite3_close(g_db);
            g_db = NULL;
        }
        return 0;
    }

    /* Enable WAL mode and foreign keys */
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL);

    /* 1. Locations table */
    const char *sql_locations =
        "CREATE TABLE IF NOT EXISTS Locations ("
        "  location_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  location_name TEXT UNIQUE COLLATE NOCASE,"
        "  latitude REAL NOT NULL,"
        "  longitude REAL NOT NULL,"
        "  display_name TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    /* 2. Delivery_Plans table */
    const char *sql_plans =
        "CREATE TABLE IF NOT EXISTS Delivery_Plans ("
        "  plan_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  source_location TEXT NOT NULL,"
        "  total_locations INTEGER NOT NULL,"
        "  locations_json TEXT,"
        "  matrix_json TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    /* 3. Route_Results table */
    const char *sql_results =
        "CREATE TABLE IF NOT EXISTS Route_Results ("
        "  result_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  plan_id INTEGER NOT NULL,"
        "  algorithm_name TEXT NOT NULL,"
        "  route_sequence TEXT NOT NULL,"
        "  total_distance REAL NOT NULL,"
        "  execution_time REAL NOT NULL,"
        "  nodes_explored INTEGER NOT NULL,"
        "  FOREIGN KEY (plan_id) REFERENCES Delivery_Plans(plan_id) ON DELETE CASCADE"
        ");";

    /* 4. Distance_Cache table */
    const char *sql_cache =
        "CREATE TABLE IF NOT EXISTS Distance_Cache ("
        "  source_location TEXT NOT NULL COLLATE NOCASE,"
        "  destination_location TEXT NOT NULL COLLATE NOCASE,"
        "  distance_km REAL NOT NULL,"
        "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (source_location, destination_location)"
        ");";

    char *err_msg = NULL;
    if (sqlite3_exec(g_db, sql_locations, NULL, NULL, &err_msg) != SQLITE_OK ||
        sqlite3_exec(g_db, sql_plans, NULL, NULL, &err_msg) != SQLITE_OK ||
        sqlite3_exec(g_db, sql_results, NULL, NULL, &err_msg) != SQLITE_OK ||
        sqlite3_exec(g_db, sql_cache, NULL, NULL, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "[DB ERROR] Table creation failed: %s\n", err_msg ? err_msg : "Unknown");
        sqlite3_free(err_msg);
        return 0;
    }

    printf("[DATABASE] SQLite database initialized successfully at '%s'\n", db_path);
    
    /* Seed well-known regional landmarks for instant, reliable zero-latency resolution */
    db_save_location("SRM University AP", 16.462400, 80.506400, "SRM University-AP, Neerukonda, Mangalagiri, Amaravati, Andhra Pradesh");
    db_save_location("SRM University", 16.462400, 80.506400, "SRM University-AP, Amaravati, Andhra Pradesh");
    db_save_location("SRM AP", 16.462400, 80.506400, "SRM University-AP, Amaravati, Andhra Pradesh");
    db_save_location("PVP Mall", 16.502306, 80.641868, "PVP Square / PVP Mall, MG Road, Vijayawada, Andhra Pradesh");
    db_save_location("PVP Square", 16.502306, 80.641868, "PVP Square, MG Road, Vijayawada, Andhra Pradesh");
    db_save_location("PVP", 16.502306, 80.641868, "PVP Square, MG Road, Vijayawada, Andhra Pradesh");
    db_save_location("Vijayawada", 16.506174, 80.648015, "Vijayawada, Krishna District, Andhra Pradesh");
    db_save_location("Vijayawada Airport", 16.527600, 80.796700, "Vijayawada International Airport, Gannavaram, Andhra Pradesh");
    db_save_location("Gannavaram Airport", 16.527600, 80.796700, "Vijayawada International Airport, Gannavaram, Andhra Pradesh");
    db_save_location("Vijayawada Railway Station", 16.518600, 80.620000, "Vijayawada Junction Railway Station, Andhra Pradesh");
    db_save_location("Trendset Mall", 16.504200, 80.652100, "Trendset Mall, Benz Circle, Vijayawada, Andhra Pradesh");
    db_save_location("Benz Circle", 16.498900, 80.653400, "Benz Circle, Vijayawada, Andhra Pradesh");
    db_save_location("Kanaka Durga Temple", 16.517300, 80.606500, "Sri Durga Malleswara Swamy Temple, Indrakeeladri, Vijayawada");
    db_save_location("VIT AP", 16.497100, 80.499200, "VIT-AP University, Inavolu, Amaravati, Andhra Pradesh");
    db_save_location("VIT AP University", 16.497100, 80.499200, "VIT-AP University, Inavolu, Amaravati, Andhra Pradesh");
    db_save_location("Mangalagiri", 16.435700, 80.569400, "Mangalagiri, Guntur District, Andhra Pradesh");
    db_save_location("Guntur", 16.306700, 80.436500, "Guntur, Andhra Pradesh");
    db_save_location("Guntur Bus Station", 16.306700, 80.436500, "NTR Bus Station, Guntur, Andhra Pradesh");
    db_save_location("Amaravati", 16.573500, 80.357500, "Amaravati, Capital City, Andhra Pradesh");
    db_save_location("Bhavani Island", 16.520400, 80.589800, "Bhavani Island, Krishna River, Vijayawada");
    db_save_location("Undavalli Caves", 16.496900, 80.580200, "Undavalli Caves, Guntur District, Andhra Pradesh");
    db_save_location("Pandit Nehru Bus Station", 16.510300, 80.617800, "Pandit Nehru Bus Station (PNBS), Vijayawada");
    db_save_location("PNBS", 16.510300, 80.617800, "Pandit Nehru Bus Station (PNBS), Vijayawada");
    db_save_location("AIIMS Mangalagiri", 16.447500, 80.556200, "AIIMS Mangalagiri, Andhra Pradesh");
    db_save_location("KL University", 16.441900, 80.622500, "KLEF (KL University), Vaddeswaram, Andhra Pradesh");
    db_save_location("Prakasam Barrage", 16.507400, 80.605300, "Prakasam Barrage, Krishna River, Vijayawada");

    return 1;
}

void db_close(void) {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

int db_get_location(const char *name, double *out_lat, double *out_lon, char *out_display_name, size_t max_len) {
    if (!g_db || !name) return 0;

    const char *sql = "SELECT latitude, longitude, display_name FROM Locations WHERE location_name = ? LIMIT 1;";
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (out_lat) *out_lat = sqlite3_column_double(stmt, 0);
            if (out_lon) *out_lon = sqlite3_column_double(stmt, 1);
            if (out_display_name && max_len > 0) {
                const unsigned char *disp = sqlite3_column_text(stmt, 2);
                if (disp) {
                    strncpy(out_display_name, (const char*)disp, max_len - 1);
                    out_display_name[max_len - 1] = '\0';
                } else {
                    out_display_name[0] = '\0';
                }
            }
            found = 1;
        }
        sqlite3_finalize(stmt);
    }

    /* 2. Fallback to fuzzy substring match if exact match not found */
    if (!found) {
        const char *fuzzy_sql = "SELECT latitude, longitude, display_name FROM Locations "
                               "WHERE (? LIKE '%' || location_name || '%' OR location_name LIKE '%' || ? || '%') "
                               "ORDER BY length(location_name) DESC LIMIT 1;";
        if (sqlite3_prepare_v2(g_db, fuzzy_sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                if (out_lat) *out_lat = sqlite3_column_double(stmt, 0);
                if (out_lon) *out_lon = sqlite3_column_double(stmt, 1);
                if (out_display_name && max_len > 0) {
                    const unsigned char *disp = sqlite3_column_text(stmt, 2);
                    if (disp) {
                        strncpy(out_display_name, (const char*)disp, max_len - 1);
                        out_display_name[max_len - 1] = '\0';
                    } else {
                        out_display_name[0] = '\0';
                    }
                }
                found = 1;
            }
            sqlite3_finalize(stmt);
        }
    }
    return found;
}

int db_save_location(const char *name, double lat, double lon, const char *display_name) {
    if (!g_db || !name) return 0;

    const char *sql = "INSERT INTO Locations (location_name, latitude, longitude, display_name) "
                      "VALUES (?, ?, ?, ?) "
                      "ON CONFLICT(location_name) DO UPDATE SET "
                      "latitude=excluded.latitude, longitude=excluded.longitude, display_name=excluded.display_name;";
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 2, lat);
        sqlite3_bind_double(stmt, 3, lon);
        sqlite3_bind_text(stmt, 4, display_name ? display_name : name, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = 1;
        }
        sqlite3_finalize(stmt);
    }
    return ok;
}

int db_get_cached_distance(const char *src, const char *dest, double *out_km) {
    if (!g_db || !src || !dest) return 0;

    const char *sql = "SELECT distance_km FROM Distance_Cache WHERE source_location = ? AND destination_location = ? LIMIT 1;";
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, src, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, dest, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (out_km) *out_km = sqlite3_column_double(stmt, 0);
            found = 1;
        }
        sqlite3_finalize(stmt);
    }
    return found;
}

int db_save_cached_distance(const char *src, const char *dest, double km) {
    if (!g_db || !src || !dest) return 0;

    const char *sql = "INSERT INTO Distance_Cache (source_location, destination_location, distance_km, updated_at) "
                      "VALUES (?, ?, ?, CURRENT_TIMESTAMP) "
                      "ON CONFLICT(source_location, destination_location) DO UPDATE SET "
                      "distance_km=excluded.distance_km, updated_at=CURRENT_TIMESTAMP;";
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, src, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, dest, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, km);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = 1;
        }
        sqlite3_finalize(stmt);
    }
    return ok;
}

int db_save_delivery_plan(const char *source, int total_locations, const char *locations_json, const char *matrix_json, int *out_plan_id) {
    if (!g_db || !source) return 0;

    const char *sql = "INSERT INTO Delivery_Plans (source_location, total_locations, locations_json, matrix_json) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, source, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, total_locations);
        sqlite3_bind_text(stmt, 3, locations_json ? locations_json : "[]", -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, matrix_json ? matrix_json : "[]", -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            if (out_plan_id) {
                *out_plan_id = (int)sqlite3_last_insert_rowid(g_db);
            }
            ok = 1;
        }
        sqlite3_finalize(stmt);
    }
    return ok;
}

int db_save_route_result(int plan_id, const char *algorithm_name, const char *route_sequence, double total_distance, double execution_time, long long nodes_explored) {
    if (!g_db || !algorithm_name || !route_sequence) return 0;

    const char *sql = "INSERT INTO Route_Results (plan_id, algorithm_name, route_sequence, total_distance, execution_time, nodes_explored) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, plan_id);
        sqlite3_bind_text(stmt, 2, algorithm_name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, route_sequence, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 4, total_distance);
        sqlite3_bind_double(stmt, 5, execution_time);
        sqlite3_bind_int64(stmt, 6, nodes_explored);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = 1;
        }
        sqlite3_finalize(stmt);
    }
    return ok;
}

char* db_get_history_json(void) {
    if (!g_db) return safe_strdup("[]");

    const char *sql =
        "SELECT p.plan_id, p.source_location, p.total_locations, p.created_at, "
        "       rb.total_distance AS bb_dist, rb.execution_time AS bb_time, rb.route_sequence AS bb_route, "
        "       rn.total_distance AS nn_dist, rn.execution_time AS nn_time, rn.route_sequence AS nn_route "
        "FROM Delivery_Plans p "
        "LEFT JOIN Route_Results rb ON p.plan_id = rb.plan_id AND rb.algorithm_name = 'Branch and Bound' "
        "LEFT JOIN Route_Results rn ON p.plan_id = rn.plan_id AND rn.algorithm_name = 'Nearest Neighbor' "
        "ORDER BY p.plan_id DESC LIMIT 50;";

    sqlite3_stmt *stmt = NULL;
    cJSON *root = cJSON_CreateArray();

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "plan_id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(item, "source_location", (const char*)sqlite3_column_text(stmt, 1));
            cJSON_AddNumberToObject(item, "total_locations", sqlite3_column_int(stmt, 2));
            cJSON_AddStringToObject(item, "created_at", (const char*)sqlite3_column_text(stmt, 3));

            if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
                cJSON_AddNumberToObject(item, "bb_distance", sqlite3_column_double(stmt, 4));
                cJSON_AddNumberToObject(item, "bb_time_ms", sqlite3_column_double(stmt, 5));
                cJSON_AddStringToObject(item, "bb_route", (const char*)sqlite3_column_text(stmt, 6));
            } else {
                cJSON_AddNullToObject(item, "bb_distance");
            }

            if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
                cJSON_AddNumberToObject(item, "nn_distance", sqlite3_column_double(stmt, 7));
                cJSON_AddNumberToObject(item, "nn_time_ms", sqlite3_column_double(stmt, 8));
                cJSON_AddStringToObject(item, "nn_route", (const char*)sqlite3_column_text(stmt, 9));
            } else {
                cJSON_AddNullToObject(item, "nn_distance");
            }

            cJSON_AddItemToArray(root, item);
        }
        sqlite3_finalize(stmt);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str ? json_str : safe_strdup("[]");
}

char* db_get_plan_details_json(int plan_id) {
    if (!g_db) return NULL;

    const char *sql = "SELECT plan_id, source_location, total_locations, locations_json, matrix_json, created_at FROM Delivery_Plans WHERE plan_id = ? LIMIT 1;";
    sqlite3_stmt *stmt = NULL;
    cJSON *root = NULL;

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, plan_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "plan_id", sqlite3_column_int(stmt, 0));
            cJSON_AddStringToObject(root, "source_location", (const char*)sqlite3_column_text(stmt, 1));
            cJSON_AddNumberToObject(root, "total_locations", sqlite3_column_int(stmt, 2));

            const char *loc_str = (const char*)sqlite3_column_text(stmt, 3);
            cJSON *locs = loc_str ? cJSON_Parse(loc_str) : NULL;
            if (locs) cJSON_AddItemToObject(root, "locations", locs);
            else cJSON_AddArrayToObject(root, "locations");

            const char *mat_str = (const char*)sqlite3_column_text(stmt, 4);
            cJSON *mat = mat_str ? cJSON_Parse(mat_str) : NULL;
            if (mat) cJSON_AddItemToObject(root, "matrix", mat);
            else cJSON_AddArrayToObject(root, "matrix");

            cJSON_AddStringToObject(root, "created_at", (const char*)sqlite3_column_text(stmt, 5));
        }
        sqlite3_finalize(stmt);
    }

    if (!root) return NULL;

    /* Fetch route results for this plan */
    const char *sql_res = "SELECT algorithm_name, route_sequence, total_distance, execution_time, nodes_explored FROM Route_Results WHERE plan_id = ?;";
    cJSON *results_arr = cJSON_CreateArray();

    if (sqlite3_prepare_v2(g_db, sql_res, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, plan_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            cJSON *r = cJSON_CreateObject();
            cJSON_AddStringToObject(r, "algorithm_name", (const char*)sqlite3_column_text(stmt, 0));
            cJSON_AddStringToObject(r, "route_sequence", (const char*)sqlite3_column_text(stmt, 1));
            cJSON_AddNumberToObject(r, "total_distance", sqlite3_column_double(stmt, 2));
            cJSON_AddNumberToObject(r, "execution_time", sqlite3_column_double(stmt, 3));
            cJSON_AddNumberToObject(r, "nodes_explored", sqlite3_column_int64(stmt, 4));
            cJSON_AddItemToArray(results_arr, r);
        }
        sqlite3_finalize(stmt);
    }
    cJSON_AddItemToObject(root, "results", results_arr);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

int db_delete_plan(int plan_id) {
    if (!g_db) return 0;
    const char *sql = "DELETE FROM Delivery_Plans WHERE plan_id = ?;";
    sqlite3_stmt *stmt = NULL;
    int ok = 0;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, plan_id);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            ok = 1;
        }
        sqlite3_finalize(stmt);
    }
    return ok;
}
