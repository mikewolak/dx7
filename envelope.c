#include "dx7.h"

// External global sample rate
extern int g_sample_rate;

// DX7 envelope rate table (approximate timings in seconds for full scale)
static const double rate_table[100] = {
    // Rates 0-9: Very slow
    30.0, 25.0, 20.0, 18.0, 16.0, 14.0, 12.0, 10.0, 8.0, 6.0,
    // Rates 10-19: Slow
    5.5, 5.0, 4.5, 4.0, 3.5, 3.0, 2.8, 2.6, 2.4, 2.2,
    // Rates 20-29: Medium-slow
    2.0, 1.8, 1.6, 1.4, 1.2, 1.0, 0.95, 0.90, 0.85, 0.80,
    // Rates 30-39: Medium
    0.75, 0.70, 0.65, 0.60, 0.55, 0.50, 0.47, 0.44, 0.41, 0.38,
    // Rates 40-49: Medium-fast
    0.35, 0.32, 0.29, 0.26, 0.23, 0.20, 0.19, 0.18, 0.17, 0.16,
    // Rates 50-59: Fast
    0.15, 0.14, 0.13, 0.12, 0.11, 0.10, 0.095, 0.090, 0.085, 0.080,
    // Rates 60-69: Very fast
    0.075, 0.070, 0.065, 0.060, 0.055, 0.050, 0.047, 0.044, 0.041, 0.038,
    // Rates 70-79: Extremely fast
    0.035, 0.032, 0.029, 0.026, 0.023, 0.020, 0.018, 0.016, 0.014, 0.012,
    // Rates 80-89: Lightning fast
    0.010, 0.009, 0.008, 0.007, 0.006, 0.005, 0.0045, 0.004, 0.0035, 0.003,
    // Rates 90-99: Instant
    0.0025, 0.002, 0.0018, 0.0016, 0.0014, 0.0012, 0.001, 0.0008, 0.0006, 0.0004
};

double dx7_envelope_rate_to_time(int rate, int level_diff) {
    if (rate == 0) return 30.0; // Maximum time
    if (rate >= 99) return 0.0004; // Minimum time
    
    double base_time = rate_table[rate];
    
    // Scale by level difference (larger jumps take longer)
    double scale = (double)abs(level_diff) / 99.0;
    if (scale < 0.1) scale = 0.1; // Minimum scaling
    
    return base_time * scale;
}

void init_envelope(envelope_state_t* env, const dx7_operator_t* op, double rate_scale) {
    env->stage = ENV_ATTACK;
    env->level = 0.0;
    env->samples_in_stage = 0;
    
    // Calculate attack rate (scaled by keyboard rate scaling)
    double attack_time = dx7_envelope_rate_to_time(op->env_rates[ENV_ATTACK], op->env_levels[ENV_ATTACK]);
    attack_time /= (1.0 + rate_scale * (op->key_rate_scaling / 7.0));
    
    if (attack_time > 0.0) {
        env->rate = (double)op->env_levels[ENV_ATTACK] / (99.0 * attack_time * g_sample_rate);
    } else {
        env->rate = 99.0; // Instant attack
    }
    
    env->target = (double)op->env_levels[ENV_ATTACK] / 99.0;
}

double update_envelope(envelope_state_t* env, const dx7_operator_t* op, double rate_scale) {
    env->samples_in_stage++;
    
    switch (env->stage) {
        case ENV_ATTACK:
            if (env->level >= env->target || op->env_rates[ENV_ATTACK] >= 99) {
                // Move to Decay 1
                env->stage = ENV_DECAY1;
                env->level = env->target;
                env->samples_in_stage = 0;
                
                // Calculate Decay 1 rate
                int level_diff = op->env_levels[ENV_ATTACK] - op->env_levels[ENV_DECAY1];
                double decay1_time = dx7_envelope_rate_to_time(op->env_rates[ENV_DECAY1], level_diff);
                decay1_time /= (1.0 + rate_scale * (op->key_rate_scaling / 7.0));
                
                if (decay1_time > 0.0 && level_diff != 0) {
                    env->rate = -(double)level_diff / (99.0 * decay1_time * g_sample_rate);
                } else {
                    env->rate = 0.0;
                }
                
                env->target = (double)op->env_levels[ENV_DECAY1] / 99.0;
            } else {
                env->level += env->rate;
                if (env->level > env->target) env->level = env->target;
            }
            break;
            
        case ENV_DECAY1:
            if (env->level <= env->target || op->env_rates[ENV_DECAY1] >= 99) {
                // Move to Decay 2 (Sustain)
                env->stage = ENV_DECAY2;
                env->level = env->target;
                env->samples_in_stage = 0;
                
                // Calculate Decay 2 rate
                int level_diff = op->env_levels[ENV_DECAY1] - op->env_levels[ENV_DECAY2];
                double decay2_time = dx7_envelope_rate_to_time(op->env_rates[ENV_DECAY2], level_diff);
                decay2_time /= (1.0 + rate_scale * (op->key_rate_scaling / 7.0));
                
                if (decay2_time > 0.0 && level_diff != 0) {
                    env->rate = -(double)level_diff / (99.0 * decay2_time * g_sample_rate);
                } else {
                    env->rate = 0.0;
                }
                
                env->target = (double)op->env_levels[ENV_DECAY2] / 99.0;
            } else {
                env->level += env->rate;
                if (env->level < env->target) env->level = env->target;
            }
            break;
            
        case ENV_DECAY2:
            // Sustain phase - continue decaying slowly toward target
            if (env->level > env->target) {
                env->level += env->rate;
                if (env->level < env->target) env->level = env->target;
            }
            break;
            
        case ENV_RELEASE:
            env->level += env->rate;
            if (env->level < 0.0) env->level = 0.0;
            break;
    }
    
    return env->level;
}

void trigger_release(envelope_state_t* env, const dx7_operator_t* op, double rate_scale) {
    env->stage = ENV_RELEASE;
    env->samples_in_stage = 0;
    
    // Calculate release rate
    int level_diff = (int)(env->level * 99.0) - op->env_levels[ENV_RELEASE];
    double release_time = dx7_envelope_rate_to_time(op->env_rates[ENV_RELEASE], level_diff);
    release_time /= (1.0 + rate_scale * (op->key_rate_scaling / 7.0));
    
    if (release_time > 0.0 && level_diff != 0) {
        env->rate = -(double)level_diff / (99.0 * release_time * g_sample_rate);
    } else {
        env->rate = -0.1; // Default fast release
    }
    
    env->target = (double)op->env_levels[ENV_RELEASE] / 99.0;
}
