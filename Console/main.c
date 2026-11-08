#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../Common/route.h"
#include "../Common/nearest_neighbor.h"
#include "../Common/branch_bound.h"

/* Pauses and waits for user to press Enter before returning to menu */
static void pause_for_user(void) {
    printf("\nPress [Enter] to return to the main menu...");
    char buf[128];
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        /* EOF or error, continue */
    }
}

/* Reads a trimmed line of text from standard input */
static void read_line(char *buffer, size_t max_len) {
    if (fgets(buffer, (int)max_len, stdin)) {
        size_t len = strlen(buffer);
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
            buffer[--len] = '\0';
        }
    } else {
        buffer[0] = '\0';
    }
}

/* Reads an integer within [min_val, max_val] with robust validation */
static int read_int_input(const char *prompt, int min_val, int max_val) {
    char line[128];
    while (1) {
        printf("%s", prompt);
        read_line(line, sizeof(line));

        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '\0') {
            printf("  [!] Input cannot be empty. Please enter an integer between %d and %d.\n", min_val, max_val);
            continue;
        }

        char *endptr;
        long val = strtol(p, &endptr, 10);
        while (*endptr == ' ' || *endptr == '\t') endptr++;

        if (*endptr != '\0') {
            printf("  [!] Invalid input '%s'. Please enter a valid integer between %d and %d.\n", p, min_val, max_val);
            continue;
        }

        if (val < min_val || val > max_val) {
            printf("  [!] Value %ld is out of range. Allowed range is [%d to %d].\n", val, min_val, max_val);
            continue;
        }

        return (int)val;
    }
}

/* Reads a single travel cost (>= 0 or -1 for INF) with comprehensive validation */
static double read_cost_input(const char *prompt) {
    char line[128];
    while (1) {
        printf("%s", prompt);
        read_line(line, sizeof(line));

        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '\0') {
            printf("  [!] Input cannot be empty. Enter a non-negative number (>= 0) or -1 for no direct road.\n");
            continue;
        }

        char *endptr;
        double val = strtod(p, &endptr);
        while (*endptr == ' ' || *endptr == '\t') endptr++;

        if (*endptr != '\0') {
            printf("  [!] Invalid numeric format '%s'. Please enter a number (e.g. 15, 22.5) or -1.\n", p);
            continue;
        }

        if (val < -1.0) {
            printf("  [!] Negative travel costs are not allowed (entered: %.2f). Enter >= 0, or -1 for no road.\n", val);
            continue;
        }

        if (val == -1.0) {
            return INF; /* Mapped internally to infinity representing no direct road */
        }

        return val;
    }
}

/* Displays the current cost matrix in a clear grid format */
static void display_matrix(const DeliveryNetwork *net) {
    printf("\n================================================================================\n");
    printf("                  DIRECTED TRAVEL COST MATRIX (ASYMMETRIC TSP)                  \n");
    printf("================================================================================\n");
    printf("Number of Delivery Locations: %d\n\n", net->num_locations);

    printf("%-4s %-24s", "ID", "Location Name");
    for (int j = 0; j < net->num_locations; j++) {
        printf(" [%2d]", j);
    }
    printf("\n");

    printf("---- ------------------------");
    for (int j = 0; j < net->num_locations; j++) {
        printf(" -----");
    }
    printf("\n");

    for (int i = 0; i < net->num_locations; i++) {
        printf("[%2d] %-24.24s", i, net->names[i]);
        for (int j = 0; j < net->num_locations; j++) {
            if (i == j) {
                printf("     -"); /* Self travel disabled */
            } else if (net->cost_matrix[i][j] >= INF / 2.0) {
                printf("   INF"); /* No direct connection */
            } else {
                printf(" %5.1f", net->cost_matrix[i][j]);
            }
        }
        printf("\n");
    }
    printf("================================================================================\n");
    printf("Legend: '-' = Self-loop disabled | 'INF' = No direct road | Units = Travel Cost / Distance / Time\n");
    printf("Note  : Opposite costs may differ (C[i][j] != C[j][i]) due to one-way streets/traffic.\n");
}

/* Interactively enters a custom delivery network */
static void input_custom_network(DeliveryNetwork *net, int *start_index) {
    printf("\n================================================================================\n");
    printf("              1. ENTER CUSTOM DELIVERY LOCATIONS AND COSTS                      \n");
    printf("================================================================================\n");
    printf("Instructions:\n");
    printf(" - Enter the number of locations in your delivery circuit (between 2 and %d).\n", MAX_LOCATIONS);
    printf(" - Provide a descriptive name for each location (or press Enter for default).\n");
    printf(" - Enter the directed travel costs one by one when prompted.\n");
    printf("================================================================================\n\n");

    int n = read_int_input("Enter number of delivery locations (2 to 20): ", 2, MAX_LOCATIONS);
    init_network(net, n);

    printf("\n--- Step 1: Enter Delivery Location Names ---\n");
    for (int i = 0; i < n; i++) {
        char prompt[128];
        snprintf(prompt, sizeof(prompt), "Location [%d] Name (Press [Enter] for 'Location %d'): ", i, i + 1);
        printf("%s", prompt);

        char name[MAX_NAME_LEN];
        read_line(name, sizeof(name));

        char *p = name;
        while (*p == ' ' || *p == '\t') p++;

        if (*p != '\0') {
            set_location_name(net, i, p);
        } else {
            char default_name[32];
            snprintf(default_name, sizeof(default_name), "Location %d", i + 1);
            set_location_name(net, i, default_name);
        }
    }

    printf("\n================================================================================\n");
    printf("--- Step 2: Enter Directed Travel Costs Between Location Pairs ---\n");
    printf("================================================================================\n");
    printf("Rules for Cost Entry:\n");
    printf(" * Enter ONE number per prompt and press [Enter].\n");
    printf(" * Enter a non-negative number (e.g., 12 or 24.5) for distance, fuel, or time.\n");
    printf(" * Enter -1 if there is NO DIRECT ROAD between the two locations.\n");
    printf(" * Self-travel (from a location to itself) is AUTOMATICALLY DISABLED.\n");
    printf(" * Since this is a directed network, travel from A -> B may differ from B -> A.\n\n");
    printf("EXAMPLE PROMPT:\n");
    printf("  Travel cost from [0] Central Warehouse to [1] Downtown Hub: 15.5\n");
    printf("  Travel cost from [0] Central Warehouse to [2] Airport Cargo: -1   (No road)\n");
    printf("================================================================================\n\n");

    for (int i = 0; i < n; i++) {
        printf("--- Outgoing routes from [%d] %s ---\n", i, net->names[i]);
        for (int j = 0; j < n; j++) {
            if (i == j) {
                net->cost_matrix[i][j] = INF;
                continue; /* Self-travel disabled */
            }

            char prompt[256];
            snprintf(prompt, sizeof(prompt), "  Travel cost from [%d] %s to [%d] %s: ",
                     i, net->names[i], j, net->names[j]);
            double cost = read_cost_input(prompt);
            net->cost_matrix[i][j] = cost;
        }
        printf("\n");
    }

    *start_index = 0;

    printf("\n--- Step 3: Review Completed Cost Matrix ---\n");
    display_matrix(net);

    char err[256];
    if (validate_network(net, err, sizeof(err))) {
        printf("\n[SUCCESS] Delivery network successfully configured and verified!\n");
    } else {
        printf("\n[NOTICE] %s\n", err);
    }

    pause_for_user();
}

/* Loads a predefined sample dataset */
static void load_sample_menu(DeliveryNetwork *net, int *start_index) {
    printf("\n================================================================================\n");
    printf("                   2. LOAD PREDEFINED SAMPLE DATASET                            \n");
    printf("================================================================================\n");
    printf("Select one of the benchmark e-commerce delivery networks:\n\n");
    printf("  [1] Sample 1: 4 Locations (Urban Delivery Circuit - Asymmetric Streets)\n");
    printf("  [2] Sample 2: 5 Locations (Regional Fulfillment Centers - Classic ATSP Benchmark)\n");
    printf("  [3] Sample 3: 6 Locations (Metro Express Distribution Network)\n");
    printf("  [4] Sample 4: 4 Locations (Disconnected Graph - Edge Case / Unreachable Test)\n");
    printf("================================================================================\n");

    int choice = read_int_input("Select dataset [1-4]: ", 1, 4);
    load_sample_network(net, choice);
    *start_index = 0;

    printf("\n[SUCCESS] Loaded: %s\n", get_sample_name(choice));
    display_matrix(net);

    pause_for_user();
}

/* Selects the starting delivery location */
static void select_start_menu(const DeliveryNetwork *net, int *start_index) {
    printf("\n================================================================================\n");
    printf("                      4. SELECT STARTING LOCATION                               \n");
    printf("================================================================================\n");
    printf("Current Starting Location: [%d] %s\n\n", *start_index, net->names[*start_index]);
    printf("Available Locations:\n");
    for (int i = 0; i < net->num_locations; i++) {
        printf("  [%d] %s%s\n", i, net->names[i], (i == *start_index) ? "  <-- Current Start" : "");
    }
    printf("================================================================================\n");

    int choice = read_int_input("Select new starting location index: ", 0, net->num_locations - 1);
    *start_index = choice;
    printf("\n[SUCCESS] Starting location updated to: [%d] %s\n", *start_index, net->names[*start_index]);

    pause_for_user();
}

/* Executes and displays Branch and Bound */
static void run_branch_bound_menu(const DeliveryNetwork *net, int start_index) {
    printf("\n================================================================================\n");
    printf("                5. RUN BRANCH AND BOUND (EXACT OPTIMAL ROUTE)                   \n");
    printf("================================================================================\n");
    printf("Computing exact optimal delivery route starting from [%d] %s...\n\n",
           start_index, net->names[start_index]);

    RouteResult res = solve_tsp_branch_bound(net, start_index);

    if (!res.is_valid) {
        printf("STATUS: NO VALID ROUTE FOUND\n");
        printf("Reason: %s\n", res.message);
        printf("================================================================================\n");
        pause_for_user();
        return;
    }

    char route_str[1024];
    format_route_string(net, &res, route_str, sizeof(route_str));

    printf("================================================================================\n");
    printf("                     BRANCH AND BOUND EXECUTION RESULTS                         \n");
    printf("================================================================================\n");
    printf("Delivery Route Sequence:\n  %s\n\n", route_str);
    printf("Total Travel Cost : %.2f units\n", res.total_cost);
    printf("Solution Quality  : OPTIMAL (Guaranteed minimum-cost Hamiltonian cycle)\n");
    printf("Locations Visited : %d unique locations (Every location visited exactly once)\n", res.num_locations);
    printf("Return to Depot   : Successfully returns to start location '[%d] %s'\n", start_index, net->names[start_index]);
    printf("State Tree Nodes  : %lld nodes explored in state space tree\n", res.nodes_explored);
    printf("Execution Time    : %.4f ms\n", res.execution_time_ms);
    printf("Algorithm Method  : Least-Cost Branch and Bound (LCBB) with Matrix Reduction\n");
    printf("Status Details    : %s\n", res.message);
    printf("================================================================================\n");

    pause_for_user();
}

/* Executes and displays Nearest Neighbor */
static void run_nearest_neighbor_menu(const DeliveryNetwork *net, int start_index) {
    printf("\n================================================================================\n");
    printf("              6. RUN NEAREST NEIGHBOR (APPROXIMATE HEURISTIC ROUTE)             \n");
    printf("================================================================================\n");
    printf("Computing approximate delivery route starting from [%d] %s...\n\n",
           start_index, net->names[start_index]);

    RouteResult res = solve_tsp_nearest_neighbor(net, start_index);

    if (!res.is_valid) {
        printf("STATUS: NO COMPLETE ROUTE FOUND\n");
        printf("Reason: %s\n", res.message);
        printf("================================================================================\n");
        pause_for_user();
        return;
    }

    char route_str[1024];
    format_route_string(net, &res, route_str, sizeof(route_str));

    printf("================================================================================\n");
    printf("                    NEAREST NEIGHBOR EXECUTION RESULTS                          \n");
    printf("================================================================================\n");
    printf("Delivery Route Sequence:\n  %s\n\n", route_str);
    printf("Total Travel Cost : %.2f units\n", res.total_cost);
    printf("Solution Quality  : APPROXIMATE (Greedy heuristic, not guaranteed optimal)\n");
    printf("Locations Visited : %d unique locations (Every location visited exactly once)\n", res.num_locations);
    printf("Return to Depot   : Successfully returns to start location '[%d] %s'\n", start_index, net->names[start_index]);
    printf("Decision Steps    : %lld candidate neighbor evaluations\n", res.nodes_explored);
    printf("Execution Time    : %.4f ms\n", res.execution_time_ms);
    printf("Algorithm Method  : Greedy choice of nearest unvisited vertex at each step\n");
    printf("Status Details    : %s\n", res.message);
    printf("================================================================================\n");

    pause_for_user();
}

/* Compares both algorithms side by side */
static void compare_algorithms_menu(const DeliveryNetwork *net, int start_index) {
    printf("\n================================================================================\n");
    printf("                       7. COMPARE BOTH ALGORITHMS                               \n");
    printf("================================================================================\n");
    printf("Executing Branch and Bound (Exact Optimal)...");
    fflush(stdout);
    RouteResult bb_res = solve_tsp_branch_bound(net, start_index);
    printf(" Done.\n");

    printf("Executing Nearest Neighbor (Heuristic)...");
    fflush(stdout);
    RouteResult nn_res = solve_tsp_nearest_neighbor(net, start_index);
    printf(" Done.\n\n");

    char bb_route[1024], nn_route[1024];
    format_route_string(net, &bb_res, bb_route, sizeof(bb_route));
    format_route_string(net, &nn_res, nn_route, sizeof(nn_route));

    printf("====================================================================================================\n");
    printf("                   DAA ALGORITHM COMPARISON: BRANCH & BOUND vs NEAREST NEIGHBOR                     \n");
    printf("====================================================================================================\n");
    printf("Starting Location : [%d] %s | Total Locations: %d\n\n", start_index, net->names[start_index], net->num_locations);

    printf("%-26s | %-32s | %-32s\n", "FEATURE / METRIC", "BRANCH AND BOUND (EXACT)", "NEAREST NEIGHBOR (HEURISTIC)");
    printf("---------------------------+----------------------------------+---------------------------------\n");
    printf("%-26s | %-32s | %-32s\n", "Algorithm Class", "State Space Search (LCBB)", "Greedy Local Search");
    printf("%-26s | %-32s | %-32s\n", "Optimality Guarantee", "Guaranteed Exact Global Minimum", "Approximate (Suboptimal)");
    printf("%-26s | %-32s | %-32s\n", "Time Complexity", "O(N^2 * 2^N) avg / O(N!*N^2) max", "O(N^2) deterministic");
    printf("%-26s | %-32s | %-32s\n", "Space Complexity", "O(M * N^2) priority queue", "O(N) visited array");

    if (bb_res.is_valid) {
        printf("%-26s | %-26.2f units | ", "Total Route Cost", bb_res.total_cost);
    } else {
        printf("%-26s | %-32s | ", "Total Route Cost", "No valid route");
    }

    if (nn_res.is_valid) {
        printf("%-26.2f units\n", nn_res.total_cost);
    } else {
        printf("%-32s\n", "No valid route");
    }

    printf("%-26s | %-29.4f ms | %-29.4f ms\n", "Execution Time", bb_res.execution_time_ms, nn_res.execution_time_ms);
    printf("%-26s | %-26lld nodes | %-26lld steps\n", "Nodes / Steps Evaluated", bb_res.nodes_explored, nn_res.nodes_explored);

    printf("---------------------------+----------------------------------+---------------------------------\n");
    printf("DELIVERY ROUTES:\n");
    printf("  Optimal Route (B&B)    :\n    %s\n", bb_route);
    printf("  Approximate Route (NN) :\n    %s\n", nn_route);
    printf("----------------------------------------------------------------------------------------------------\n");

    /* Quantitative Cost Difference and Optimality Gap */
    if (bb_res.is_valid && nn_res.is_valid) {
        double cost_diff = nn_res.total_cost - bb_res.total_cost;
        double gap_percent = (bb_res.total_cost > 0.0) ? (cost_diff / bb_res.total_cost) * 100.0 : 0.0;

        printf("\n--- QUANTITATIVE DAA ANALYSIS ---\n");
        printf("1. Absolute Cost Difference : %.2f units\n", cost_diff);
        if (cost_diff <= 0.00001) {
            printf("2. Optimality Gap           : 0.00%% (Nearest Neighbor happened to find the exact optimal route!)\n");
        } else {
            printf("2. Optimality Gap           : +%.2f%% cost increase using Nearest Neighbor.\n", gap_percent);
        }
        printf("3. Practical Trade-off      : Nearest Neighbor ran in %.4f ms (Greedy O(N^2)), but\n"
               "                              produced a route with higher travel expenditure.\n"
               "                              Branch and Bound required %.4f ms, searching %lld nodes\n"
               "                              to mathematically prove and deliver the lowest possible cost.\n",
               nn_res.execution_time_ms, bb_res.execution_time_ms, bb_res.nodes_explored);
    } else if (!bb_res.is_valid && !nn_res.is_valid) {
        printf("\nBoth algorithms verified that no complete Hamiltonian cycle exists in this network.\n");
    } else if (bb_res.is_valid && !nn_res.is_valid) {
        printf("\nKey Observation: Nearest Neighbor reached a dead end and failed to complete a tour,\n"
               "while Branch and Bound found the exact optimal route of cost %.2f!\n", bb_res.total_cost);
    }
    printf("====================================================================================================\n");

    pause_for_user();
}

int main(void) {
    DeliveryNetwork current_net;
    int start_index = 0;

    /* Load Sample 1 by default on startup */
    load_sample_network(&current_net, 1);

    while (1) {
        printf("================================================================================\n");
        printf("          OPTIMAL DELIVERY ROUTE PLANNING FOR E-COMMERCE APPLICATIONS           \n");
        printf("                      Design and Analysis of Algorithms (DAA)                   \n");
        printf("================================================================================\n");
        printf(" Active Network : %d Locations | Starting Point: [%d] %s\n",
               current_net.num_locations, start_index, current_net.names[start_index]);
        printf("================================================================================\n");
        printf("  1. Enter Custom Delivery Locations and Costs\n");
        printf("  2. Load Predefined Sample Dataset\n");
        printf("  3. Display Current Cost Matrix\n");
        printf("  4. Select Starting Location\n");
        printf("  5. Run Branch and Bound\n");
        printf("  6. Run Nearest Neighbor\n");
        printf("  7. Compare Both Algorithms\n");
        printf("  8. Exit\n");
        printf("================================================================================\n");

        int choice = read_int_input("Enter your choice [1-8]: ", 1, 8);

        if (choice == 8) {
            printf("\n================================================================================\n");
            printf("Exiting Optimal Delivery Route Planning. Thank you!\n");
            printf("================================================================================\n");
            break;
        }

        switch (choice) {
            case 1:
                input_custom_network(&current_net, &start_index);
                break;
            case 2:
                load_sample_menu(&current_net, &start_index);
                break;
            case 3:
                display_matrix(&current_net);
                pause_for_user();
                break;
            case 4:
                select_start_menu(&current_net, &start_index);
                break;
            case 5:
                run_branch_bound_menu(&current_net, start_index);
                break;
            case 6:
                run_nearest_neighbor_menu(&current_net, start_index);
                break;
            case 7:
                compare_algorithms_menu(&current_net, start_index);
                break;
            default:
                break;
        }
    }

    return 0;
}
