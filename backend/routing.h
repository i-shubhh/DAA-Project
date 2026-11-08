#ifndef ROUTING_H
#define ROUTING_H

#include "../Common/route.h"

/**
 * Builds the directed NxN road distance cost matrix using OSRM Table API.
 * Uses SQLite cache for previously queried location pairs.
 * 
 * @param num_locs Number of locations (2 to MAX_LOCATIONS).
 * @param lats Array of latitudes.
 * @param lons Array of longitudes.
 * @param names Array of location names.
 * @param out_matrix Output 2D matrix populated with road distances in kilometers.
 * @return 1 on success, 0 on failure.
 */
int calculate_road_distance_matrix(int num_locs, const double lats[], const double lons[],
                                   const char names[][MAX_NAME_LEN],
                                   double out_matrix[MAX_LOCATIONS][MAX_LOCATIONS]);

/**
 * Fetches the real road-route GeoJSON geometry from OSRM for a sequence of visited locations.
 * 
 * @param path_len Number of points along the path (e.g. N + 1 for complete cycle).
 * @param path Array of visited location indices.
 * @param lats Array of latitudes of all locations.
 * @param lons Array of longitudes of all locations.
 * @return Dynamically allocated JSON string containing GeoJSON geometry (must be freed by caller), or NULL on failure.
 */
char* fetch_route_geometry_geojson(int path_len, const int *path, const double lats[], const double lons[]);

#endif /* ROUTING_H */

