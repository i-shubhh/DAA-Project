#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../Common/route.h"
#include "../Common/nearest_neighbor.h"
#include "../Common/branch_bound.h"

static int test_count = 0;
static int pass_count = 0;

static void assert_condition(const char *test_name, int condition, const char *details) {
    test_count++;
    if (condition) {
        pass_count++;
        printf("  [PASS] %-40s | %s\n", test_name, details);
    } else {
        printf("  [FAIL] %-40s | %s\n", test_name, details);
    }
}

int main(void) {
    printf("================================================================================\n");
    printf("             DAA TEST SUITE: ALGORITHM & TOPOLOGY VERIFICATION                  \n");
    printf("================================================================================\n\n");

    /* -------------------------------------------------------------------------
     * TEST 1: Benchmark Predefined Datasets (Samples 1 to 4)
     * ------------------------------------------------------------------------- */
    printf("--- TEST SUITE 1: Predefined Benchmark Datasets ---\n");
    for (int sid = 1; sid <= 4; sid++) {
        DeliveryNetwork net;
        load_sample_network(&net, sid);

        RouteResult bb = solve_tsp_branch_bound(&net, 0);
        RouteResult nn = solve_tsp_nearest_neighbor(&net, 0);

        if (sid == 4) {
            /* Disconnected Graph */
            assert_condition("Sample 4 (Disconnected): B&B detects no route",
                             !bb.is_valid && bb.total_cost >= INF / 2.0,
                             "Correctly identified no Hamiltonian cycle");
            assert_condition("Sample 4 (Disconnected): NN detects no route",
                             !nn.is_valid && nn.total_cost >= INF / 2.0,
                             "Correctly reported unreachable / dead end");
        } else {
            /* Hamiltonian Cycle Verification */
            int bb_hc = verify_hamiltonian_cycle(&net, &bb, 0);
            int nn_hc = verify_hamiltonian_cycle(&net, &nn, 0);

            char desc_bb[128], desc_nn[128];
            snprintf(desc_bb, sizeof(desc_bb), "Valid cycle visiting all %d nodes once, cost=%.2f", net.num_locations, bb.total_cost);
            snprintf(desc_nn, sizeof(desc_nn), "Valid cycle visiting all %d nodes once, cost=%.2f", net.num_locations, nn.total_cost);

            char label_bb[64], label_nn[64];
            snprintf(label_bb, sizeof(label_bb), "Sample %d: B&B Hamiltonian Cycle", sid);
            snprintf(label_nn, sizeof(label_nn), "Sample %d: NN Hamiltonian Cycle", sid);

            assert_condition(label_bb, bb_hc, desc_bb);
            assert_condition(label_nn, nn_hc, desc_nn);

            /* Optimality Check: B&B cost <= NN cost */
            char opt_label[64], opt_desc[128];
            snprintf(opt_label, sizeof(opt_label), "Sample %d: B&B Optimality vs NN", sid);
            snprintf(opt_desc, sizeof(opt_desc), "B&B (%.2f) <= NN (%.2f), Gap: +%.2f units",
                     bb.total_cost, nn.total_cost, nn.total_cost - bb.total_cost);
            assert_condition(opt_label, bb.total_cost <= nn.total_cost + 1e-6, opt_desc);

            /* Brute-Force Verification: B&B cost == Brute Force cost */
            RouteResult bf = solve_tsp_brute_force(&net, 0);
            char bf_label[64], bf_desc[128];
            snprintf(bf_label, sizeof(bf_label), "Sample %d: B&B matches Brute Force", sid);
            snprintf(bf_desc, sizeof(bf_desc), "B&B (%.2f) == Brute Force (%.2f)", bb.total_cost, bf.total_cost);
            assert_condition(bf_label, fabs(bb.total_cost - bf.total_cost) < 1e-6, bf_desc);
        }
    }

    /* -------------------------------------------------------------------------
     * TEST 2: Custom Asymmetric Matrices (Different directions have different costs)
     * ------------------------------------------------------------------------- */
    printf("\n--- TEST SUITE 2: Custom Asymmetric Matrices ---\n");
    DeliveryNetwork asym_net;
    init_network(&asym_net, 4);
    set_location_name(&asym_net, 0, "Depot");
    set_location_name(&asym_net, 1, "Client A");
    set_location_name(&asym_net, 2, "Client B");
    set_location_name(&asym_net, 3, "Client C");

    /* Asymmetric costs: C[0][1] = 10, C[1][0] = 50, etc. */
    double asym_matrix[4][4] = {
        {INF,  10.0, 45.0, 20.0},
        {50.0, INF,  15.0, 35.0},
        {25.0, 40.0, INF,  10.0},
        {15.0, 20.0, 30.0, INF}
    };
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            asym_net.cost_matrix[i][j] = asym_matrix[i][j];
        }
    }

    RouteResult asym_bb = solve_tsp_branch_bound(&asym_net, 0);
    RouteResult asym_bf = solve_tsp_brute_force(&asym_net, 0);
    RouteResult asym_nn = solve_tsp_nearest_neighbor(&asym_net, 0);

    assert_condition("Asymmetric 4x4: B&B Hamiltonian Cycle",
                     verify_hamiltonian_cycle(&asym_net, &asym_bb, 0),
                     "Valid cycle visiting all 4 nodes");
    assert_condition("Asymmetric 4x4: B&B matches Brute Force",
                     fabs(asym_bb.total_cost - asym_bf.total_cost) < 1e-6,
                     "B&B exact cost matches exhaustive permutation search");
    assert_condition("Asymmetric 4x4: B&B optimal vs NN",
                     asym_bb.total_cost <= asym_nn.total_cost + 1e-6,
                     "B&B cost <= NN cost");

    /* -------------------------------------------------------------------------
     * TEST 3: Network Topology Validation & Invalid Inputs
     * ------------------------------------------------------------------------- */
    printf("\n--- TEST SUITE 3: Validation and Error Handling ---\n");
    DeliveryNetwork invalid_net;
    init_network(&invalid_net, 3);
    char err_buf[256];

    /* Test negative cost */
    invalid_net.cost_matrix[0][1] = -5.0;
    invalid_net.cost_matrix[1][2] = 10.0;
    invalid_net.cost_matrix[2][0] = 10.0;
    int val_neg = validate_network(&invalid_net, err_buf, sizeof(err_buf));
    assert_condition("Validation: Negative cost rejected",
                     !val_neg,
                     "Correctly rejected negative travel cost");

    /* Test isolated node with no incoming edges */
    invalid_net.cost_matrix[0][1] = 10.0;
    invalid_net.cost_matrix[0][2] = INF;
    invalid_net.cost_matrix[1][0] = 10.0;
    invalid_net.cost_matrix[1][2] = INF;
    invalid_net.cost_matrix[2][0] = INF;
    invalid_net.cost_matrix[2][1] = INF;
    int val_isolated = validate_network(&invalid_net, err_buf, sizeof(err_buf));
    assert_condition("Validation: Isolated node detected",
                     !val_isolated,
                     "Correctly flagged node with no incoming/outgoing paths");

    /* Test null pointer handling */
    RouteResult null_bb = solve_tsp_branch_bound(NULL, 0);
    assert_condition("Robustness: NULL network handled safely",
                     !null_bb.is_valid,
                     "Cleanly reported error without segmentation fault");

    /* Test start index out of bounds */
    RouteResult oob_bb = solve_tsp_branch_bound(&asym_net, 99);
    assert_condition("Robustness: Invalid start index handled",
                     !oob_bb.is_valid,
                     "Cleanly reported invalid starting location index");

    /* -------------------------------------------------------------------------
     * TEST 4: Starting Location Invariance
     * ------------------------------------------------------------------------- */
    printf("\n--- TEST SUITE 4: Starting Location Flexibility ---\n");
    /* Starting from node 0 vs node 2 on Sample 1 */
    DeliveryNetwork s1;
    load_sample_network(&s1, 1);
    RouteResult s1_start0 = solve_tsp_branch_bound(&s1, 0);
    RouteResult s1_start2 = solve_tsp_branch_bound(&s1, 2);

    assert_condition("Starting Location: Start from Node 0 valid",
                     verify_hamiltonian_cycle(&s1, &s1_start0, 0),
                     "Valid cycle starting and ending at Node 0");
    assert_condition("Starting Location: Start from Node 2 valid",
                     verify_hamiltonian_cycle(&s1, &s1_start2, 2),
                     "Valid cycle starting and ending at Node 2");
    assert_condition("Starting Location: Cycle cost equivalence",
                     fabs(s1_start0.total_cost - s1_start2.total_cost) < 1e-6,
                     "Optimal Hamiltonian cycle cost is invariant under starting node choice");

    printf("\n================================================================================\n");
    printf("TEST RESULTS: %d / %d TESTS PASSED\n", pass_count, test_count);
    if (pass_count == test_count) {
        printf("STATUS: ALL TESTS PASSED SUCCESSFULLY (100%%)\n");
    } else {
        printf("STATUS: FAILURES DETECTED\n");
    }
    printf("================================================================================\n");

    return (pass_count == test_count) ? 0 : 1;
}

