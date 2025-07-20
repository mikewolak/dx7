#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "dx7.h"

#ifdef __cplusplus
extern "C" {
#endif

// MIDI message types
#define MIDI_NOTE_OFF           0x80
#define MIDI_NOTE_ON            0x90
#define MIDI_POLYPHONIC_PRESSURE 0xA0
#define MIDI_CONTROL_CHANGE     0xB0
#define MIDI_PROGRAM_CHANGE     0xC0
#define MIDI_CHANNEL_PRESSURE   0xD0
#define MIDI_PITCH_BEND         0xE0
#define MIDI_SYSTEM_EXCLUSIVE   0xF0

// MIDI Control Change numbers
#define MIDI_CC_MODWHEEL        1
#define MIDI_CC_BREATH          2
#define MIDI_CC_FOOT            4
#define MIDI_CC_PORTAMENTO_TIME 5
#define MIDI_CC_DATA_ENTRY_MSB  6
#define MIDI_CC_VOLUME          7
#define MIDI_CC_BALANCE         8
#define MIDI_CC_PAN             10
#define MIDI_CC_EXPRESSION      11
#define MIDI_CC_SUSTAIN_PEDAL   64
#define MIDI_CC_PORTAMENTO      65
#define MIDI_CC_ALL_SOUND_OFF   120
#define MIDI_CC_ALL_CONTROLLERS_OFF 121
#define MIDI_CC_ALL_NOTES_OFF   123

// Maximum polyphony
#define MAX_VOICES              16

// MIDI input parser state
typedef struct {
    uint8_t running_status;
    uint8_t data_bytes_needed;
    uint8_t data_bytes_received;
    uint8_t data_buffer[3];
    bool in_sysex;
    uint8_t sysex_buffer[512];
    size_t sysex_length;
} midi_parser_state_t;

// Voice state for polyphonic synthesis
typedef struct {
    bool active;
    uint8_t midi_note;
    uint8_t velocity;
    uint8_t channel;
    voice_state_t synth_voice;
    uint64_t note_on_time;
    bool sustain_held;
} poly_voice_t;

// MIDI controller values
typedef struct {
    float pitch_bend;       // -1.0 to +1.0 (±2 semitones by default)
    float mod_wheel;        // 0.0 to 1.0 (CC 1)
    float breath;           // 0.0 to 1.0 (CC 2)
    float foot;             // 0.0 to 1.0 (CC 4)
    float volume;           // 0.0 to 1.0 (CC 7)
    float expression;       // 0.0 to 1.0 (CC 11)
    float pan;              // -1.0 to +1.0 (CC 10)
    bool sustain_pedal;     // CC 64
    bool portamento;        // CC 65
    
    // Placeholder for all other controllers
    float controllers[128]; // All CC values 0-127
} midi_controllers_t;

// MIDI input system state
typedef struct {
    bool active;
    bool play_mode;
    pthread_mutex_t voice_mutex;
    
    // Current patch
    dx7_patch_t current_patch;
    
    // Voice management
    poly_voice_t voices[MAX_VOICES];
    int voice_count;
    uint64_t voice_counter; // For voice stealing LRU
    
    // MIDI state
    midi_parser_state_t parser;
    midi_controllers_t controllers;
    uint8_t current_channel; // 0-15 (MIDI channels 1-16)
    
    // Audio output handle
    void* audio_output_handle;
    
    // Statistics
    uint32_t notes_played;
    uint32_t voice_steals;
    uint32_t midi_errors;
} midi_input_system_t;

// Global system instance
extern midi_input_system_t g_midi_system;

// MIDI input system functions
bool midi_input_initialize(const dx7_patch_t* patch, int input_device, int channel);
void midi_input_shutdown(void);
bool midi_input_start_play_mode(void);
void midi_input_stop_play_mode(void);

// MIDI message parsing
void midi_parse_byte(uint8_t byte);
void midi_handle_message(uint8_t status, uint8_t data1, uint8_t data2);

// Voice management
int allocate_voice(uint8_t midi_note, uint8_t velocity, uint8_t channel);
void release_voice(uint8_t midi_note, uint8_t channel);
void release_all_voices(void);
poly_voice_t* find_voice(uint8_t midi_note, uint8_t channel);

// MIDI message handlers
void handle_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
void handle_note_off(uint8_t channel, uint8_t note, uint8_t velocity);
void handle_control_change(uint8_t channel, uint8_t controller, uint8_t value);
void handle_pitch_bend(uint8_t channel, uint16_t bend_value);
void handle_program_change(uint8_t channel, uint8_t program);
void handle_channel_pressure(uint8_t channel, uint8_t pressure);

// Audio generation (called by audio output thread)
void generate_audio_block(float* output_buffer, int frame_count, double sample_rate);

// Utility functions
double midi_note_to_frequency_with_bend(uint8_t midi_note, float pitch_bend);
float midi_to_float(uint8_t midi_value); // Convert 0-127 to 0.0-1.0
float midi_to_bipolar(uint8_t midi_value); // Convert 0-127 to -1.0-1.0
void apply_controllers_to_voice(poly_voice_t* voice);

// Statistics and debugging
void print_midi_stats(void);
void print_active_voices(void);

#ifdef __cplusplus
}
#endif

#endif // MIDI_INPUT_H
