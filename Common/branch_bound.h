#ifndef BRANCH_BOUND_H
#define BRANCH_BOUND_H

#include "route.h"

/**
  Solves the Travelling Salesperson Problem (TSP) using the Branch and Bound
 algorithm
 * based on the Reduced Cost Matrix method (Horowitz & Sahni / Little's
 Formulation).
 *
 * Algorithm Characteristics:
 * - Strategy: Exact State Space Tree Exploration with Least-Cost Branch and
 Bound (LCBB).
 * - Bounding: Matrix reduction (row and column lower bound reductions).
 * - Pruning: Eliminates branches with Lower Bound >= current best solution.
 * - Time Complexity: O(N^2 * 2^N) empirical average with matrix reduction lower
 bounding;
 *                    O(N! * N^2) theoretical worst-case when no branches are
 pruned.
 * - Space Complexity: O(M * N^2) where M is the maximum number of live active
 nodes
 *                     in the priority queue heap.
 * - Optimality: Guaranteed exact global optimal minimum-cost tour.
 *
 * @param net Pointer to the delivery network with cost matrix.
 * @param start_index The starting location index (e.g. Central Warehouse).
 * @return RouteResult containing exact optimal route sequence, minimum cost,
 and metrics.
 */
RouteResult solve_tsp_branch_bound(const DeliveryNetwork *net, int start_index);

#endif /* BRANCH_BOUND_H */
