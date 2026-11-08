#ifndef ROUTE_H
#define ROUTE_H

#include <stddef.h>

#define MAX_LOCATIONS 20
#define MAX_NAME_LEN 64
#define INF 1000000000.0

/**
 * Structure representing the e-commerce delivery network.
 * Supports directed (asymmetric) travel costs between locations.
 */
typedef struct {
    int num_locations;
    char names[MAX_LOCATIONS][MAX_NAME_LEN];
    double cost_matrix[MAX_LOCATIONS][MAX_LOCATIONS];
} DeliveryNetwork;

/**
 * Structure storing the results of a TSP solver algorithm.
 */
typedef struct {
    int num_locations;
    int path[MAX_LOCATIONS + 1];   /* Sequence of visited location indices: path[0] ... path[N] = path[0] */
    double total_cost;             /* Sum of travel costs along the route */
    int is_valid;                  /* 1 if a valid Hamiltonian cycle exists, 0 otherwise */
    char message[256];             /* Status message or error description */
    long long nodes_explored;      /* Number of state space tree nodes or decision steps evaluated */
    double execution_time_ms;      /* Execution time in milliseconds */
} RouteResult;

/* Initialization and Setup */
void init_network(DeliveryNetwork *net, int num_locations);
void set_location_name(DeliveryNetwork *net, int index, const char *name);
void set_cost(DeliveryNetwork *net, int from, int to, double cost);

/* Validation */
int validate_network(const DeliveryNetwork *net, char *err_buf, size_t buf_len);

/* Cost and Verification */
double calculate_route_cost(const DeliveryNetwork *net, const int *path, int len);
int verify_hamiltonian_cycle(const DeliveryNetwork *net, const RouteResult *res, int start_index);

/* Formatting and Display */
void format_route_string(const DeliveryNetwork *net, const RouteResult *res, char *buffer, size_t buf_len);

/* Benchmark Sample Datasets */
void load_sample_network(DeliveryNetwork *net, int sample_id);
const char* get_sample_name(int sample_id);

/* Ground-Truth Verification Solver (for validation testing on small N <= 8) */
RouteResult solve_tsp_brute_force(const DeliveryNetwork *net, int start_index);

#endif /* ROUTE_H */
