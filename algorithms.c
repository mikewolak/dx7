#include "dx7.h"

#define TWO_PI (2.0 * M_PI)

// Algorithm structures - simplified representation of DX7's 32 algorithms
// For each algorithm, we define which operators are carriers and how modulation flows
typedef struct {
    int carriers[MAX_OPERATORS];    // Which operators contribute to final output
    int num_carriers;               // Number of carrier operators
    int modulation_matrix[MAX_OPERATORS][MAX_OPERATORS]; // [modulator][carrier] = strength
} algorithm_def_t;

static algorithm_def_t algorithms[33] = { // Index 0 unused, algorithms 1-32
    {{0}, 0, {{0}}}, // Placeholder for index 0
    
    // Algorithm 1: 6→5→4→3→2→1 (Classic FM chain)
    {{1}, 1, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,1,0,0,0}, {0,0,0,1,0,0}, {0,0,0,0,1,0}}},
    
    // Algorithm 2: 6→5→4→3→2, 1 (Two separate chains)
    {{1,2}, 2, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,1,0,0,0}, {0,0,0,1,0,0}, {0,0,0,0,1,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 3: 6→5→4→3, 2→1 (Two separate chains)
    {{1,3}, 2, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,1,0,0}, {0,0,0,0,1,0}}},
    
    // Algorithm 4: 6→5→4, 3→2→1 (E.PIANO algorithm - two chains)
    {{1,4}, 2, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,1,0}}},
    
    // Algorithm 5: 6→5, 4→3→2→1 (Mixed)
    {{1,5}, 2, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,1,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 6: 6→5, 4→3→2, 1 (Three separate outputs)
    {{1,2,5}, 3, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,1,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 7: 6→5, 4→3, 2→1 (Three chains)
    {{1,3,5}, 3, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 8: 6→5, 4→3, 2, 1 (Four outputs)
    {{1,2,3,5}, 4, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 9: 6→5, 4, 3→2→1 (Mixed)
    {{1,4,5}, 3, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 10: 6→5, 4, 3→2, 1 (Four outputs)
    {{1,2,4,5}, 4, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 11: 6→5, 4, 3, 2→1 (Four outputs)
    {{1,3,4,5}, 4, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 12: 6→5, 4, 3, 2, 1 (Five separate outputs)
    {{1,2,3,4,5}, 5, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 13: 6, 5→4→3→2→1 (One modulator chain)
    {{1,6}, 2, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,1,0,0,0}, {0,0,0,1,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 14: 6, 5→4→3→2, 1 (Two separate outputs)
    {{1,2,6}, 3, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,1,0,0,0}, {0,0,0,1,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 15: 6, 5→4→3, 2→1 (Three outputs)
    {{1,3,6}, 3, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,1,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 16: 6, 5→4, 3→2→1 (Three outputs)
    {{1,4,6}, 3, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 17: 6, 5→4, 3→2, 1 (Four outputs)
    {{1,2,4,6}, 4, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 18: 6, 5→4, 3, 2→1 (Four outputs)
    {{1,3,4,6}, 4, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 19: 6, 5, 4→3→2→1 (Three outputs with one chain)
    {{1,5,6}, 3, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,1,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 20: 6, 5, 4→3→2, 1 (Four outputs)
    {{1,2,5,6}, 4, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,1,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 21: 6, 5, 4→3, 2→1 (Four outputs)
    {{1,3,5,6}, 4, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 22: 6, 5, 4, 3→2→1 (Four outputs)
    {{1,4,5,6}, 4, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 23: 6, 5, 4, 3→2, 1 (Five outputs)
    {{1,2,4,5,6}, 5, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 24: 6, 5, 4, 3, 2→1 (Five outputs)
    {{1,3,4,5,6}, 5, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 25: 6, 5, 4, 3, 2, 1 (All separate - additive synthesis)
    {{1,2,3,4,5,6}, 6, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}},
    
    // Algorithm 26: (6+5)→4→3→2→1 (Parallel modulators)
    {{1}, 1, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,1,0,0,0}, {0,0,0,1,0,0}, {0,0,0,1,0,0}}},
    
    // Algorithm 27: (6+5)→4→3→2, 1 (Parallel modulators to chain)
    {{1,2}, 2, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,1,0,0,0}, {0,0,0,1,0,0}, {0,0,0,1,0,0}}},
    
    // Algorithm 28: (6+5)→4→3, 2→1 (Mixed parallel)
    {{1,3}, 2, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,1,0,0}, {0,0,0,1,0,0}, {0,0,0,1,0,0}}},
    
    // Algorithm 29: (6+5)→4, 3→2→1 (Parallel to separate chains)
    {{1,4}, 2, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,1,0}, {0,0,0,0,1,0}}},
    
    // Algorithm 30: (6+5)→4, 3→2, 1 (Multiple outputs)
    {{1,2,4}, 3, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,1,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,1,0,0}, {0,0,0,1,0,0}}},
    
    // Algorithm 31: (6+5)→4, 3, 2→1 (Multiple outputs)
    {{1,3,4}, 3, {{0,0,0,0,0,0}, {1,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,1,0,0}, {0,0,0,1,0,0}}},
    
    // Algorithm 32: (6+5)→(4+3+2+1) (All modulated by parallel pair)
    {{1,2,3,4}, 4, {{0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {1,1,1,1,0,0}, {1,1,1,1,0,0}}}
};

double apply_fm_modulation(double carrier_freq, double modulator_output, double mod_index) {
    return sin(TWO_PI * carrier_freq + modulator_output * mod_index);
}

double process_algorithm(const double* op_outputs, const double* op_levels, int algorithm, double feedback_val) {
    if (algorithm < 1 || algorithm > 32) {
        algorithm = 1; // Default to algorithm 1
    }
    
    const algorithm_def_t* alg = &algorithms[algorithm];
    double final_output = 0.0;
    double processed_ops[MAX_OPERATORS];
    
    // Copy original outputs and scale by operator levels
    for (int i = 0; i < MAX_OPERATORS; i++) {
        processed_ops[i] = op_outputs[i] * op_levels[i];
    }
    
    // Apply feedback to operator 1 (0-indexed as operator 0)
    if (feedback_val != 0.0) {
        processed_ops[0] = sin(TWO_PI * processed_ops[0] + feedback_val);
    }
    
    // Process modulation matrix
    for (int modulator = 0; modulator < MAX_OPERATORS; modulator++) {
        for (int carrier = 0; carrier < MAX_OPERATORS; carrier++) {
            if (algorithms[algorithm].modulation_matrix[modulator][carrier] > 0) {
                // Apply FM modulation - modulator level affects modulation depth
                double mod_depth = (double)algorithms[algorithm].modulation_matrix[modulator][carrier] * 
                                 op_levels[modulator] * 2.0; // Scale modulation by modulator level
                processed_ops[carrier] = apply_fm_modulation(1.0, processed_ops[modulator], mod_depth);
            }
        }
    }
    
    // Sum carrier outputs (already scaled by their levels)
    for (int i = 0; i < alg->num_carriers; i++) {
        int carrier_idx = alg->carriers[i] - 1; // Convert to 0-indexed
        if (carrier_idx >= 0 && carrier_idx < MAX_OPERATORS) {
            final_output += processed_ops[carrier_idx];
        }
    }
    
    // Normalize by number of carriers to prevent clipping
    if (alg->num_carriers > 0) {
        final_output /= sqrt((double)alg->num_carriers);
    }
    
    return final_output;
}

void get_algorithm_routing(int algorithm, int* carriers, int* num_carriers, 
                          int routing[MAX_OPERATORS][MAX_OPERATORS]) {
    if (algorithm < 1 || algorithm > 32) {
        algorithm = 1;
    }
    
    const algorithm_def_t* alg = &algorithms[algorithm];
    
    *num_carriers = alg->num_carriers;
    for (int i = 0; i < alg->num_carriers; i++) {
        carriers[i] = alg->carriers[i];
    }
    
    for (int i = 0; i < MAX_OPERATORS; i++) {
        for (int j = 0; j < MAX_OPERATORS; j++) {
            routing[i][j] = alg->modulation_matrix[i][j];
        }
    }
}
