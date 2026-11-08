#ifndef DATABASE_H
#define DATABASE_H

#include <stddef.h>

/**
 * Initializes the SQLite database and creates tables if they do not exist.
 * @param db_path Path to the SQLite database file (e.g. "database/delivery.db").
 * @return 1 on success, 0 on failure.
 */
int db_init(const char *db_path);

/**
 * Closes the database connection.
 */
void db_close(void);

/**
 * Retrieves cached geocoded coordinates for a location name.
 * @return 1 if found in cache, 0 otherwise.
 */
int db_get_location(const char *name, double *out_lat, double *out_lon, char *out_display_name, size_t max_len);

/**
 * Caches geocoded location coordinates.
 * @return 1 on success, 0 on failure.
 */
int db_save_location(const char *name, double lat, double lon, const char *display_name);

/**
 * Retrieves cached road distance between two locations.
 * @return 1 if found in cache, 0 otherwise.
 */
int db_get_cached_distance(const char *src, const char *dest, double *out_km);

/**
 * Caches road distance between two locations.
 * @return 1 on success, 0 on failure.
 */
int db_save_cached_distance(const char *src, const char *dest, double km);

/**
 * Saves a new delivery plan record.
 * @return 1 on success, 0 on failure.
 */
int db_save_delivery_plan(const char *source, int total_locations, const char *locations_json, const char *matrix_json, int *out_plan_id);

/**
 * Saves an algorithm execution result for a delivery plan.
 * @return 1 on success, 0 on failure.
 */
int db_save_route_result(int plan_id, const char *algorithm_name, const char *route_sequence, double total_distance, double execution_time, long long nodes_explored);

/**
 * Returns a JSON array of all past delivery plans with route summaries.
 * The caller must free() the returned string.
 */
char* db_get_history_json(void);

/**
 * Returns detailed JSON of a specific delivery plan including its cost matrix and route results.
 * The caller must free() the returned string.
 */
char* db_get_plan_details_json(int plan_id);

/**
 * Deletes a delivery plan and its associated results.
 * @return 1 on success, 0 on failure.
 */
int db_delete_plan(int plan_id);

#endif /* DATABASE_H */

