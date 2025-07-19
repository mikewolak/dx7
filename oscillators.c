#include "dx7.h"

#define TWO_PI (2.0 * M_PI)

// External global sample rate
extern int g_sample_rate;

// Convert MIDI note to frequency
double midi_note_to_frequency(int midi_note) {
    return 440.0 * pow(2.0, (midi_note - 69) / 12.0);
}

// Calculate keyboard level scaling
double calculate_key_scaling(int midi_note, int break_point, int left_depth, int right_depth, 
                           int left_curve, int right_curve) {
    double scale = 1.0;
    
    if (midi_note < break_point) {
        // Left side scaling
        double distance = (double)(break_point - midi_note) / 127.0;
        double depth = (double)left_depth / 99.0;
        
        switch (left_curve) {
            case 0: // Linear negative
                scale = 1.0 - (distance * depth);
                break;
            case 1: // Exponential negative
                scale = 1.0 - (depth * (1.0 - exp(-distance * 3.0)));
                break;
            case 2: // Exponential positive
                scale = 1.0 + (depth * (1.0 - exp(-distance * 3.0)));
                break;
            case 3: // Linear positive
                scale = 1.0 + (distance * depth);
                break;
        }
    } else if (midi_note > break_point) {
        // Right side scaling
        double distance = (double)(midi_note - break_point) / 127.0;
        double depth = (double)right_depth / 99.0;
        
        switch (right_curve) {
            case 0: // Linear negative
                scale = 1.0 - (distance * depth);
                break;
            case 1: // Exponential negative
                scale = 1.0 - (depth * (1.0 - exp(-distance * 3.0)));
                break;
            case 2: // Exponential positive
                scale = 1.0 + (depth * (1.0 - exp(-distance * 3.0)));
                break;
            case 3: // Linear positive
                scale = 1.0 + (distance * depth);
                break;
        }
    }
    
    if (scale < 0.0) scale = 0.0;
    if (scale > 2.0) scale = 2.0;
    
    return scale;
}

void init_operators(voice_state_t* voice, const dx7_patch_t* patch, int midi_note, double velocity) {
    voice->note_freq = midi_note_to_frequency(midi_note);
    voice->midi_note = midi_note;
    voice->velocity = velocity;
    voice->samples_played = 0;
    voice->lfo_phase = 0.0;
    
    for (int i = 0; i < MAX_OPERATORS; i++) {
        const dx7_operator_t* op = &patch->operators[i];
        operator_state_t* op_state = &voice->operators[i];
        
        // Initialize phase
        op_state->phase = 0.0;
        
        // Calculate base frequency with ratio and detune
        op_state->freq = voice->note_freq * op->freq_ratio;
        
        // Apply detune (approximately ±100 cents per detune unit)
        double detune_factor = pow(2.0, (op->detune / 7.0) * 0.01); // About 1% per detune unit
        op_state->freq *= detune_factor;
        
        // Calculate keyboard level scaling
        op_state->level_scale = calculate_key_scaling(
            midi_note,
            op->key_level_scale_break_point,
            op->key_level_scale_left_depth,
            op->key_level_scale_right_depth,
            op->key_level_scale_left_curve,
            op->key_level_scale_right_curve
        );
        
        // Calculate keyboard rate scaling (affects envelope rates)
        double key_distance = (double)(midi_note - 60) / 12.0; // Distance from C4 in octaves
        op_state->rate_scale = key_distance * (op->key_rate_scaling / 7.0);
        
        // Initialize envelope
        init_envelope(&op_state->env, op, op_state->rate_scale);
        
        op_state->output = 0.0;
    }
}

double process_operators(voice_state_t* voice, const dx7_patch_t* patch) {
    double op_outputs[MAX_OPERATORS];
    double op_levels[MAX_OPERATORS];
    
    // Update LFO
    voice->lfo_phase += (double)patch->lfo_speed / 99.0 * 6.0 / g_sample_rate; // Max ~6 Hz
    if (voice->lfo_phase >= 1.0) voice->lfo_phase -= 1.0;
    
    double lfo_value = sin(TWO_PI * voice->lfo_phase);
    
    // Process each operator
    for (int i = 0; i < MAX_OPERATORS; i++) {
        const dx7_operator_t* op = &patch->operators[i];
        operator_state_t* op_state = &voice->operators[i];
        
        // Update envelope
        double env_level = update_envelope(&op_state->env, op, op_state->rate_scale);
        
        // Calculate output level with velocity sensitivity and keyboard scaling
        double vel_factor = 1.0 - (1.0 - voice->velocity) * (op->key_vel_sens / 7.0);
        double total_level = (double)op->output_level / 99.0 * env_level * vel_factor * op_state->level_scale;
        
        // Apply LFO amplitude modulation
        double lfo_amp_mod = 1.0 + (lfo_value * (double)patch->lfo_amd / 99.0 * 0.5);
        total_level *= lfo_amp_mod;
        
        op_levels[i] = total_level;
        
        // Generate sine wave (raw output without level scaling)
        op_outputs[i] = sin(TWO_PI * op_state->phase);
        
        // Update phase
        double freq_with_lfo = op_state->freq;
        
        // Apply LFO pitch modulation
        if (patch->lfo_pmd > 0) {
            double pitch_mod = lfo_value * (double)patch->lfo_pmd / 99.0 * (patch->lfo_pitch_mod_sens / 7.0) * 0.1;
            freq_with_lfo *= pow(2.0, pitch_mod);
        }
        
        op_state->phase += freq_with_lfo / g_sample_rate;
        if (op_state->phase >= 1.0) op_state->phase -= 1.0;
        
        op_state->output = op_outputs[i] * total_level; // Store scaled output for feedback
    }
    
    // Process algorithm routing and get final output
    double feedback_value = voice->operators[0].output * (double)patch->feedback / 7.0 * 0.1;
    double final_output = process_algorithm(op_outputs, op_levels, patch->algorithm, feedback_value);
    
    voice->samples_played++;
    
    return final_output;
}
