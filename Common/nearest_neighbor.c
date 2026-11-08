#include "nearest_neighbor.h"
#include "route.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

RouteResult solve_tsp_nearest_neighbor(const DeliveryNetwork *net,
                                       int start_index) {
  RouteResult res;
  memset(&res, 0, sizeof(RouteResult));
  res.is_valid = 0;
  res.total_cost = INF;

  clock_t start_time = clock();

  if (!net) {
    snprintf(res.message, sizeof(res.message),
             "Error: Delivery network is NULL.");
    return res;
  }

  int n = net->num_locations;
  res.num_locations = n;

  if (n < 2) {
    snprintf(res.message, sizeof(res.message),
             "Error: At least 2 delivery locations required.");
    return res;
  }

  if (start_index < 0 || start_index >= n) {
    snprintf(res.message, sizeof(res.message),
             "Error: Invalid starting location index %d.", start_index);
    return res;
  }

  int visited[MAX_LOCATIONS] = {0};
  res.path[0] = start_index;
  visited[start_index] = 1;

  int current = start_index;
  double accumulated_cost = 0.0;
  long long decision_steps = 0;

  /* Greedily visit the nearest unvisited neighbor */
  for (int step = 1; step < n; step++) {
    int best_next = -1;
    double min_edge_cost = INF;

    for (int candidate = 0; candidate < n; candidate++) {
      decision_steps++;
      if (!visited[candidate] && candidate != current) {
        double cost = net->cost_matrix[current][candidate];
        if (cost < min_edge_cost) {
          min_edge_cost = cost;
          best_next = candidate;
        }
      }
    }

    /* Check if a reachable unvisited location was found */
    if (best_next == -1 || min_edge_cost >= INF / 2.0) {
      snprintf(res.message, sizeof(res.message),
               "Nearest Neighbor failed: Dead end reached at '%s'. No outgoing "
               "edge to unvisited locations.",
               net->names[current]);
      clock_t end_time = clock();
      res.execution_time_ms =
          ((double)(end_time - start_time) * 1000.0) / CLOCKS_PER_SEC;
      res.nodes_explored = decision_steps;
      return res;
    }

    visited[best_next] = 1;
    res.path[step] = best_next;
    accumulated_cost += min_edge_cost;
    current = best_next;
  }

  /* Complete the round trip: return to start_index */
  double return_cost = net->cost_matrix[current][start_index];
  decision_steps++;
  if (return_cost >= INF / 2.0) {
    snprintf(res.message, sizeof(res.message),
             "Nearest Neighbor failed: Cannot return to starting point '%s' "
             "from last location '%s'.",
             net->names[start_index], net->names[current]);
    clock_t end_time = clock();
    res.execution_time_ms =
        ((double)(end_time - start_time) * 1000.0) / CLOCKS_PER_SEC;
    res.nodes_explored = decision_steps;
    return res;
  }

  res.path[n] = start_index;
  accumulated_cost += return_cost;

  clock_t end_time = clock();
  res.execution_time_ms =
      ((double)(end_time - start_time) * 1000.0) / CLOCKS_PER_SEC;
  res.nodes_explored = decision_steps;
  res.total_cost = accumulated_cost;
  res.is_valid = 1;
  snprintf(res.message, sizeof(res.message),
           "Approximate tour found by Nearest Neighbor heuristic (Note: greedy "
           "choice, not guaranteed optimal).");

  return res;
}
