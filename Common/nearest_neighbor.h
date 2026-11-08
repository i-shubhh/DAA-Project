#ifndef NEAREST_NEIGHBOR_H
#define NEAREST_NEIGHBOR_H

#include "route.h"

/**
 * Solves the Travelling Salesperson Problem (TSP) using the Nearest Neighbor heuristic.
 * 
 * Algorithm Characteristics:
 * - Strategy: Greedy Heuristic (Approximate solution).
 * - Time Complexity: O(N^2)
 * - Space Complexity: O(N)
 * - Optimality: Suboptimal / Local optima (No guarantee of global minimum).
 * 
 * @param net Pointer to the delivery network with cost matrix.
 * @param start_index The starting location index (e.g. Central Warehouse).
 * @return RouteResult containing route sequence, total cost, and execution metrics.
 */
RouteResult solve_tsp_nearest_neighbor(const DeliveryNetwork *net, int start_index);

#endif /* NEAREST_NEIGHBOR_H */

