#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "route.h"

void init_network(DeliveryNetwork *net, int num_locations) {
    if (!net) return;
    if (num_locations < 2) num_locations = 2;
    if (num_locations > MAX_LOCATIONS) num_locations = MAX_LOCATIONS;

    net->num_locations = num_locations;
    for (int i = 0; i < MAX_LOCATIONS; i++) {
        snprintf(net->names[i], MAX_NAME_LEN, "Location %d", i + 1);
        for (int j = 0; j < MAX_LOCATIONS; j++) {
            if (i == j) {
                net->cost_matrix[i][j] = INF; /* No self-loops allowed in TSP */
            } else {
                net->cost_matrix[i][j] = INF;
            }
        }
    }
}

void set_location_name(DeliveryNetwork *net, int index, const char *name) {
    if (!net || index < 0 || index >= MAX_LOCATIONS || !name) return;
    strncpy(net->names[index], name, MAX_NAME_LEN - 1);
    net->names[index][MAX_NAME_LEN - 1] = '\0';
}

void set_cost(DeliveryNetwork *net, int from, int to, double cost) {
    if (!net || from < 0 || from >= MAX_LOCATIONS || to < 0 || to >= MAX_LOCATIONS) return;
    if (from == to) {
        net->cost_matrix[from][to] = INF;
    } else {
        net->cost_matrix[from][to] = cost;
    }
}

int validate_network(const DeliveryNetwork *net, char *err_buf, size_t buf_len) {
    if (!net) {
        if (err_buf) snprintf(err_buf, buf_len, "Error: Network pointer is NULL.");
        return 0;
    }

    if (net->num_locations < 2) {
        if (err_buf) snprintf(err_buf, buf_len, "Error: Number of delivery locations must be at least 2 (got %d).", net->num_locations);
        return 0;
    }

    if (net->num_locations > MAX_LOCATIONS) {
        if (err_buf) snprintf(err_buf, buf_len, "Error: Number of delivery locations exceeds maximum limit of %d (got %d).", MAX_LOCATIONS, net->num_locations);
        return 0;
    }

    /* Validate cost matrix values */
    for (int i = 0; i < net->num_locations; i++) {
        int outgoing_edges = 0;
        int incoming_edges = 0;

        for (int j = 0; j < net->num_locations; j++) {
            if (i == j) continue;
            double c = net->cost_matrix[i][j];
            if (c < 0.0) {
                if (err_buf) snprintf(err_buf, buf_len, "Error: Negative travel cost (%.2f) from '%s' to '%s' is not allowed.",
                                      c, net->names[i], net->names[j]);
                return 0;
            }
            if (c < INF / 2.0) {
                outgoing_edges++;
            }
            if (net->cost_matrix[j][i] < INF / 2.0) {
                incoming_edges++;
            }
        }

        if (outgoing_edges == 0) {
            if (err_buf) snprintf(err_buf, buf_len, "Warning: Location '%s' [%d] has no outgoing travel routes.", net->names[i], i);
            return 0;
        }
        if (incoming_edges == 0) {
            if (err_buf) snprintf(err_buf, buf_len, "Warning: Location '%s' [%d] has no incoming travel routes.", net->names[i], i);
            return 0;
        }
    }

    if (err_buf) snprintf(err_buf, buf_len, "Network topology and cost matrix are valid.");
    return 1;
}

double calculate_route_cost(const DeliveryNetwork *net, const int *path, int len) {
    if (!net || !path || len <= 1) return INF;

    double total = 0.0;
    for (int i = 0; i < len - 1; i++) {
        int u = path[i];
        int v = path[i + 1];
        if (u < 0 || u >= net->num_locations || v < 0 || v >= net->num_locations) {
            return INF;
        }
        double edge_cost = net->cost_matrix[u][v];
        if (edge_cost >= INF / 2.0) {
            return INF; /* Broken or impossible link */
        }
        total += edge_cost;
    }
    return total;
}

int verify_hamiltonian_cycle(const DeliveryNetwork *net, const RouteResult *res, int start_index) {
    if (!net || !res || !res->is_valid) return 0;
    int n = net->num_locations;
    if (res->num_locations != n) return 0;
    if (res->path[0] != start_index || res->path[n] != start_index) return 0;

    int visited[MAX_LOCATIONS] = {0};
    for (int i = 0; i < n; i++) {
        int u = res->path[i];
        if (u < 0 || u >= n) return 0;
        if (visited[u]) return 0; /* Visited more than once */
        visited[u] = 1;

        int next = res->path[i + 1];
        if (net->cost_matrix[u][next] >= INF / 2.0) return 0; /* Invalid edge */
    }

    /* Check all visited */
    for (int i = 0; i < n; i++) {
        if (!visited[i]) return 0;
    }

    return 1;
}

void format_route_string(const DeliveryNetwork *net, const RouteResult *res, char *buffer, size_t buf_len) {
    if (!net || !res || !buffer || buf_len == 0) return;

    if (!res->is_valid) {
        snprintf(buffer, buf_len, "No valid route found: %s", res->message);
        return;
    }

    buffer[0] = '\0';
    size_t offset = 0;
    int total_nodes = net->num_locations + 1;

    for (int i = 0; i < total_nodes; i++) {
        int idx = res->path[i];
        const char *name = (idx >= 0 && idx < net->num_locations) ? net->names[idx] : "Unknown";
        int written;
        if (i == 0) {
            written = snprintf(buffer + offset, buf_len - offset, "[%d] %s", idx, name);
        } else {
            written = snprintf(buffer + offset, buf_len - offset, " -> [%d] %s", idx, name);
        }
        if (written < 0 || (size_t)written >= buf_len - offset) {
            break; /* Buffer full */
        }
        offset += written;
    }
}

const char* get_sample_name(int sample_id) {
    switch (sample_id) {
        case 1: return "Sample 1: 4 Locations (Urban Delivery Circuit - Asymmetric)";
        case 2: return "Sample 2: 5 Locations (Regional Fulfillment Centers - Classic ATSP)";
        case 3: return "Sample 3: 6 Locations (Metro Express Distribution Network)";
        case 4: return "Sample 4: 4 Locations (Disconnected Graph - Edge Case Test)";
        default: return "Unknown Sample";
    }
}

void load_sample_network(DeliveryNetwork *net, int sample_id) {
    if (!net) return;

    if (sample_id == 1) {
        /* 4 Locations: Urban Delivery Circuit (Asymmetric due to one-way streets & traffic) */
        init_network(net, 4);
        set_location_name(net, 0, "Central Warehouse");
        set_location_name(net, 1, "Downtown Hub");
        set_location_name(net, 2, "North Suburb Depot");
        set_location_name(net, 3, "South Suburb Depot");

        double matrix[4][4] = {
            {INF,  12.0, 25.0, 20.0},
            {10.0, INF,  15.0, 18.0},
            {22.0, 14.0, INF,  30.0},
            {19.0, 16.0, 28.0, INF}
        };

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                net->cost_matrix[i][j] = matrix[i][j];
            }
        }
    } else if (sample_id == 2) {
        /* 5 Locations: Regional Fulfillment Centers (Classic ATSP) */
        init_network(net, 5);
        set_location_name(net, 0, "Fulfillment Center");
        set_location_name(net, 1, "Metro North Hub");
        set_location_name(net, 2, "West Suburb Depot");
        set_location_name(net, 3, "East Terminal");
        set_location_name(net, 4, "South Depot");

        double matrix[5][5] = {
            {INF,  20.0, 30.0, 10.0, 11.0},
            {15.0, INF,  16.0,  4.0,  2.0},
            { 3.0,  5.0, INF,   2.0,  4.0},
            {19.0,  6.0, 18.0, INF,   3.0},
            {16.0,  4.0,  7.0, 16.0, INF}
        };

        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                net->cost_matrix[i][j] = matrix[i][j];
            }
        }
    } else if (sample_id == 3) {
        /* 6 Locations: Metro Express Distribution Network */
        init_network(net, 6);
        set_location_name(net, 0, "Primary Logistics Hub");
        set_location_name(net, 1, "Zone A (Commercial)");
        set_location_name(net, 2, "Zone B (Residential)");
        set_location_name(net, 3, "Zone C (Industrial)");
        set_location_name(net, 4, "Zone D (Airport Cargo)");
        set_location_name(net, 5, "Zone E (Harbor Depot)");

        double matrix[6][6] = {
            {INF,  15.0, 25.0, 35.0, 40.0, 20.0},
            {14.0, INF,  10.0, 28.0, 32.0, 18.0},
            {26.0, 12.0, INF,  14.0, 22.0, 30.0},
            {33.0, 27.0, 16.0, INF,  12.0, 25.0},
            {38.0, 30.0, 20.0, 15.0, INF,  10.0},
            {22.0, 17.0, 29.0, 24.0, 11.0, INF}
        };

        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 6; j++) {
                net->cost_matrix[i][j] = matrix[i][j];
            }
        }
    } else if (sample_id == 4) {
        /* 4 Locations: Disconnected Graph (Edge case testing when no Hamiltonian tour is possible) */
        init_network(net, 4);
        set_location_name(net, 0, "Warehouse");
        set_location_name(net, 1, "Hub 1");
        set_location_name(net, 2, "Isolated Hub 2");
        set_location_name(net, 3, "Hub 3");

        double matrix[4][4] = {
            {INF,  10.0, INF,  20.0},
            {12.0, INF,  15.0, 25.0},
            {INF,  INF,  INF,  INF}, /* Isolated: no outgoing edges */
            {18.0, 22.0, 30.0, INF}
        };

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                net->cost_matrix[i][j] = matrix[i][j];
            }
        }
    }
}

/* Helper for brute-force permutations */
static void brute_force_recurse(const DeliveryNetwork *net, int start_index, int n,
                                int *current_path, int level, int *visited,
                                double *best_cost, int *best_path, long long *tours_evaluated) {
    if (level == n) {
        int last_v = current_path[n - 1];
        double return_edge = net->cost_matrix[last_v][start_index];
        (*tours_evaluated)++;
        if (return_edge < INF / 2.0) {
            current_path[n] = start_index;
            double cost = calculate_route_cost(net, current_path, n + 1);
            if (cost < *best_cost) {
                *best_cost = cost;
                memcpy(best_path, current_path, (size_t)(n + 1) * sizeof(int));
            }
        }
        return;
    }

    for (int v = 0; v < n; v++) {
        if (!visited[v]) {
            int prev_v = current_path[level - 1];
            if (net->cost_matrix[prev_v][v] < INF / 2.0) {
                visited[v] = 1;
                current_path[level] = v;
                brute_force_recurse(net, start_index, n, current_path, level + 1, visited,
                                    best_cost, best_path, tours_evaluated);
                visited[v] = 0;
            }
        }
    }
}

RouteResult solve_tsp_brute_force(const DeliveryNetwork *net, int start_index) {
    RouteResult res;
    memset(&res, 0, sizeof(RouteResult));
    res.is_valid = 0;
    res.total_cost = INF;

    clock_t start_t = clock();

    if (!net || net->num_locations < 2 || net->num_locations > 10) {
        snprintf(res.message, sizeof(res.message), "Brute force solver supported for 2 to 10 locations.");
        return res;
    }

    int n = net->num_locations;
    res.num_locations = n;

    int current_path[MAX_LOCATIONS + 1];
    int best_path[MAX_LOCATIONS + 1];
    int visited[MAX_LOCATIONS] = {0};
    double best_cost = INF;
    long long tours_evaluated = 0;

    current_path[0] = start_index;
    visited[start_index] = 1;

    brute_force_recurse(net, start_index, n, current_path, 1, visited,
                        &best_cost, best_path, &tours_evaluated);

    clock_t end_t = clock();
    res.execution_time_ms = ((double)(end_t - start_t) * 1000.0) / CLOCKS_PER_SEC;
    res.nodes_explored = tours_evaluated;

    if (best_cost < INF / 2.0) {
        res.is_valid = 1;
        res.total_cost = best_cost;
        memcpy(res.path, best_path, (size_t)(n + 1) * sizeof(int));
        snprintf(res.message, sizeof(res.message), "Brute force optimal route found.");
    } else {
        res.is_valid = 0;
        res.total_cost = INF;
        snprintf(res.message, sizeof(res.message), "No valid Hamiltonian cycle exists.");
    }

    return res;
}
