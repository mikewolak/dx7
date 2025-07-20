#include "dx7.h"
#include <string.h>
#include <stdio.h>

// Calculate DX7 SysEx checksum (2's complement)
uint8_t calculate_dx7_checksum(const uint8_t* data, size_t length) {
    uint32_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return (128 - (sum & 0x7F)) & 0x7F;
}

// Convert frequency ratio to DX7 coarse/fine format
static void freq_ratio_to_dx7_format(double ratio, uint8_t* coarse, uint8_t* fine) {
    if (ratio < 1.0) {
        // Fixed ratios below 1.0
        if (ratio >= 0.50) {
            *coarse = 0;  // 0.50
            *fine = 0;
        } else {
            *coarse = 0;
            *fine = 0;
        }
    } else {
        // Integer part (coarse)
        int coarse_int = (int)ratio;
        if (coarse_int > 31) coarse_int = 31;
        *coarse = coarse_int;
        
        // Fractional part (fine) - scale to 0-99
        double fractional = ratio - coarse_int;
        *fine = (uint8_t)(fractional * 99.0);
        if (*fine > 99) *fine = 99;
    }
}

// Convert DX7 coarse/fine format to frequency ratio
static double dx7_format_to_freq_ratio(uint8_t coarse, uint8_t fine) {
    if (coarse == 0) {
        return 0.50; // Special case for sub-harmonic
    }
    return (double)coarse + ((double)fine / 99.0);
}

// Convert patch to DX7 SysEx format
bool dx7_patch_to_sysex(const dx7_patch_t* patch, dx7_sysex_voice_t* sysex, int channel) {
    if (!patch || !sysex || channel < 0 || channel > 15) {
        return false;
    }
    
    // Initialize SysEx header
    sysex->start_sysex = 0xF0;
    sysex->manufacturer = 0x43;           // Yamaha
    sysex->sub_status = 0x00 | channel;   // Device 0 + MIDI channel
    sysex->format = 0x00;                 // Voice data
    sysex->byte_count_msb = 0x01;         // MSB of 155
    sysex->byte_count_lsb = 0x1B;         // LSB of 155 (decimal)
    sysex->end_sysex = 0xF7;
    
    // Clear voice data
    memset(sysex->voice_data, 0, sizeof(sysex->voice_data));
    
    // Pack operator data (6 operators × 21 bytes = 126 bytes)
    // DX7 stores operators in reverse order (6,5,4,3,2,1)
    for (int op = 0; op < 6; op++) {
        int dx7_op = 5 - op; // Reverse order for DX7
        int base = op * 21;
        const dx7_operator_t* operator = &patch->operators[dx7_op];
        
        // Envelope rates (0-3)
        sysex->voice_data[base + 0] = operator->env_rates[ENV_ATTACK];
        sysex->voice_data[base + 1] = operator->env_rates[ENV_DECAY1];
        sysex->voice_data[base + 2] = operator->env_rates[ENV_DECAY2];
        sysex->voice_data[base + 3] = operator->env_rates[ENV_RELEASE];
        
        // Envelope levels (4-7)
        sysex->voice_data[base + 4] = operator->env_levels[ENV_ATTACK];
        sysex->voice_data[base + 5] = operator->env_levels[ENV_DECAY1];
        sysex->voice_data[base + 6] = operator->env_levels[ENV_DECAY2];
        sysex->voice_data[base + 7] = operator->env_levels[ENV_RELEASE];
        
        // Keyboard level scaling (8-10)
        sysex->voice_data[base + 8] = operator->key_level_scale_break_point;
        sysex->voice_data[base + 9] = operator->key_level_scale_left_depth;
        sysex->voice_data[base + 10] = operator->key_level_scale_right_depth;
        
        // Packed: Left curve + Right curve (11-12)
        sysex->voice_data[base + 11] = operator->key_level_scale_left_curve & 0x03;
        sysex->voice_data[base + 12] = (operator->key_level_scale_right_curve & 0x03) | 
                                      ((operator->key_rate_scaling & 0x07) << 2);
        
        // Packed: Amp mod sensitivity + Key velocity sensitivity (13)
        sysex->voice_data[base + 13] = 0 | ((operator->key_vel_sens & 0x07) << 2);
        
        // Output level (14)
        sysex->voice_data[base + 14] = operator->output_level;
        
        // Frequency parameters (15-17)
        uint8_t freq_coarse, freq_fine;
        freq_ratio_to_dx7_format(operator->freq_ratio, &freq_coarse, &freq_fine);
        
        // Packed: Oscillator mode + Frequency coarse (15)
        sysex->voice_data[base + 15] = (operator->osc_sync & 0x01) | ((freq_coarse & 0x1F) << 1);
        
        // Frequency fine (16)
        sysex->voice_data[base + 16] = freq_fine;
        
        // Packed: Oscillator sync + Detune (17)
        // Convert detune from -7 to +7 range to 0-14 range
        uint8_t detune_dx7 = (operator->detune + 7) & 0x0F;
        sysex->voice_data[base + 17] = (operator->osc_sync & 0x01) | ((detune_dx7 & 0x0F) << 1);
        
        // Remaining bytes (18-20) - unused in basic DX7
        sysex->voice_data[base + 18] = 0;
        sysex->voice_data[base + 19] = 0;
        sysex->voice_data[base + 20] = 0;
    }
    
    // Global parameters (bytes 126-154)
    int global_base = 126;
    
    // Pitch envelope rates (126-129)
    for (int i = 0; i < 4; i++) {
        sysex->voice_data[global_base + i] = patch->pitch_env_rates[i];
    }
    
    // Pitch envelope levels (130-133)
    for (int i = 0; i < 4; i++) {
        sysex->voice_data[global_base + 4 + i] = patch->pitch_env_levels[i];
    }
    
    // Algorithm (134) - convert from 1-32 to 0-31
    sysex->voice_data[134] = (patch->algorithm - 1) & 0x1F;
    
    // Packed: Feedback + Oscillator sync (135)
    sysex->voice_data[135] = (patch->feedback & 0x07);
    
    // LFO parameters (136-140)
    sysex->voice_data[136] = patch->lfo_speed;
    sysex->voice_data[137] = patch->lfo_delay;
    sysex->voice_data[138] = patch->lfo_pmd;
    sysex->voice_data[139] = patch->lfo_amd;
    
    // Packed: LFO sync + LFO wave + Pitch mod sensitivity (140)
    sysex->voice_data[140] = (patch->lfo_sync & 0x01) | 
                            ((patch->lfo_wave & 0x07) << 1) |
                            ((patch->lfo_pitch_mod_sens & 0x07) << 4);
    
    // Transpose (141) - convert from -24/+24 to 0-48
    sysex->voice_data[141] = (patch->transpose + 24) & 0x3F;
    
    // Voice name (142-151) - 10 characters, padded with spaces
    for (int i = 0; i < 10; i++) {
        if (i < (int)strlen(patch->name)) {
            sysex->voice_data[142 + i] = (uint8_t)patch->name[i];
        } else {
            sysex->voice_data[142 + i] = ' '; // Pad with spaces
        }
    }
    
    // Operator enable flags (152) - all operators enabled
    sysex->voice_data[152] = 0x3F; // All 6 operators on
    
    // Remaining bytes (153-154) - unused
    sysex->voice_data[153] = 0;
    sysex->voice_data[154] = 0;
    
    // Calculate and set checksum
    sysex->checksum = calculate_dx7_checksum(sysex->voice_data, 155);
    
    return true;
}

// Convert DX7 SysEx to patch format (for receiving patches)
bool dx7_sysex_to_patch(const dx7_sysex_voice_t* sysex, dx7_patch_t* patch) {
    if (!sysex || !patch) {
        return false;
    }
    
    // Verify SysEx header
    if (sysex->start_sysex != 0xF0 || 
        sysex->manufacturer != 0x43 ||
        sysex->format != 0x00 ||
        sysex->byte_count_msb != 0x01 ||
        sysex->byte_count_lsb != 0x1B ||
        sysex->end_sysex != 0xF7) {
        return false;
    }
    
    // Verify checksum
    uint8_t calculated_checksum = calculate_dx7_checksum(sysex->voice_data, 155);
    if (calculated_checksum != sysex->checksum) {
        return false;
    }
    
    // Clear patch
    memset(patch, 0, sizeof(dx7_patch_t));
    
    // Unpack operator data (reverse order: DX7 stores as 6,5,4,3,2,1)
    for (int op = 0; op < 6; op++) {
        int dx7_op = 5 - op; // Reverse order
        int base = op * 21;
        dx7_operator_t* operator = &patch->operators[dx7_op];
        
        // Envelope rates and levels
        operator->env_rates[ENV_ATTACK] = sysex->voice_data[base + 0];
        operator->env_rates[ENV_DECAY1] = sysex->voice_data[base + 1];
        operator->env_rates[ENV_DECAY2] = sysex->voice_data[base + 2];
        operator->env_rates[ENV_RELEASE] = sysex->voice_data[base + 3];
        
        operator->env_levels[ENV_ATTACK] = sysex->voice_data[base + 4];
        operator->env_levels[ENV_DECAY1] = sysex->voice_data[base + 5];
        operator->env_levels[ENV_DECAY2] = sysex->voice_data[base + 6];
        operator->env_levels[ENV_RELEASE] = sysex->voice_data[base + 7];
        
        // Keyboard scaling
        operator->key_level_scale_break_point = sysex->voice_data[base + 8];
        operator->key_level_scale_left_depth = sysex->voice_data[base + 9];
        operator->key_level_scale_right_depth = sysex->voice_data[base + 10];
        operator->key_level_scale_left_curve = sysex->voice_data[base + 11] & 0x03;
        operator->key_level_scale_right_curve = sysex->voice_data[base + 12] & 0x03;
        operator->key_rate_scaling = (sysex->voice_data[base + 12] >> 2) & 0x07;
        
        // Velocity sensitivity
        operator->key_vel_sens = (sysex->voice_data[base + 13] >> 2) & 0x07;
        
        // Output level
        operator->output_level = sysex->voice_data[base + 14];
        
        // Frequency parameters
        uint8_t freq_coarse = (sysex->voice_data[base + 15] >> 1) & 0x1F;
        uint8_t freq_fine = sysex->voice_data[base + 16];
        operator->freq_ratio = dx7_format_to_freq_ratio(freq_coarse, freq_fine);
        
        // Detune and sync
        operator->osc_sync = sysex->voice_data[base + 15] & 0x01;
        uint8_t detune_dx7 = (sysex->voice_data[base + 17] >> 1) & 0x0F;
        operator->detune = (int)detune_dx7 - 7; // Convert from 0-14 to -7 to +7
    }
    
    // Global parameters
    for (int i = 0; i < 4; i++) {
        patch->pitch_env_rates[i] = sysex->voice_data[126 + i];
        patch->pitch_env_levels[i] = sysex->voice_data[130 + i];
    }
    
    // Algorithm (convert from 0-31 to 1-32)
    patch->algorithm = (sysex->voice_data[134] & 0x1F) + 1;
    
    // Feedback
    patch->feedback = sysex->voice_data[135] & 0x07;
    
    // LFO parameters
    patch->lfo_speed = sysex->voice_data[136];
    patch->lfo_delay = sysex->voice_data[137];
    patch->lfo_pmd = sysex->voice_data[138];
    patch->lfo_amd = sysex->voice_data[139];
    patch->lfo_sync = sysex->voice_data[140] & 0x01;
    patch->lfo_wave = (sysex->voice_data[140] >> 1) & 0x07;
    patch->lfo_pitch_mod_sens = (sysex->voice_data[140] >> 4) & 0x07;
    
    // Transpose (convert from 0-48 to -24/+24)
    patch->transpose = (int)(sysex->voice_data[141] & 0x3F) - 24;
    
    // Voice name
    for (int i = 0; i < 10 && i < MAX_PATCH_NAME - 1; i++) {
        patch->name[i] = (char)sysex->voice_data[142 + i];
    }
    patch->name[MAX_PATCH_NAME - 1] = '\0';
    
    // Trim trailing spaces from name
    int len = strlen(patch->name);
    while (len > 0 && patch->name[len - 1] == ' ') {
        patch->name[--len] = '\0';
    }
    
    return true;
}

// Send patch to MIDI device
bool dx7_send_patch_to_device(void* device_handle, const dx7_patch_t* patch, int channel) {
    if (!device_handle || !patch) {
        return false;
    }
    
    // Convert patch to SysEx
    dx7_sysex_voice_t sysex;
    if (!dx7_patch_to_sysex(patch, &sysex, channel)) {
        return false;
    }
    
    // Send SysEx data
    bool result = midi_platform_send_data(device_handle, (const uint8_t*)&sysex, sizeof(sysex));
    
    if (result) {
        printf("Successfully sent patch '%s' to DX7 (channel %d)\n", patch->name, channel + 1);
    } else {
        printf("Failed to send patch to DX7\n");
    }
    
    return result;
}
