#ifndef DX7_H
#define DX7_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sndfile.h>

#define MAX_OPERATORS 6
// Default sample rate (can be overridden)
extern int g_sample_rate;
#define ENVELOPE_STAGES 4
#define MAX_ALGORITHMS 32
#define MAX_PATCH_NAME 32

// MIDI note number for C3
#define MIDI_C3 60

// Envelope stage indices
#define ENV_ATTACK 0
#define ENV_DECAY1 1
#define ENV_DECAY2 2
#define ENV_RELEASE 3

// DX7 Operator structure
typedef struct {
    // Frequency parameters
    double freq_ratio;      // 0.50 to 31.99
    int detune;            // -7 to +7
    
    // Envelope parameters (4 stages: Attack, Decay1, Decay2, Release)
    int env_rates[ENVELOPE_STAGES];   // 0-99 for each stage
    int env_levels[ENVELOPE_STAGES];  // 0-99 for each stage
    
    // Output and scaling
    int output_level;      // 0-99
    int key_vel_sens;      // 0-7
    
    // Keyboard scaling
    int key_level_scale_break_point;  // 0-99 (MIDI note)
    int key_level_scale_left_depth;   // 0-99
    int key_level_scale_right_depth;  // 0-99
    int key_level_scale_left_curve;   // 0-3
    int key_level_scale_right_curve;  // 0-3
    
    int key_rate_scaling;  // 0-7
    
    // Oscillator sync
    int osc_sync;         // 0-1
} dx7_operator_t;

// DX7 Patch structure
typedef struct {
    char name[MAX_PATCH_NAME];
    dx7_operator_t operators[MAX_OPERATORS];
    
    // Algorithm and feedback
    int algorithm;        // 1-32
    int feedback;         // 0-7
    
    // LFO parameters
    int lfo_speed;        // 0-99
    int lfo_delay;        // 0-99
    int lfo_pmd;          // 0-99 (pitch mod depth)
    int lfo_amd;          // 0-99 (amplitude mod depth)
    int lfo_sync;         // 0-1
    int lfo_wave;         // 0-5
    int lfo_pitch_mod_sens; // 0-7
    
    // Pitch envelope
    int pitch_env_rates[ENVELOPE_STAGES];   // 0-99
    int pitch_env_levels[ENVELOPE_STAGES];  // 0-50
    
    // Transpose and tune
    int transpose;        // -24 to +24
    
    // Voice parameters
    int poly_mono;        // 0=poly, 1=mono
    int pitch_bend_range; // 0-12
    int portamento_mode;  // 0-1
    int portamento_gliss; // 0-1
    int portamento_time;  // 0-99
} dx7_patch_t;

// Envelope state for runtime
typedef struct {
    int stage;            // Current envelope stage
    double level;         // Current envelope level (0.0-1.0)
    double rate;          // Current rate
    double target;        // Target level for current stage
    int samples_in_stage; // Samples elapsed in current stage
} envelope_state_t;

// Operator state for runtime
typedef struct {
    double phase;         // Current phase (0.0-1.0)
    double freq;          // Current frequency in Hz
    double output;        // Current output value
    envelope_state_t env; // Envelope state
    double level_scale;   // Keyboard level scaling factor
    double rate_scale;    // Keyboard rate scaling factor
} operator_state_t;

// Voice state for runtime
typedef struct {
    operator_state_t operators[MAX_OPERATORS];
    double note_freq;     // Base note frequency
    int midi_note;        // MIDI note number
    double velocity;      // Note velocity (0.0-1.0)
    int samples_played;   // Total samples played
    double lfo_phase;     // LFO phase
} voice_state_t;

// Function declarations from envelope.c
double dx7_envelope_rate_to_time(int rate, int level_diff);
void init_envelope(envelope_state_t* env, const dx7_operator_t* op, double rate_scale);
double update_envelope(envelope_state_t* env, const dx7_operator_t* op, double rate_scale);
void trigger_release(envelope_state_t* env, const dx7_operator_t* op, double rate_scale);

// Function declarations from oscillators.c
void init_operators(voice_state_t* voice, const dx7_patch_t* patch, int midi_note, double velocity);
double process_operators(voice_state_t* voice, const dx7_patch_t* patch);
double midi_note_to_frequency(int midi_note);
double calculate_key_scaling(int midi_note, int break_point, int left_depth, int right_depth, 
                           int left_curve, int right_curve);

// Function declarations from algorithms.c
double process_algorithm(const double* op_outputs, const double* op_levels, int algorithm, double feedback_val);
void get_algorithm_routing(int algorithm, int* carriers, int* num_carriers, 
                          int routing[MAX_OPERATORS][MAX_OPERATORS]);

// Function declarations from main.c
int load_patch(const char* filename, dx7_patch_t* patch);
void print_usage(const char* program_name);
double calculate_lfo_frequency(const dx7_patch_t* patch);
int calculate_perfect_loop_samples(const dx7_patch_t* patch, int num_cycles);
int find_zero_crossing_loop_end(voice_state_t* voice, const dx7_patch_t* patch, 
                                int target_samples, float* buffer, int max_samples);

#endif // DX7_H
