#include "midi_input.h"
#include "MacAudioOutput.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

// Global MIDI system instance
midi_input_system_t g_midi_system = {0};

// External sample rate from main
extern int g_sample_rate;

// MIDI input callback for threading
static void midi_input_callback(const uint8_t *data, size_t length, uint64_t timestamp, void *context);

// Get current time in microseconds
static uint64_t get_time_microseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

// Initialize MIDI input system
bool midi_input_initialize(const dx7_patch_t* patch, int input_device, int channel) {
    if (g_midi_system.active) {
        printf("MIDI input system already active\n");
        return false;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&g_midi_system.voice_mutex, NULL) != 0) {
        printf("❌ Failed to initialize voice mutex\n");
        return false;
    }
    
    // Initialize MIDI platform
    if (!midi_platform_initialize()) {
        printf("❌ Failed to initialize MIDI platform\n");
        pthread_mutex_destroy(&g_midi_system.voice_mutex);
        return false;
    }
    
    // Copy patch
    if (patch) {
        memcpy(&g_midi_system.current_patch, patch, sizeof(dx7_patch_t));
    }
    
    // Initialize controllers to default values
    memset(&g_midi_system.controllers, 0, sizeof(midi_controllers_t));
    g_midi_system.controllers.volume = 1.0f;       // CC 7 = 127
    g_midi_system.controllers.expression = 1.0f;   // CC 11 = 127
    g_midi_system.controllers.controllers[7] = 1.0f;   // Volume
    g_midi_system.controllers.controllers[11] = 1.0f;  // Expression
    
    // Initialize parser
    memset(&g_midi_system.parser, 0, sizeof(midi_parser_state_t));
    
    // Set channel (convert from 1-16 to 0-15)
    g_midi_system.current_channel = (channel - 1) & 0x0F;
    
    // Store input device index for potential use
    if (input_device >= 0) {
        printf("🎹 MIDI input device %d configured for channel %d\n", input_device, channel);
        // Note: actual device opening is done separately in main.c
    }
    
    // Initialize audio output
    g_midi_system.audio_output_handle = audio_output_initialize(g_sample_rate);
    if (!g_midi_system.audio_output_handle) {
        printf("❌ Failed to initialize audio output\n");
        midi_platform_shutdown();
        pthread_mutex_destroy(&g_midi_system.voice_mutex);
        return false;
    }
    
    // Set MIDI input callback
    midi_platform_set_input_callback(midi_input_callback);
    
    g_midi_system.active = true;
    printf("✅ MIDI input system initialized (channel %d)\n", channel);
    
    return true;
}

// Shutdown MIDI input system
void midi_input_shutdown(void) {
    if (!g_midi_system.active) {
        return;
    }
    
    // Stop play mode if active
    if (g_midi_system.play_mode) {
        midi_input_stop_play_mode();
    }
    
    // Release all voices
    pthread_mutex_lock(&g_midi_system.voice_mutex);
    release_all_voices();
    pthread_mutex_unlock(&g_midi_system.voice_mutex);
    
    // Shutdown audio output
    if (g_midi_system.audio_output_handle) {
        audio_output_shutdown(g_midi_system.audio_output_handle);
        g_midi_system.audio_output_handle = NULL;
    }
    
    // Shutdown MIDI platform
    midi_platform_shutdown();
    
    // Cleanup mutex
    pthread_mutex_destroy(&g_midi_system.voice_mutex);
    
    // Clear system state
    memset(&g_midi_system, 0, sizeof(midi_input_system_t));
    
    printf("✅ MIDI input system shutdown\n");
}

// Start play mode
bool midi_input_start_play_mode(void) {
    if (!g_midi_system.active || g_midi_system.play_mode) {
        return false;
    }
    
    // Start audio output
    if (!audio_output_start(g_midi_system.audio_output_handle)) {
        printf("❌ Failed to start audio output\n");
        return false;
    }
    
    g_midi_system.play_mode = true;
    printf("🎹 Play mode started - ready for MIDI input!\n");
    printf("💡 Play some notes on your MIDI controller\n");
    
    return true;
}

// Stop play mode
void midi_input_stop_play_mode(void) {
    if (!g_midi_system.play_mode) {
        return;
    }
    
    g_midi_system.play_mode = false;
    
    // Stop audio output
    if (g_midi_system.audio_output_handle) {
        audio_output_stop(g_midi_system.audio_output_handle);
    }
    
    // Release all voices
    pthread_mutex_lock(&g_midi_system.voice_mutex);
    release_all_voices();
    pthread_mutex_unlock(&g_midi_system.voice_mutex);
    
    printf("🎹 Play mode stopped\n");
}

// MIDI input callback (called from MIDI thread)
static void midi_input_callback(const uint8_t *data, size_t length, uint64_t timestamp, void *context) {
    (void)timestamp;
    (void)context;
    
    if (!g_midi_system.active || !g_midi_system.play_mode) {
        return;
    }
    
    // Parse each byte
    for (size_t i = 0; i < length; i++) {
        midi_parse_byte(data[i]);
    }
}

// Parse MIDI byte with running status support
void midi_parse_byte(uint8_t byte) {
    midi_parser_state_t* parser = &g_midi_system.parser;
    
    if (byte & 0x80) {
        // Status byte
        if (byte == 0xF0) {
            // Start of SysEx
            parser->in_sysex = true;
            parser->sysex_length = 0;
            return;
        } else if (byte == 0xF7) {
            // End of SysEx
            parser->in_sysex = false;
            return;
        } else if (byte >= 0xF8) {
            // Real-time messages - ignore for now
            return;
        }
        
        // Regular status byte
        parser->running_status = byte;
        parser->data_bytes_received = 0;
        
        // Determine how many data bytes we need
        uint8_t msg_type = byte & 0xF0;
        switch (msg_type) {
            case MIDI_PROGRAM_CHANGE:
            case MIDI_CHANNEL_PRESSURE:
                parser->data_bytes_needed = 1;
                break;
            default:
                parser->data_bytes_needed = 2;
                break;
        }
    } else {
        // Data byte
        if (parser->in_sysex) {
            // Store SysEx data
            if (parser->sysex_length < sizeof(parser->sysex_buffer)) {
                parser->sysex_buffer[parser->sysex_length++] = byte;
            }
            return;
        }
        
        if (parser->running_status == 0) {
            // No running status - ignore orphaned data byte
            g_midi_system.midi_errors++;
            return;
        }
        
        // Store data byte
        parser->data_buffer[parser->data_bytes_received++] = byte;
        
        // Check if we have a complete message
        if (parser->data_bytes_received >= parser->data_bytes_needed) {
            // Process complete message
            uint8_t data1 = parser->data_bytes_needed > 0 ? parser->data_buffer[0] : 0;
            uint8_t data2 = parser->data_bytes_needed > 1 ? parser->data_buffer[1] : 0;
            
            midi_handle_message(parser->running_status, data1, data2);
            
            // Reset for next message (keep running status)
            parser->data_bytes_received = 0;
        }
    }
}

// Handle complete MIDI message
void midi_handle_message(uint8_t status, uint8_t data1, uint8_t data2) {
    uint8_t msg_type = status & 0xF0;
    uint8_t channel = status & 0x0F;
    
    // Only respond to our channel (or omni mode)
    if (channel != g_midi_system.current_channel) {
        return;
    }
    
    switch (msg_type) {
        case MIDI_NOTE_ON:
            if (data2 > 0) {
                handle_note_on(channel, data1, data2);
            } else {
                handle_note_off(channel, data1, data2);
            }
            break;
            
        case MIDI_NOTE_OFF:
            handle_note_off(channel, data1, data2);
            break;
            
        case MIDI_CONTROL_CHANGE:
            handle_control_change(channel, data1, data2);
            break;
            
        case MIDI_PITCH_BEND:
            handle_pitch_bend(channel, (uint16_t)data1 | ((uint16_t)data2 << 7));
            break;
            
        case MIDI_PROGRAM_CHANGE:
            handle_program_change(channel, data1);
            break;
            
        case MIDI_CHANNEL_PRESSURE:
            handle_channel_pressure(channel, data1);
            break;
            
        default:
            // Unsupported message type
            break;
    }
}

// Handle note on
void handle_note_on(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (note > 127 || velocity == 0) {
        return;
    }
    
    pthread_mutex_lock(&g_midi_system.voice_mutex);
    
    int voice_index = allocate_voice(note, velocity, channel);
    if (voice_index >= 0) {
        g_midi_system.notes_played++;
        printf("🎵 Note ON: %d vel:%d (voice %d)\n", note, velocity, voice_index);
    }
    
    pthread_mutex_unlock(&g_midi_system.voice_mutex);
}

// Handle note off
void handle_note_off(uint8_t channel, uint8_t note, uint8_t velocity) {
    (void)velocity; // Velocity ignored for note off
    
    pthread_mutex_lock(&g_midi_system.voice_mutex);
    
    poly_voice_t* voice = find_voice(note, channel);
    if (voice) {
        if (g_midi_system.controllers.sustain_pedal) {
            // Mark for sustain release
            voice->sustain_held = true;
        } else {
            // Release immediately
            for (int i = 0; i < MAX_OPERATORS; i++) {
                trigger_release(&voice->synth_voice.operators[i].env, 
                              &g_midi_system.current_patch.operators[i],
                              voice->synth_voice.operators[i].rate_scale);
            }
        }
        printf("🎵 Note OFF: %d\n", note);
    }
    
    pthread_mutex_unlock(&g_midi_system.voice_mutex);
}

// Handle control change
void handle_control_change(uint8_t channel, uint8_t controller, uint8_t value) {
    (void)channel;
    
    // Store raw value
    if (controller < 128) {
        g_midi_system.controllers.controllers[controller] = midi_to_float(value);
    }
    
    // Handle specific controllers
    switch (controller) {
        case MIDI_CC_MODWHEEL:
            g_midi_system.controllers.mod_wheel = midi_to_float(value);
            printf("🎛️ Mod Wheel: %.2f\n", g_midi_system.controllers.mod_wheel);
            break;
            
        case MIDI_CC_BREATH:
            g_midi_system.controllers.breath = midi_to_float(value);
            break;
            
        case MIDI_CC_FOOT:
            g_midi_system.controllers.foot = midi_to_float(value);
            break;
            
        case MIDI_CC_VOLUME:
            g_midi_system.controllers.volume = midi_to_float(value);
            printf("🔊 Volume: %.2f\n", g_midi_system.controllers.volume);
            break;
            
        case MIDI_CC_EXPRESSION:
            g_midi_system.controllers.expression = midi_to_float(value);
            break;
            
        case MIDI_CC_PAN:
            g_midi_system.controllers.pan = midi_to_bipolar(value);
            break;
            
        case MIDI_CC_SUSTAIN_PEDAL:
            g_midi_system.controllers.sustain_pedal = (value >= 64);
            printf("🦶 Sustain: %s\n", g_midi_system.controllers.sustain_pedal ? "ON" : "OFF");
            
            // If sustain released, release all sustained notes
            if (!g_midi_system.controllers.sustain_pedal) {
                pthread_mutex_lock(&g_midi_system.voice_mutex);
                for (int i = 0; i < MAX_VOICES; i++) {
                    poly_voice_t* voice = &g_midi_system.voices[i];
                    if (voice->active && voice->sustain_held) {
                        voice->sustain_held = false;
                        for (int op = 0; op < MAX_OPERATORS; op++) {
                            trigger_release(&voice->synth_voice.operators[op].env,
                                          &g_midi_system.current_patch.operators[op],
                                          voice->synth_voice.operators[op].rate_scale);
                        }
                    }
                }
                pthread_mutex_unlock(&g_midi_system.voice_mutex);
            }
            break;
            
        case MIDI_CC_PORTAMENTO:
            g_midi_system.controllers.portamento = (value >= 64);
            break;
            
        case MIDI_CC_ALL_SOUND_OFF:
        case MIDI_CC_ALL_NOTES_OFF:
            pthread_mutex_lock(&g_midi_system.voice_mutex);
            release_all_voices();
            pthread_mutex_unlock(&g_midi_system.voice_mutex);
            printf("🔇 All notes off\n");
            break;
            
        case MIDI_CC_ALL_CONTROLLERS_OFF:
            // Reset all controllers except volume and expression
            memset(&g_midi_system.controllers, 0, sizeof(midi_controllers_t));
            g_midi_system.controllers.volume = 1.0f;
            g_midi_system.controllers.expression = 1.0f;
            break;
            
        default:
            // Placeholder for other controllers
            printf("🎛️ CC %d: %d\n", controller, value);
            break;
    }
}

// Handle pitch bend
void handle_pitch_bend(uint8_t channel, uint16_t bend_value) {
    (void)channel;
    
    // Convert 14-bit pitch bend to -1.0 to +1.0
    float bend = ((float)bend_value - 8192.0f) / 8192.0f;
    g_midi_system.controllers.pitch_bend = bend;
    
    printf("🎵 Pitch Bend: %.3f\n", bend);
}

// Handle program change
void handle_program_change(uint8_t channel, uint8_t program) {
    (void)channel;
    printf("🎛️ Program Change: %d\n", program);
    // TODO: Load different patch based on program number
}

// Handle channel pressure
void handle_channel_pressure(uint8_t channel, uint8_t pressure) {
    (void)channel;
    printf("🎵 Channel Pressure: %d\n", pressure);
    // TODO: Apply pressure to all active voices
}

// Allocate voice for new note
int allocate_voice(uint8_t midi_note, uint8_t velocity, uint8_t channel) {
    // First, try to find an inactive voice
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!g_midi_system.voices[i].active) {
            poly_voice_t* voice = &g_midi_system.voices[i];
            
            // Initialize voice
            voice->active = true;
            voice->midi_note = midi_note;
            voice->velocity = velocity;
            voice->channel = channel;
            voice->note_on_time = get_time_microseconds();
            voice->sustain_held = false;
            
            // Initialize synthesis voice
            init_operators(&voice->synth_voice, &g_midi_system.current_patch, 
                          midi_note, (double)velocity / 127.0);
            
            g_midi_system.voice_count++;
            return i;
        }
    }
    
    // No free voices - steal oldest voice
    int oldest_voice = 0;
    uint64_t oldest_time = g_midi_system.voices[0].note_on_time;
    
    for (int i = 1; i < MAX_VOICES; i++) {
        if (g_midi_system.voices[i].note_on_time < oldest_time) {
            oldest_time = g_midi_system.voices[i].note_on_time;
            oldest_voice = i;
        }
    }
    
    // Steal the oldest voice
    poly_voice_t* voice = &g_midi_system.voices[oldest_voice];
    voice->active = true;
    voice->midi_note = midi_note;
    voice->velocity = velocity;
    voice->channel = channel;
    voice->note_on_time = get_time_microseconds();
    voice->sustain_held = false;
    
    // Re-initialize synthesis voice
    init_operators(&voice->synth_voice, &g_midi_system.current_patch,
                  midi_note, (double)velocity / 127.0);
    
    g_midi_system.voice_steals++;
    printf("🔄 Voice steal: voice %d\n", oldest_voice);
    
    return oldest_voice;
}

// Find voice by note and channel
poly_voice_t* find_voice(uint8_t midi_note, uint8_t channel) {
    for (int i = 0; i < MAX_VOICES; i++) {
        poly_voice_t* voice = &g_midi_system.voices[i];
        if (voice->active && voice->midi_note == midi_note && voice->channel == channel) {
            return voice;
        }
    }
    return NULL;
}

// Release all voices
void release_all_voices(void) {
    for (int i = 0; i < MAX_VOICES; i++) {
        g_midi_system.voices[i].active = false;
        g_midi_system.voices[i].sustain_held = false;
    }
    g_midi_system.voice_count = 0;
}

// Generate audio block (called by audio thread)
void generate_audio_block(float* output_buffer, int frame_count, double sample_rate) {
    // Use the sample rate parameter for any rate-dependent calculations
    // For now, we use the global sample rate, but this parameter allows for runtime changes
    (void)sample_rate; // Acknowledge parameter - could be used for future sample rate changes
    
    if (!g_midi_system.active || !g_midi_system.play_mode) {
        // Fill with silence
        memset(output_buffer, 0, frame_count * sizeof(float));
        return;
    }
    
    // Clear output buffer
    memset(output_buffer, 0, frame_count * sizeof(float));
    
    pthread_mutex_lock(&g_midi_system.voice_mutex);
    
    // Mix all active voices
    for (int voice_idx = 0; voice_idx < MAX_VOICES; voice_idx++) {
        poly_voice_t* voice = &g_midi_system.voices[voice_idx];
        
        if (!voice->active) {
            continue;
        }
        
        // Generate samples for this voice
        for (int frame = 0; frame < frame_count; frame++) {
            // Apply controllers to voice
            apply_controllers_to_voice(voice);
            
            // Generate sample
            double sample = process_operators(&voice->synth_voice, &g_midi_system.current_patch);
            
            // Apply master volume and expression
            sample *= g_midi_system.controllers.volume;
            sample *= g_midi_system.controllers.expression;
            
            // Apply velocity scaling
            sample *= (double)voice->velocity / 127.0;
            
            // Mix into output buffer
            output_buffer[frame] += (float)sample * 0.5f; // Scale to prevent clipping
        }
        
        // Check if voice envelope has finished
        bool voice_finished = true;
        for (int op = 0; op < MAX_OPERATORS; op++) {
            if (voice->synth_voice.operators[op].env.level > 0.001) {
                voice_finished = false;
                break;
            }
        }
        
        if (voice_finished) {
            voice->active = false;
            g_midi_system.voice_count--;
        }
    }
    
    pthread_mutex_unlock(&g_midi_system.voice_mutex);
}

// Apply controllers to voice
void apply_controllers_to_voice(poly_voice_t* voice) {
    // Calculate pitch with bend
    double base_freq = midi_note_to_frequency_with_bend(voice->midi_note, 
                                                       g_midi_system.controllers.pitch_bend);
    
    // Apply pitch bend to all operators
    for (int op = 0; op < MAX_OPERATORS; op++) {
        operator_state_t* op_state = &voice->synth_voice.operators[op];
        const dx7_operator_t* op_params = &g_midi_system.current_patch.operators[op];
        
        // Update frequency with pitch bend
        op_state->freq = base_freq * op_params->freq_ratio;
        
        // Apply detune
        double detune_factor = pow(2.0, (op_params->detune / 7.0) * 0.01);
        op_state->freq *= detune_factor;
    }
    
    // Apply mod wheel to LFO amplitude (if LFO is active)
    // This is done in the voice's LFO processing
}

// Convert MIDI note to frequency with pitch bend
double midi_note_to_frequency_with_bend(uint8_t midi_note, float pitch_bend) {
    // Base frequency
    double freq = midi_note_to_frequency(midi_note);
    
    // Apply pitch bend (±2 semitones by default)
    double bend_semitones = pitch_bend * 2.0; // ±2 semitones range
    freq *= pow(2.0, bend_semitones / 12.0);
    
    return freq;
}

// Convert MIDI value (0-127) to float (0.0-1.0)
float midi_to_float(uint8_t midi_value) {
    return (float)midi_value / 127.0f;
}

// Convert MIDI value (0-127) to bipolar (-1.0 to +1.0)
float midi_to_bipolar(uint8_t midi_value) {
    return ((float)midi_value / 127.0f * 2.0f) - 1.0f;
}

// Print MIDI statistics
void print_midi_stats(void) {
    printf("\n🎹 MIDI System Statistics:\n");
    printf("   Active voices: %d/%d\n", g_midi_system.voice_count, MAX_VOICES);
    printf("   Notes played: %u\n", g_midi_system.notes_played);
    printf("   Voice steals: %u\n", g_midi_system.voice_steals);
    printf("   MIDI errors: %u\n", g_midi_system.midi_errors);
    printf("   Channel: %d\n", g_midi_system.current_channel + 1);
    printf("   Pitch bend: %.3f\n", g_midi_system.controllers.pitch_bend);
    printf("   Mod wheel: %.3f\n", g_midi_system.controllers.mod_wheel);
    printf("   Volume: %.3f\n", g_midi_system.controllers.volume);
    printf("   Sustain: %s\n", g_midi_system.controllers.sustain_pedal ? "ON" : "OFF");
}

// Print active voices
void print_active_voices(void) {
    printf("\n🎵 Active Voices:\n");
    for (int i = 0; i < MAX_VOICES; i++) {
        poly_voice_t* voice = &g_midi_system.voices[i];
        if (voice->active) {
            printf("   [%d] Note:%d Vel:%d Ch:%d %s\n", 
                   i, voice->midi_note, voice->velocity, voice->channel + 1,
                   voice->sustain_held ? "(sustained)" : "");
        }
    }
}
