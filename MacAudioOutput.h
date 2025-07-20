#ifndef MACAUDIOOUTPUT_H
#define MACAUDIOOUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Professional Audio Output System for macOS
// Supports 16+ voices, adjustable buffers, latency monitoring, and real-time performance tracking

// Audio output system initialization and control
void* audio_output_initialize(double sample_rate);
void audio_output_shutdown(void* handle);
bool audio_output_start(void* handle);
void audio_output_stop(void* handle);

// Audio output configuration
bool audio_output_set_sample_rate(void* handle, double sample_rate);
bool audio_output_set_buffer_size(void* handle, uint32_t buffer_size_frames);
double audio_output_get_sample_rate(void* handle);
uint32_t audio_output_get_buffer_size(void* handle);

// Professional latency and performance monitoring
double audio_output_get_latency_ms(void* handle);
double audio_output_get_cpu_load(void* handle);
bool audio_output_is_running(void* handle);

// Comprehensive statistics and monitoring
void audio_output_print_stats(void* handle);

// Audio buffer size recommendations based on use case
#define AUDIO_BUFFER_SIZE_ULTRA_LOW_LATENCY    64   // ~1.3ms at 48kHz - for live performance
#define AUDIO_BUFFER_SIZE_LOW_LATENCY         128   // ~2.7ms at 48kHz - for recording
#define AUDIO_BUFFER_SIZE_BALANCED            256   // ~5.3ms at 48kHz - good balance
#define AUDIO_BUFFER_SIZE_STANDARD            512   // ~10.7ms at 48kHz - standard (default)
#define AUDIO_BUFFER_SIZE_SAFE               1024   // ~21.3ms at 48kHz - very stable
#define AUDIO_BUFFER_SIZE_MAXIMUM            4096   // ~85.3ms at 48kHz - maximum stability

// Sample rate recommendations
#define AUDIO_SAMPLE_RATE_CD_QUALITY        44100.0  // CD quality
#define AUDIO_SAMPLE_RATE_PROFESSIONAL      48000.0  // Professional standard (default)
#define AUDIO_SAMPLE_RATE_HIGH_QUALITY      88200.0  // High quality
#define AUDIO_SAMPLE_RATE_STUDIO            96000.0  // Studio quality
#define AUDIO_SAMPLE_RATE_ULTRA_HIGH       192000.0  // Ultra high quality

// Audio format specifications
// - Output: 32-bit Float Stereo Interleaved
// - Input from synthesis: 32-bit Float Mono (converted to stereo)
// - Thread-safe audio generation with MIDI input processing
// - Automatic clipping protection and level management
// - Real-time performance monitoring and statistics

#ifdef __cplusplus
}
#endif

#endif // MACAUDIOOUTPUT_H
