#include "dx7.h"
#include <getopt.h>
#include <unistd.h>
#include <string.h>

// Global sample rate variable
int g_sample_rate = 48000;

void print_usage(const char* program_name) {
    printf("Usage: %s [options] <patch_file>\n", program_name);
    printf("Options:\n");
    printf("  -n, --note <note>     MIDI note number (default: 60 = C3)\n");
    printf("  -o, --output <file>   Output WAV file (default: output.wav)\n");
    printf("  -v, --velocity <vel>  Note velocity 0-127 (default: 100)\n");
    printf("  -d, --duration <sec>  Duration in seconds (default: 1.0)\n");
    printf("  -s, --samplerate <hz> Sample rate in Hz (default: 48000)\n");
    printf("  -l, --loop [cycles]   Generate perfect loop (1-16 cycles, default: 1)\n");
    printf("                        Overrides duration - creates exact LFO cycles\n");
    printf("  -m, --midi-list       List available MIDI devices and exit\n");
    printf("  -M, --midi-send <dev> Send patch to MIDI device (device index)\n");
    printf("  -c, --midi-channel <ch> MIDI channel for SysEx (1-16, default: 1)\n");
    printf("  -p, --play            Real-time MIDI play mode\n");
    printf("  -i, --midi-input <dev> MIDI input device for play mode (device index)\n");
    printf("  -h, --help           Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s -n 64 -o epiano.wav epiano.patch\n", program_name);
    printf("  %s -s 44100 -d 2.0 -o brass.wav brass1.patch\n", program_name);
    printf("  %s -l 4 -o wobble_loop.wav wobble_bass.patch  # 4 LFO cycles\n", program_name);
    printf("  %s -m                                         # List MIDI devices\n", program_name);
    printf("  %s -M 0 -c 1 epiano.patch                     # Send to MIDI device 0, channel 1\n", program_name);
    printf("  %s -p -i 0 -c 1 epiano.patch                  # Real-time play mode\n", program_name);
}

int load_patch(const char* filename, dx7_patch_t* patch) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open patch file '%s'\n", filename);
        return -1;
    }
    
    char line[256];
    int current_operator = -1;
    
    // Initialize patch with defaults
    memset(patch, 0, sizeof(dx7_patch_t));
    strcpy(patch->name, "INIT VOICE");
    patch->algorithm = 1;
    patch->feedback = 0;
    patch->transpose = 0;
    patch->poly_mono = 0;
    patch->pitch_bend_range = 2;
    
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') continue;
        
        // Parse operator header
        if (strncmp(line, "OP", 2) == 0) {
            current_operator = atoi(line + 2) - 1; // Convert to 0-indexed
            if (current_operator < 0 || current_operator >= MAX_OPERATORS) {
                current_operator = -1;
            }
            continue;
        }
        
        // Parse parameters
        char param[64], value[64];
        if (sscanf(line, "%63s = %63s", param, value) == 2) {
            if (strcmp(param, "NAME") == 0) {
                strncpy(patch->name, value, MAX_PATCH_NAME - 1);
                patch->name[MAX_PATCH_NAME - 1] = '\0';
            } else if (strcmp(param, "ALGORITHM") == 0) {
                patch->algorithm = atoi(value);
            } else if (strcmp(param, "FEEDBACK") == 0) {
                patch->feedback = atoi(value);
            } else if (strcmp(param, "LFO_SPEED") == 0) {
                patch->lfo_speed = atoi(value);
            } else if (strcmp(param, "LFO_DELAY") == 0) {
                patch->lfo_delay = atoi(value);
            } else if (strcmp(param, "LFO_PMD") == 0) {
                patch->lfo_pmd = atoi(value);
            } else if (strcmp(param, "LFO_AMD") == 0) {
                patch->lfo_amd = atoi(value);
            } else if (strcmp(param, "LFO_SYNC") == 0) {
                patch->lfo_sync = atoi(value);
            } else if (strcmp(param, "LFO_WAVE") == 0) {
                patch->lfo_wave = atoi(value);
            } else if (strcmp(param, "LFO_PITCH_MOD_SENS") == 0) {
                patch->lfo_pitch_mod_sens = atoi(value);
            } else if (strcmp(param, "TRANSPOSE") == 0) {
                patch->transpose = atoi(value);
            } else if (current_operator >= 0) {
                dx7_operator_t* op = &patch->operators[current_operator];
                
                if (strcmp(param, "FREQ_RATIO") == 0) {
                    op->freq_ratio = atof(value);
                } else if (strcmp(param, "DETUNE") == 0) {
                    op->detune = atoi(value);
                } else if (strcmp(param, "OUTPUT_LEVEL") == 0) {
                    op->output_level = atoi(value);
                } else if (strcmp(param, "KEY_VEL_SENS") == 0) {
                    op->key_vel_sens = atoi(value);
                } else if (strcmp(param, "ENV_ATTACK") == 0) {
                    op->env_rates[ENV_ATTACK] = atoi(value);
                } else if (strcmp(param, "ENV_DECAY1") == 0) {
                    op->env_rates[ENV_DECAY1] = atoi(value);
                } else if (strcmp(param, "ENV_DECAY2") == 0) {
                    op->env_rates[ENV_DECAY2] = atoi(value);
                } else if (strcmp(param, "ENV_RELEASE") == 0) {
                    op->env_rates[ENV_RELEASE] = atoi(value);
                } else if (strcmp(param, "ENV_LEVEL1") == 0) {
                    op->env_levels[ENV_ATTACK] = atoi(value);
                } else if (strcmp(param, "ENV_LEVEL2") == 0) {
                    op->env_levels[ENV_DECAY1] = atoi(value);
                } else if (strcmp(param, "ENV_LEVEL3") == 0) {
                    op->env_levels[ENV_DECAY2] = atoi(value);
                } else if (strcmp(param, "ENV_LEVEL4") == 0) {
                    op->env_levels[ENV_RELEASE] = atoi(value);
                } else if (strcmp(param, "KEY_LEVEL_SCALE_BREAK_POINT") == 0) {
                    op->key_level_scale_break_point = atoi(value);
                } else if (strcmp(param, "KEY_LEVEL_SCALE_LEFT_DEPTH") == 0) {
                    op->key_level_scale_left_depth = atoi(value);
                } else if (strcmp(param, "KEY_LEVEL_SCALE_RIGHT_DEPTH") == 0) {
                    op->key_level_scale_right_depth = atoi(value);
                } else if (strcmp(param, "KEY_LEVEL_SCALE_LEFT_CURVE") == 0) {
                    op->key_level_scale_left_curve = atoi(value);
                } else if (strcmp(param, "KEY_LEVEL_SCALE_RIGHT_CURVE") == 0) {
                    op->key_level_scale_right_curve = atoi(value);
                } else if (strcmp(param, "KEY_RATE_SCALING") == 0) {
                    op->key_rate_scaling = atoi(value);
                } else if (strcmp(param, "OSC_SYNC") == 0) {
                    op->osc_sync = atoi(value);
                }
            }
        }
    }
    
    fclose(file);
    printf("Loaded patch: %s\n", patch->name);
    return 0;
}

double calculate_lfo_frequency(const dx7_patch_t* patch) {
    // LFO frequency calculation based on oscillators.c implementation
    // Max frequency is 6 Hz when speed = 99
    return (double)patch->lfo_speed / 99.0 * 6.0;
}

int calculate_perfect_loop_samples(const dx7_patch_t* patch, int num_cycles) {
    double lfo_freq = calculate_lfo_frequency(patch);
    
    if (lfo_freq <= 0.0) {
        // No LFO, default to 1 second loop
        printf("No LFO detected, using 1 second loop\n");
        return g_sample_rate;
    }
    
    // Calculate approximate samples for desired number of LFO cycles
    double cycle_time = (double)num_cycles / lfo_freq;
    int samples = (int)round(cycle_time * g_sample_rate);
    
    // Ensure we have at least some samples
    if (samples < 1) samples = 1;
    
    printf("LFO frequency: %.2f Hz\n", lfo_freq);
    printf("Approximate loop duration: %.3f seconds (%d cycles)\n", cycle_time, num_cycles);
    printf("Target samples (will adjust for zero crossing): %d\n", samples);
    
    return samples;
}

int find_zero_crossing_loop_end(voice_state_t* voice, const dx7_patch_t* patch, 
                                int target_samples, float* buffer, int max_samples) {
    double prev_sample = 0.0;
    double current_sample = 0.0;
    int samples_generated = 0;
    int lfo_cycles_completed = 0;
    double prev_lfo_phase = 0.0;
    int target_cycles = (int)round((double)target_samples * calculate_lfo_frequency(patch) / g_sample_rate);
    int loop_start_index = 0;
    int found_start = 0;
    
    printf("Target LFO cycles: %d\n", target_cycles);
    
    // Step 1: Find first zero crossing to start our loop
    printf("Finding starting zero crossing...\n");
    for (int i = 0; i < max_samples / 4; i++) { // Search first quarter of buffer for start
        current_sample = process_operators(voice, patch);
        
        if (i > 0) {
            // Look for zero crossing (sign change or very close to zero)
            if (((prev_sample >= 0.0 && current_sample < 0.0) || 
                 (prev_sample < 0.0 && current_sample >= 0.0)) ||
                 (fabs(current_sample) < 0.001)) { // Very close to zero
                
                loop_start_index = i;
                buffer[i] = 0.0f; // Force exact zero at start
                found_start = 1;
                samples_generated = i + 1;
                prev_lfo_phase = voice->lfo_phase;
                printf("Loop start at sample %d (value: %.6f)\n", i, current_sample);
                break;
            }
        }
        
        buffer[i] = (float)current_sample * 0.8f;
        prev_sample = current_sample;
    }
    
    if (!found_start) {
        printf("Warning: Could not find starting zero crossing, using sample 0\n");
        loop_start_index = 0;
        samples_generated = 1;
        prev_lfo_phase = voice->lfo_phase;
        buffer[0] = 0.0f; // Force zero start
    }
    
    // Step 2: Continue synthesis, counting LFO cycles
    prev_sample = 0.0; // Reset for end detection
    
    for (int i = samples_generated; i < max_samples; i++) {
        current_sample = process_operators(voice, patch);
        
        // Count completed LFO cycles (detect when LFO phase wraps around)
        if (voice->lfo_phase < prev_lfo_phase) {
            lfo_cycles_completed++;
            printf("LFO cycle %d completed at sample %d\n", lfo_cycles_completed, i);
        }
        prev_lfo_phase = voice->lfo_phase;
        
        // Step 3: After completing target cycles, look for ending zero crossing
        if (lfo_cycles_completed >= target_cycles && i > (loop_start_index + target_samples)) {
            // Check for zero crossing (sign change) or very close to zero
            if (((prev_sample >= 0.0 && current_sample < 0.0) || 
                 (prev_sample < 0.0 && current_sample >= 0.0)) ||
                 (fabs(current_sample) < 0.001)) {
                
                buffer[i] = 0.0f; // Force exact zero for perfect loop end
                samples_generated = i + 1;
                
                printf("Loop end zero crossing at sample %d (value: %.6f)\n", i, current_sample);
                printf("Total LFO cycles completed: %d\n", lfo_cycles_completed);
                printf("Loop length: %d samples (%.3f seconds)\n", 
                       samples_generated - loop_start_index, 
                       (double)(samples_generated - loop_start_index) / g_sample_rate);
                
                // Shift the loop to start at index 0
                if (loop_start_index > 0) {
                    int loop_length = samples_generated - loop_start_index;
                    memmove(buffer, buffer + loop_start_index, loop_length * sizeof(float));
                    samples_generated = loop_length;
                    printf("Shifted loop to start at sample 0, final length: %d samples\n", samples_generated);
                }
                
                return samples_generated;
            }
        }
        
        // Apply gentle limiting to prevent clipping
        if (current_sample > 1.0) current_sample = 1.0;
        if (current_sample < -1.0) current_sample = -1.0;
        
        buffer[i] = (float)current_sample * 0.8f;
        prev_sample = current_sample;
        samples_generated++;
        
        // Safety check
        if (i >= max_samples - 1) {
            printf("Warning: Reached maximum samples without finding ending zero crossing\n");
            break;
        }
    }
    
    // If we didn't find a perfect end, just return what we have
    if (loop_start_index > 0 && samples_generated > loop_start_index) {
        int loop_length = samples_generated - loop_start_index;
        memmove(buffer, buffer + loop_start_index, loop_length * sizeof(float));
        return loop_length;
    }
    
    return samples_generated;
}

// List available MIDI devices
void list_midi_devices(void) {
    printf("🎹 Available MIDI Devices:\n");
    printf("==================================================\n");
    
    if (!midi_platform_initialize()) {
        printf("❌ Failed to initialize MIDI system\n");
        return;
    }
    
    midi_device_list_t* devices = midi_get_device_list();
    if (!devices) {
        printf("❌ Failed to get MIDI device list\n");
        midi_platform_shutdown();
        return;
    }
    
    printf("📤 Output Devices (for sending patches to DX7):\n");
    if (devices->output_count == 0) {
        printf("   No MIDI output devices found\n");
    } else {
        for (int i = 0; i < devices->output_count; i++) {
            midi_device_info_t* device = &devices->output_devices[i];
            printf("   [%d] %s\n", i, device->display_name);
            printf("       Name: %s\n", device->name);
            printf("       Manufacturer: %s\n", device->manufacturer);
            printf("       Model: %s\n", device->model);
            printf("       Status: %s%s\n", 
                   device->online ? "Online" : "Offline",
                   device->external ? " (External)" : " (Internal)");
            printf("\n");
        }
    }
    
    printf("📥 Input Devices (for receiving patches from DX7):\n");
    if (devices->input_count == 0) {
        printf("   No MIDI input devices found\n");
    } else {
        for (int i = 0; i < devices->input_count; i++) {
            midi_device_info_t* device = &devices->input_devices[i];
            printf("   [%d] %s\n", i, device->display_name);
            printf("       Name: %s\n", device->name);
            printf("       Status: %s%s\n", 
                   device->online ? "Online" : "Offline",
                   device->external ? " (External)" : " (Internal)");
            printf("\n");
        }
    }
    
    midi_free_device_list(devices);
    midi_platform_shutdown();
}

// Send patch to MIDI device
bool send_patch_to_midi_device(int device_index, const dx7_patch_t* patch, int channel) {
    if (!patch || channel < 1 || channel > 16) {
        return false;
    }
    
    if (!midi_platform_initialize()) {
        printf("❌ Failed to initialize MIDI system\n");
        return false;
    }
    
    // Get device list to validate index
    midi_device_list_t* devices = midi_get_device_list();
    if (!devices) {
        printf("❌ Failed to get MIDI device list\n");
        midi_platform_shutdown();
        return false;
    }
    
    if (device_index < 0 || device_index >= devices->output_count) {
        printf("❌ Invalid MIDI device index %d (available: 0-%d)\n", 
               device_index, devices->output_count - 1);
        midi_free_device_list(devices);
        midi_platform_shutdown();
        return false;
    }
    
    printf("🎹 Sending patch '%s' to device: %s\n", 
           patch->name, devices->output_devices[device_index].display_name);
    
    // Open MIDI device
    void* device_handle = NULL;
    bool success = midi_platform_open_output_device(device_index, &device_handle);
    
    if (!success || !device_handle) {
        printf("❌ Failed to open MIDI device %d\n", device_index);
        midi_free_device_list(devices);
        midi_platform_shutdown();
        return false;
    }
    
    // Send patch as SysEx
    success = dx7_send_patch_to_device(device_handle, patch, channel - 1); // Convert to 0-based
    
    // Cleanup
    midi_platform_close_device(device_handle);
    midi_free_device_list(devices);
    midi_platform_shutdown();
    
    if (success) {
        printf("✅ Patch sent successfully to DX7 on channel %d\n", channel);
        printf("💡 The patch should now be loaded in the DX7's edit buffer\n");
        printf("💡 Save it to a patch location on the DX7 if you want to keep it\n");
    }
    
    return success;
}

int main(int argc, char* argv[]) {
    // Default parameters
    int midi_note = MIDI_C3;
    char output_filename[256] = "output.wav";
    int velocity = 100;
    double duration = 1.0;
    int use_loop_mode = 0;
    int loop_cycles = 1;
    int midi_list = 0;
    int midi_device = -1;
    int midi_channel = 1;
    int play_mode = 0;
    int midi_input_device = -1;
    
    // Command line parsing
    static struct option long_options[] = {
        {"note", required_argument, 0, 'n'},
        {"output", required_argument, 0, 'o'},
        {"velocity", required_argument, 0, 'v'},
        {"duration", required_argument, 0, 'd'},
        {"samplerate", required_argument, 0, 's'},
        {"loop", optional_argument, 0, 'l'},
        {"midi-list", no_argument, 0, 'm'},
        {"midi-send", required_argument, 0, 'M'},
        {"midi-channel", required_argument, 0, 'c'},
        {"play", no_argument, 0, 'p'},
        {"midi-input", required_argument, 0, 'i'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "n:o:v:d:s:l::mM:c:pi:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'n':
                midi_note = atoi(optarg);
                if (midi_note < 0 || midi_note > 127) {
                    fprintf(stderr, "Error: MIDI note must be 0-127\n");
                    return 1;
                }
                break;
            case 'o':
                strncpy(output_filename, optarg, sizeof(output_filename) - 1);
                output_filename[sizeof(output_filename) - 1] = '\0';
                break;
            case 'v':
                velocity = atoi(optarg);
                if (velocity < 0 || velocity > 127) {
                    fprintf(stderr, "Error: Velocity must be 0-127\n");
                    return 1;
                }
                break;
            case 'd':
                duration = atof(optarg);
                if (duration <= 0.0) {
                    fprintf(stderr, "Error: Duration must be positive\n");
                    return 1;
                }
                break;
            case 's':
                g_sample_rate = atoi(optarg);
                if (g_sample_rate < 8000 || g_sample_rate > 192000) {
                    fprintf(stderr, "Error: Sample rate must be 8000-192000 Hz\n");
                    return 1;
                }
                break;
            case 'l':
                use_loop_mode = 1;
                if (optarg) {
                    loop_cycles = atoi(optarg);
                    if (loop_cycles < 1 || loop_cycles > 16) {
                        fprintf(stderr, "Error: Loop cycles must be 1-16\n");
                        return 1;
                    }
                }
                break;
            case 'm':
                midi_list = 1;
                break;
            case 'M':
                midi_device = atoi(optarg);
                if (midi_device < 0) {
                    fprintf(stderr, "Error: MIDI device index must be >= 0\n");
                    return 1;
                }
                break;
            case 'c':
                midi_channel = atoi(optarg);
                if (midi_channel < 1 || midi_channel > 16) {
                    fprintf(stderr, "Error: MIDI channel must be 1-16\n");
                    return 1;
                }
                break;
            case 'p':
                play_mode = 1;
                break;
            case 'i':
                midi_input_device = atoi(optarg);
                if (midi_input_device < 0) {
                    fprintf(stderr, "Error: MIDI input device index must be >= 0\n");
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Handle MIDI device listing
    if (midi_list) {
        list_midi_devices();
        return 0;
    }
    
    // Check for patch file argument
    if (optind >= argc) {
        fprintf(stderr, "Error: No patch file specified\n");
        print_usage(argv[0]);
        return 1;
    }
    
    const char* patch_filename = argv[optind];
    
    // Load patch
    dx7_patch_t patch;
    if (load_patch(patch_filename, &patch) != 0) {
        return 1;
    }
    
    // Handle MIDI sending (no audio synthesis)
    if (midi_device >= 0) {
        bool success = send_patch_to_midi_device(midi_device, &patch, midi_channel);
        return success ? 0 : 1;
    }
    
    // Handle real-time play mode
    if (play_mode) {
        printf("🎹 Starting real-time MIDI play mode...\n");
        
        // Initialize MIDI input system
        if (!midi_input_initialize(&patch, midi_input_device, midi_channel)) {
            fprintf(stderr, "❌ Failed to initialize MIDI input system\n");
            return 1;
        }
        
        // Open MIDI input device if specified
        if (midi_input_device >= 0) {
            void* input_handle = NULL;
            if (midi_platform_open_input_device(midi_input_device, &input_handle)) {
                if (midi_platform_start_input(input_handle, NULL)) {
                    printf("✅ MIDI input device %d connected\n", midi_input_device);
                } else {
                    printf("❌ Failed to start MIDI input\n");
                    midi_input_shutdown();
                    return 1;
                }
            } else {
                printf("❌ Failed to open MIDI input device %d\n", midi_input_device);
                midi_input_shutdown();
                return 1;
            }
        }
        
        // Start play mode
        if (!midi_input_start_play_mode()) {
            fprintf(stderr, "❌ Failed to start play mode\n");
            midi_input_shutdown();
            return 1;
        }
        
        printf("\n🎵 Real-time synthesis active!\n");
        printf("🎹 Patch: %s\n", patch.name);
        printf("🎛️ MIDI Channel: %d\n", midi_channel);
        printf("🔊 Sample Rate: %d Hz\n", g_sample_rate);
        printf("\n📋 Controls:\n");
        printf("   • Play notes on your MIDI controller\n");
        printf("   • Use mod wheel for LFO modulation\n");
        printf("   • Use pitch bend wheel\n");
        printf("   • Sustain pedal supported\n");
        printf("   • Press 's' + Enter for statistics\n");
        printf("   • Press 'v' + Enter for active voices\n");
        printf("   • Press 'q' + Enter to quit\n\n");
        
        // Interactive loop
        char input[256];
        while (true) {
            if (fgets(input, sizeof(input), stdin) != NULL) {
                char command = input[0];
                
                switch (command) {
                    case 'q':
                    case 'Q':
                        printf("🛑 Stopping play mode...\n");
                        goto cleanup_play_mode;
                        
                    case 's':
                    case 'S':
                        print_midi_stats();
                        break;
                        
                    case 'v':
                    case 'V':
                        print_active_voices();
                        break;
                        
                    case 'h':
                    case 'H':
                        printf("\n📋 Commands:\n");
                        printf("   s - Show statistics\n");
                        printf("   v - Show active voices\n");
                        printf("   h - Show this help\n");
                        printf("   q - Quit\n\n");
                        break;
                        
                    default:
                        if (command != '\n') {
                            printf("❓ Unknown command '%c'. Press 'h' for help.\n", command);
                        }
                        break;
                }
            }
        }
        
        cleanup_play_mode:
        // Cleanup
        midi_input_shutdown();
        printf("✅ Play mode stopped\n");
        return 0;
    }
    
    // Apply transpose
    midi_note += patch.transpose;
    if (midi_note < 0) midi_note = 0;
    if (midi_note > 127) midi_note = 127;
    
    // Setup audio output
    SF_INFO sf_info;
    sf_info.samplerate = g_sample_rate;
    sf_info.channels = 1; // Mono
    sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    
    SNDFILE* sf = sf_open(output_filename, SFM_WRITE, &sf_info);
    if (!sf) {
        fprintf(stderr, "Error: Cannot create output file '%s'\n", output_filename);
        fprintf(stderr, "libsndfile error: %s\n", sf_strerror(sf));
        return 1;
    }
    
    // Initialize voice
    voice_state_t voice;
    double vel_normalized = (double)velocity / 127.0;
    init_operators(&voice, &patch, midi_note, vel_normalized);
    
    // Calculate buffer size
    int target_samples;
    int max_buffer_size;
    
    if (use_loop_mode) {
        target_samples = calculate_perfect_loop_samples(&patch, loop_cycles);
        // Allow extra buffer space for zero crossing search (up to 2x target)
        max_buffer_size = target_samples * 2;
    } else {
        target_samples = (int)(duration * g_sample_rate);
        max_buffer_size = target_samples;
    }
    
    // Synthesis loop
    float* buffer = malloc(max_buffer_size * sizeof(float));
    if (!buffer) {
        fprintf(stderr, "Error: Cannot allocate audio buffer\n");
        sf_close(sf);
        return 1;
    }
    
    int actual_samples;
    if (use_loop_mode) {
        // Use zero-crossing detection for perfect loops
        actual_samples = find_zero_crossing_loop_end(&voice, &patch, target_samples, buffer, max_buffer_size);
        duration = (double)actual_samples / g_sample_rate; // Update duration for display
    } else {
        // Standard synthesis
        for (int i = 0; i < target_samples; i++) {
            double sample = process_operators(&voice, &patch);
            
            // Apply gentle limiting to prevent clipping
            if (sample > 1.0) sample = 1.0;
            if (sample < -1.0) sample = -1.0;
            
            buffer[i] = (float)sample * 0.8f; // Scale down slightly for headroom
        }
        actual_samples = target_samples;
    }
    
    // Write to file
    sf_count_t written = sf_write_float(sf, buffer, actual_samples);
    if (written != actual_samples) {
        fprintf(stderr, "Error: Could not write all samples to file\n");
        free(buffer);
        sf_close(sf);
        return 1;
    }
    
    // Cleanup
    free(buffer);
    sf_close(sf);
    
    printf("Successfully created %s\n", output_filename);
    if (use_loop_mode) {
        printf("Perfect loop with zero crossings: loops seamlessly!\n");
    }
    return 0;
}
