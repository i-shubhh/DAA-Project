#ifndef GEOCODING_H
#define GEOCODING_H

#include <stddef.h>

/**
 * Geocodes an address or place name using OpenStreetMap Nominatim.
 * Caches and checks results in SQLite to avoid repeated requests.
 * 
 * @param place_name Input location query (e.g. "SRM University, Andhra Pradesh").
 * @param out_lat Output latitude.
 * @param out_lon Output longitude.
 * @param out_display_name Output standardized address string.
 * @param max_len Maximum length of out_display_name buffer.
 * @return 1 on success, 0 on failure.
 */
int geocode_location(const char *place_name, double *out_lat, double *out_lon, char *out_display_name, size_t max_len);

#endif /* GEOCODING_H */

