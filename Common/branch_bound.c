#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "branch_bound.h"
#include "nearest_neighbor.h"
#include "route.h"

#define INITIAL_HEAP_CAPACITY 4096

typedef struct {
    int level;                                      /* Level in the state space tree (0 to n-1) */
    int vertex;                                     /* Current delivery location */
    double bound;                                   /* Lower bound computed by matrix reduction */
    int path[MAX_LOCATIONS + 1];                    /* Partial path from root */
    int visited[MAX_LOCATIONS];                     /* Visited status of each location */
    double matrix[MAX_LOCATIONS][MAX_LOCATIONS];     /* Reduced cost matrix for this state */
} BBNode;

/* Min-Heap Priority Queue for Least-Cost Branch and Bound (LCBB) */
typedef struct {
    BBNode **nodes;
    int size;
    int capacity;
} MinHeap;

static MinHeap* create_heap(int initial_cap) {
    MinHeap *h = (MinHeap*)malloc(sizeof(MinHeap));
    if (!h) return NULL;
    h->capacity = initial_cap;
    h->size = 0;
    h->nodes = (BBNode**)malloc(sizeof(BBNode*) * h->capacity);
    if (!h->nodes) {
        free(h);
        return NULL;
    }
    return h;
}

static void free_heap(MinHeap *h) {
    if (!h) return;
    for (int i = 0; i < h->size; i++) {
        if (h->nodes[i]) free(h->nodes[i]);
    }
    free(h->nodes);
    free(h);
}

/* Dynamically expands heap capacity without arbitrary limits; returns 1 on success, 0 on OOM */
static int heap_push(MinHeap *h, BBNode *node) {
    if (!h || !node) return 0;

    if (h->size >= h->capacity) {
        int new_cap = h->capacity * 2;
        BBNode **new_nodes = (BBNode**)realloc(h->nodes, sizeof(BBNode*) * new_cap);
        if (!new_nodes) {
            /* Out of memory: do NOT silently discard, notify caller */
            return 0;
        }
        h->nodes = new_nodes;
        h->capacity = new_cap;
    }

    int i = h->size++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->nodes[parent]->bound <= node->bound) break;
        h->nodes[i] = h->nodes[parent];
        i = parent;
    }
    h->nodes[i] = node;
    return 1;
}

static BBNode* heap_pop(MinHeap *h) {
    if (!h || h->size == 0) return NULL;
    BBNode *top = h->nodes[0];
    BBNode *last = h->nodes[--h->size];

    if (h->size > 0) {
        int i = 0;
        while (i * 2 + 1 < h->size) {
            int left = i * 2 + 1;
            int right = i * 2 + 2;
            int smallest = left;
            if (right < h->size && h->nodes[right]->bound < h->nodes[left]->bound) {
                smallest = right;
            }
            if (last->bound <= h->nodes[smallest]->bound) break;
            h->nodes[i] = h->nodes[smallest];
            i = smallest;
        }
        h->nodes[i] = last;
    }
    return top;
}

/* Prunes obsolete nodes from the heap whose lower bound exceeds new best_cost, compacting memory */
static void heap_prune(MinHeap *h, double best_cost) {
    if (!h || h->size == 0) return;

    int new_size = 0;
    for (int i = 0; i < h->size; i++) {
        if (h->nodes[i]->bound < best_cost) {
            h->nodes[new_size++] = h->nodes[i];
        } else {
            free(h->nodes[i]);
        }
    }
    h->size = new_size;

    /* Reconstruct min-heap property in O(size) via bottom-up heapify */
    for (int i = (h->size / 2) - 1; i >= 0; i--) {
        int parent = i;
        while (parent * 2 + 1 < h->size) {
            int left = parent * 2 + 1;
            int right = parent * 2 + 2;
            int smallest = left;
            if (right < h->size && h->nodes[right]->bound < h->nodes[left]->bound) {
                smallest = right;
            }
            if (h->nodes[parent]->bound <= h->nodes[smallest]->bound) break;
            BBNode *tmp = h->nodes[parent];
            h->nodes[parent] = h->nodes[smallest];
            h->nodes[smallest] = tmp;
            parent = smallest;
        }
    }
}

/* Performs row and column reduction on the cost matrix and returns total reduction cost */
static double reduce_matrix(double matrix[MAX_LOCATIONS][MAX_LOCATIONS], int n) {
    double reduction_cost = 0.0;

    /* 1. Row reduction: subtract row minimum from each row */
    for (int i = 0; i < n; i++) {
        double row_min = INF;
        for (int j = 0; j < n; j++) {
            if (matrix[i][j] < row_min) {
                row_min = matrix[i][j];
            }
        }
        if (row_min < INF / 2.0 && row_min > 0.0) {
            reduction_cost += row_min;
            for (int j = 0; j < n; j++) {
                if (matrix[i][j] < INF / 2.0) {
                    matrix[i][j] -= row_min;
                }
            }
        }
    }

    /* 2. Column reduction: subtract column minimum from each column */
    for (int j = 0; j < n; j++) {
        double col_min = INF;
        for (int i = 0; i < n; i++) {
            if (matrix[i][j] < col_min) {
                col_min = matrix[i][j];
            }
        }
        if (col_min < INF / 2.0 && col_min > 0.0) {
            reduction_cost += col_min;
            for (int i = 0; i < n; i++) {
                if (matrix[i][j] < INF / 2.0) {
                    matrix[i][j] -= col_min;
                }
            }
        }
    }

    return reduction_cost;
}

/* Creates a child node in the state space tree for transition u -> v */
static BBNode* create_child_node(const BBNode *parent, int v, int start_index, int n) {
    BBNode *child = (BBNode*)malloc(sizeof(BBNode));
    if (!child) return NULL;

    child->level = parent->level + 1;
    child->vertex = v;

    /* Copy path and mark visited */
    memcpy(child->path, parent->path, (size_t)(parent->level + 1) * sizeof(int));
    child->path[child->level] = v;

    memcpy(child->visited, parent->visited, sizeof(child->visited));
    child->visited[v] = 1;

    /* Copy matrix from parent */
    memcpy(child->matrix, parent->matrix, sizeof(child->matrix));

    int u = parent->vertex;
    double edge_reduced_cost = parent->matrix[u][v];

    /* Invalidate row u and column v */
    for (int j = 0; j < n; j++) {
        child->matrix[u][j] = INF;
    }
    for (int i = 0; i < n; i++) {
        child->matrix[i][v] = INF;
    }

    /* Prevent premature cycle back to start unless reaching last vertex */
    if (child->level < n - 1) {
        child->matrix[v][start_index] = INF;
    }

    /* Prevent immediate 2-cycles between v and u */
    child->matrix[v][u] = INF;

    /* Reduce child matrix */
    double child_reduction = reduce_matrix(child->matrix, n);

    /* Lower Bound Calculation */
    child->bound = parent->bound + edge_reduced_cost + child_reduction;

    return child;
}

RouteResult solve_tsp_branch_bound(const DeliveryNetwork *net, int start_index) {
    RouteResult res;
    memset(&res, 0, sizeof(RouteResult));
    res.is_valid = 0;
    res.total_cost = INF;

    clock_t start_clock = clock();

    if (!net) {
        snprintf(res.message, sizeof(res.message), "Error: Delivery network is NULL.");
        return res;
    }

    int n = net->num_locations;
    res.num_locations = n;

    if (n < 2) {
        snprintf(res.message, sizeof(res.message), "Error: At least 2 delivery locations required.");
        return res;
    }

    if (start_index < 0 || start_index >= n) {
        snprintf(res.message, sizeof(res.message), "Error: Invalid starting location index %d.", start_index);
        return res;
    }

    /* 1. Use Nearest Neighbor to establish a valid initial upper bound */
    RouteResult nn_init = solve_tsp_nearest_neighbor(net, start_index);
    double best_cost = INF;
    int best_path[MAX_LOCATIONS + 1];

    if (nn_init.is_valid && nn_init.total_cost < INF / 2.0) {
        best_cost = nn_init.total_cost;
        memcpy(best_path, nn_init.path, (size_t)(n + 1) * sizeof(int));
    }

    /* 2. Initialize Root Node of the State Space Tree */
    BBNode *root = (BBNode*)malloc(sizeof(BBNode));
    if (!root) {
        snprintf(res.message, sizeof(res.message), "Error: Out of memory allocating root node.");
        return res;
    }

    root->level = 0;
    root->vertex = start_index;
    memset(root->visited, 0, sizeof(root->visited));
    root->visited[start_index] = 1;
    root->path[0] = start_index;

    /* Copy initial cost matrix */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            root->matrix[i][j] = net->cost_matrix[i][j];
        }
    }

    /* Compute initial lower bound via matrix reduction */
    root->bound = reduce_matrix(root->matrix, n);

    MinHeap *heap = create_heap(INITIAL_HEAP_CAPACITY);
    if (!heap) {
        free(root);
        snprintf(res.message, sizeof(res.message), "Error: Out of memory creating priority queue.");
        return res;
    }

    if (!heap_push(heap, root)) {
        free(root);
        free_heap(heap);
        snprintf(res.message, sizeof(res.message), "Error: Failed to push root node to priority queue.");
        return res;
    }

    long long nodes_explored = 0;
    int oom_occurred = 0;

    /* 3. Explore State Space Tree using Least-Cost Branch and Bound (LCBB) */
    while (heap->size > 0) {
        BBNode *current = heap_pop(heap);
        if (!current) break;

        nodes_explored++;

        /* PRUNING: If lower bound of current node exceeds best known cost, prune it */
        if (current->bound >= best_cost) {
            free(current);
            continue;
        }

        /* Check if we have formed a complete path of n locations */
        if (current->level == n - 1) {
            int last_v = current->vertex;
            double return_edge = net->cost_matrix[last_v][start_index];

            if (return_edge < INF / 2.0) {
                current->path[n] = start_index;
                double actual_tour_cost = calculate_route_cost(net, current->path, n + 1);

                if (actual_tour_cost < best_cost) {
                    best_cost = actual_tour_cost;
                    memcpy(best_path, current->path, (size_t)(n + 1) * sizeof(int));
                    /* Compact heap by freeing newly suboptimal nodes */
                    heap_prune(heap, best_cost);
                }
            }
            free(current);
            continue;
        }

        /* Branching: Generate child nodes for each unvisited destination */
        for (int next_v = 0; next_v < n; next_v++) {
            if (!current->visited[next_v] && current->matrix[current->vertex][next_v] < INF / 2.0) {
                BBNode *child = create_child_node(current, next_v, start_index, n);
                if (child) {
                    /* Prune immediately if lower bound is not promising */
                    if (child->bound < best_cost) {
                        if (!heap_push(heap, child)) {
                            free(child);
                            oom_occurred = 1;
                            break;
                        }
                    } else {
                        free(child);
                    }
                }
            }
        }

        free(current);
        if (oom_occurred) break;
    }

    free_heap(heap);

    clock_t end_clock = clock();
    res.execution_time_ms = ((double)(end_clock - start_clock) * 1000.0) / CLOCKS_PER_SEC;
    res.nodes_explored = nodes_explored;

    if (oom_occurred) {
        res.is_valid = 0;
        res.total_cost = INF;
        snprintf(res.message, sizeof(res.message),
                 "Error: Memory capacity exhausted during state space tree exploration.");
        return res;
    }

    if (best_cost < INF / 2.0) {
        res.is_valid = 1;
        res.total_cost = best_cost;
        memcpy(res.path, best_path, (size_t)(n + 1) * sizeof(int));
        snprintf(res.message, sizeof(res.message),
                 "Exact global optimal delivery route found using Branch and Bound.");
    } else {
        res.is_valid = 0;
        res.total_cost = INF;
        snprintf(res.message, sizeof(res.message),
                 "No valid Hamiltonian delivery route exists for this network topology.");
    }

    return res;
}
