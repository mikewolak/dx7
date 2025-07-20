//
//  MacAudioOutput.m  
//  DX7 Synthesizer - Working macOS Audio Output (No EnableIO!)
//

#import <Foundation/Foundation.h>
#import <AudioUnit/AudioUnit.h>
#import <CoreAudio/CoreAudio.h>
#include "MacAudioOutput.h"
#include "midi_input.h"
#include <mach/mach_time.h>
#include <pthread.h>

// Audio context structure
typedef struct {
    AudioUnit output_unit;
    AudioStreamBasicDescription stream_format;
    double sample_rate;
    uint32_t buffer_size_frames;
    bool initialized;
    bool running;
    
    // Thread safety
    pthread_mutex_t audio_mutex;
    
    // Performance monitoring
    uint64_t total_frames_rendered;
    uint64_t render_callback_count;
    double cpu_load_average;
    double current_latency_ms;
    uint32_t underrun_count;
    uint32_t overrun_count;
    
    // Timing
    mach_timebase_info_data_t timebase_info;
    double nanoseconds_per_tick;
    
    // Mono to stereo conversion buffer
    float* mono_buffer;
    uint32_t mono_buffer_size;
    
    // Statistics
    struct {
        double min_callback_time_ms;
        double max_callback_time_ms;
        double avg_callback_time_ms;
        uint64_t callback_time_samples;
        double cpu_load_peak;
    } stats;
    
} mac_audio_context_t;

// Forward declarations
static OSStatus audio_render_callback(void *inRefCon,
                                     AudioUnitRenderActionFlags *ioActionFlags,
                                     const AudioTimeStamp *inTimeStamp,
                                     UInt32 inBusNumber,
                                     UInt32 inNumberFrames,
                                     AudioBufferList *ioData);

static void print_audio_error(OSStatus error, const char* operation);
static bool setup_default_output_unit(mac_audio_context_t* context);
static void cleanup_audio_unit(mac_audio_context_t* context);
static bool configure_stream_format(mac_audio_context_t* context);
static void update_performance_stats(mac_audio_context_t* context, double callback_time_ms);

// Initialize audio output system
void* audio_output_initialize(double sample_rate) {
    mac_audio_context_t* context = calloc(1, sizeof(mac_audio_context_t));
    if (!context) {
        NSLog(@"❌ Failed to allocate audio context");
        return NULL;
    }
    
    // Initialize timing
    mach_timebase_info(&context->timebase_info);
    context->nanoseconds_per_tick = (double)context->timebase_info.numer / context->timebase_info.denom;
    
    // Initialize mutex
    if (pthread_mutex_init(&context->audio_mutex, NULL) != 0) {
        NSLog(@"❌ Failed to initialize audio mutex");
        free(context);
        return NULL;
    }
    
    // Set parameters
    context->sample_rate = sample_rate;
    context->buffer_size_frames = 512; // Default
    
    // Initialize statistics
    context->stats.min_callback_time_ms = 1000.0;
    context->stats.max_callback_time_ms = 0.0;
    
    // Configure stream format
    if (!configure_stream_format(context)) {
        NSLog(@"❌ Failed to configure stream format");
        pthread_mutex_destroy(&context->audio_mutex);
        free(context);
        return NULL;
    }
    
    // Set up audio unit - this is the key fix!
    if (!setup_default_output_unit(context)) {
        NSLog(@"❌ Failed to setup audio unit");
        pthread_mutex_destroy(&context->audio_mutex);
        free(context);
        return NULL;
    }
    
    // Allocate mono buffer for conversion
    context->mono_buffer_size = 4096; // Large enough for any reasonable buffer
    context->mono_buffer = malloc(context->mono_buffer_size * sizeof(float));
    if (!context->mono_buffer) {
        NSLog(@"❌ Failed to allocate mono buffer");
        cleanup_audio_unit(context);
        pthread_mutex_destroy(&context->audio_mutex);
        free(context);
        return NULL;
    }
    
    context->current_latency_ms = (double)context->buffer_size_frames / context->sample_rate * 1000.0;
    context->initialized = true;
    
    NSLog(@"✅ Audio output initialized successfully:");
    NSLog(@"   Sample Rate: %.0f Hz", context->sample_rate);
    NSLog(@"   Buffer Size: %u frames (%.1f ms)", 
          context->buffer_size_frames, context->current_latency_ms);
    NSLog(@"   Format: 32-bit Float Stereo");
    
    return context;
}

// Shutdown audio output
void audio_output_shutdown(void* handle) {
    if (!handle) return;
    
    mac_audio_context_t* context = (mac_audio_context_t*)handle;
    
    if (context->running) {
        audio_output_stop(handle);
    }
    
    cleanup_audio_unit(context);
    
    if (context->mono_buffer) {
        free(context->mono_buffer);
    }
    
    pthread_mutex_destroy(&context->audio_mutex);
    
    NSLog(@"✅ Audio output shutdown");
    free(context);
}

// Start audio output
bool audio_output_start(void* handle) {
    if (!handle) return false;
    
    mac_audio_context_t* context = (mac_audio_context_t*)handle;
    
    if (!context->initialized || context->running) {
        return false;
    }
    
    // Reset statistics
    context->total_frames_rendered = 0;
    context->render_callback_count = 0;
    context->underrun_count = 0;
    context->overrun_count = 0;
    
    OSStatus result = AudioOutputUnitStart(context->output_unit);
    if (result != noErr) {
        print_audio_error(result, "AudioOutputUnitStart");
        return false;
    }
    
    context->running = true;
    
    NSLog(@"🔊 Audio output started - latency: %.1f ms", context->current_latency_ms);
    return true;
}

// Stop audio output
void audio_output_stop(void* handle) {
    if (!handle) return;
    
    mac_audio_context_t* context = (mac_audio_context_t*)handle;
    
    if (!context->running) {
        return;
    }
    
    OSStatus result = AudioOutputUnitStop(context->output_unit);
    if (result != noErr) {
        print_audio_error(result, "AudioOutputUnitStop");
    }
    
    context->running = false;
    
    NSLog(@"🔇 Audio output stopped");
    NSLog(@"   Total frames: %llu", context->total_frames_rendered);
    NSLog(@"   Total callbacks: %llu", context->render_callback_count);
    NSLog(@"   Average CPU load: %.1f%%", context->cpu_load_average * 100.0);
}

// Set sample rate
bool audio_output_set_sample_rate(void* handle, double sample_rate) {
    if (!handle) return false;
    
    mac_audio_context_t* context = (mac_audio_context_t*)handle;
    
    if (context->running) {
        NSLog(@"❌ Cannot change sample rate while audio is running");
        return false;
    }
    
    context->sample_rate = sample_rate;
    context->stream_format.mSampleRate = sample_rate;
    
    OSStatus result = AudioUnitSetProperty(context->output_unit,
                                          kAudioUnitProperty_StreamFormat,
                                          kAudioUnitScope_Input,
                                          0,
                                          &context->stream_format,
                                          sizeof(AudioStreamBasicDescription));
    
    if (result != noErr) {
        print_audio_error(result, "AudioUnitSetProperty (sample rate)");
        return false;
    }
    
    context->current_latency_ms = (double)context->buffer_size_frames / context->sample_rate * 1000.0;
    
    NSLog(@"✅ Sample rate changed to %.0f Hz (latency: %.1f ms)", 
          sample_rate, context->current_latency_ms);
    return true;
}

// Set buffer size
bool audio_output_set_buffer_size(void* handle, uint32_t buffer_size) {
    if (!handle) return false;
    
    mac_audio_context_t* context = (mac_audio_context_t*)handle;
    
    if (context->running) {
        NSLog(@"❌ Cannot change buffer size while audio is running");
        return false;
    }
    
    if (buffer_size < 64 || buffer_size > 4096) {
        NSLog(@"❌ Buffer size must be between 64 and 4096 frames");
        return false;
    }
    
    context->buffer_size_frames = buffer_size;
    context->current_latency_ms = (double)buffer_size / context->sample_rate * 1000.0;
    
    NSLog(@"✅ Buffer size preference set to %u frames (%.1f ms)", 
          buffer_size, context->current_latency_ms);
    return true;
}

// Get sample rate
double audio_output_get_sample_rate(void* handle) {
    if (!handle) return 0.0;
    mac_audio_context_t* context = (mac_audio_context_t*)handle;
    return context->sample_rate;
}

// Get buffer size
uint32_t audio_output_get_buffer_size(void* handle) {
    if (!handle) return 0;
    mac_audio_context_t* context = (mac_audio_context_t*)handle;
    return context->buffer_size_frames;
}

// Get latency
double audio_output_get_latency_ms(void* handle) {
    if (!handle) return 0.0;
    mac_audio_context_t* context = (mac_audio_context_t*)handle;
    return context->current_latency_ms;
}

// Check if running
bool audio_output_is_running(void* handle) {
    if (!handle) return false;
    mac_audio_context_t* context = (mac_audio_context_t*)handle;
    return context->running;
}

// Get CPU load
double audio_output_get_cpu_load(void* handle) {
    if (!handle) return 0.0;
    mac_audio_context_t* context = (mac_audio_context_t*)handle;
    return context->cpu_load_average;
}

// Print statistics
void audio_output_print_stats(void* handle) {
    if (!handle) return;
    
    mac_audio_context_t* context = (mac_audio_context_t*)handle;
    
    double uptime_seconds = 0.0;
    if (context->render_callback_count > 0) {
        uptime_seconds = (double)context->total_frames_rendered / context->sample_rate;
    }
    
    NSLog(@"\n🔊 Audio Output Statistics:");
    NSLog(@"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    NSLog(@"📊 Configuration:");
    NSLog(@"   Sample Rate: %.0f Hz", context->sample_rate);
    NSLog(@"   Buffer Size: %u frames", context->buffer_size_frames);
    NSLog(@"   Latency: %.1f ms", context->current_latency_ms);
    NSLog(@"   Status: %@", context->running ? @"🟢 Running" : @"🔴 Stopped");
    
    NSLog(@"\n⏱️ Performance:");
    NSLog(@"   Uptime: %.1f seconds", uptime_seconds);
    NSLog(@"   Total Frames: %llu", context->total_frames_rendered);
    NSLog(@"   Total Callbacks: %llu", context->render_callback_count);
    NSLog(@"   CPU Load: %.1f%% (peak: %.1f%%)", 
          context->cpu_load_average * 100.0, context->stats.cpu_load_peak * 100.0);
    
    if (context->stats.callback_time_samples > 0) {
        NSLog(@"\n📈 Callback Timing:");
        NSLog(@"   Min: %.3f ms", context->stats.min_callback_time_ms);
        NSLog(@"   Avg: %.3f ms", context->stats.avg_callback_time_ms);
        NSLog(@"   Max: %.3f ms", context->stats.max_callback_time_ms);
    }
    
    NSLog(@"\n⚠️ Issues:");
    NSLog(@"   Underruns: %u", context->underrun_count);
    NSLog(@"   Overruns: %u", context->overrun_count);
}

// CORE AUDIO CALLBACK - The heart of the system
static OSStatus audio_render_callback(void *inRefCon,
                                     AudioUnitRenderActionFlags *ioActionFlags,
                                     const AudioTimeStamp *inTimeStamp,
                                     UInt32 inBusNumber,
                                     UInt32 inNumberFrames,
                                     AudioBufferList *ioData) {
    (void)ioActionFlags;
    (void)inTimeStamp;
    (void)inBusNumber;
    
    mac_audio_context_t* context = (mac_audio_context_t*)inRefCon;
    
    if (!context || !ioData || ioData->mNumberBuffers == 0) {
        return noErr;
    }
    
    uint64_t callback_start = mach_absolute_time();
    context->render_callback_count++;
    
    // Get stereo output buffer
    Float32* stereo_output = (Float32*)ioData->mBuffers[0].mData;
    uint32_t frame_count = inNumberFrames;
    
    // Safety check
    if (frame_count > context->mono_buffer_size) {
        memset(stereo_output, 0, frame_count * 2 * sizeof(Float32));
        context->overrun_count++;
        return noErr;
    }
    
    // Generate mono audio using our synthesis system
    pthread_mutex_lock(&context->audio_mutex);
    generate_audio_block(context->mono_buffer, frame_count, context->sample_rate);
    pthread_mutex_unlock(&context->audio_mutex);
    
    // Convert mono to stereo interleaved with gentle limiting
    for (uint32_t i = 0; i < frame_count; i++) {
        float sample = context->mono_buffer[i];
        
        // Gentle soft limiting
        if (sample > 1.0f) sample = 1.0f;
        else if (sample < -1.0f) sample = -1.0f;
        
        // Duplicate to both channels
        stereo_output[i * 2] = sample;     // Left
        stereo_output[i * 2 + 1] = sample; // Right
    }
    
    // Update statistics
    context->total_frames_rendered += frame_count;
    
    // Performance monitoring
    uint64_t callback_end = mach_absolute_time();
    double callback_time_ms = (double)(callback_end - callback_start) * 
                             context->nanoseconds_per_tick / 1000000.0;
    
    update_performance_stats(context, callback_time_ms);
    
    // CPU load calculation
    double available_time_ms = (double)frame_count / context->sample_rate * 1000.0;
    double current_cpu_load = callback_time_ms / available_time_ms;
    
    // Exponential moving average for CPU load
    const double alpha = 0.1;
    context->cpu_load_average = (1.0 - alpha) * context->cpu_load_average + alpha * current_cpu_load;
    
    // Track peak CPU
    if (current_cpu_load > context->stats.cpu_load_peak) {
        context->stats.cpu_load_peak = current_cpu_load;
    }
    
    // Detect underruns
    if (callback_time_ms > available_time_ms * 0.8) {
        context->underrun_count++;
    }
    
    return noErr;
}

// Configure stream format for stereo float32
static bool configure_stream_format(mac_audio_context_t* context) {
    AudioStreamBasicDescription* format = &context->stream_format;
    
    // Standard stereo interleaved float32 format
    format->mSampleRate = context->sample_rate;
    format->mFormatID = kAudioFormatLinearPCM;
    format->mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format->mFramesPerPacket = 1;
    format->mChannelsPerFrame = 2; // Stereo
    format->mBitsPerChannel = 32;
    format->mBytesPerFrame = format->mChannelsPerFrame * sizeof(Float32);
    format->mBytesPerPacket = format->mBytesPerFrame * format->mFramesPerPacket;
    
    return true;
}

// Set up the Default Output Unit - THE KEY FIX!
static bool setup_default_output_unit(mac_audio_context_t* context) {
    OSStatus result;
    
    // Create component description for DEFAULT OUTPUT (not HAL!)
    AudioComponentDescription description = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,  // THIS is the fix!
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0
    };
    
    // Find the component
    AudioComponent component = AudioComponentFindNext(NULL, &description);
    if (!component) {
        NSLog(@"❌ Failed to find Default Output component");
        return false;
    }
    
    // Create instance
    result = AudioComponentInstanceNew(component, &context->output_unit);
    if (result != noErr) {
        print_audio_error(result, "AudioComponentInstanceNew");
        return false;
    }
    
    // NO ENABLE IO - This was the bug! Default Output doesn't need/support EnableIO
    
    // Set stream format
    result = AudioUnitSetProperty(context->output_unit,
                                 kAudioUnitProperty_StreamFormat,
                                 kAudioUnitScope_Input,
                                 0,
                                 &context->stream_format,
                                 sizeof(AudioStreamBasicDescription));
    
    if (result != noErr) {
        print_audio_error(result, "AudioUnitSetProperty (StreamFormat)");
        return false;
    }
    
    // Set render callback
    AURenderCallbackStruct callback_struct = {
        .inputProc = audio_render_callback,
        .inputProcRefCon = context
    };
    
    result = AudioUnitSetProperty(context->output_unit,
                                 kAudioUnitProperty_SetRenderCallback,
                                 kAudioUnitScope_Input,
                                 0,
                                 &callback_struct,
                                 sizeof(AURenderCallbackStruct));
    
    if (result != noErr) {
        print_audio_error(result, "AudioUnitSetProperty (SetRenderCallback)");
        return false;
    }
    
    // Initialize
    result = AudioUnitInitialize(context->output_unit);
    if (result != noErr) {
        print_audio_error(result, "AudioUnitInitialize");
        return false;
    }
    
    NSLog(@"✅ Default Output Audio Unit setup successful");
    return true;
}

// Update performance statistics
static void update_performance_stats(mac_audio_context_t* context, double callback_time_ms) {
    if (callback_time_ms < context->stats.min_callback_time_ms) {
        context->stats.min_callback_time_ms = callback_time_ms;
    }
    if (callback_time_ms > context->stats.max_callback_time_ms) {
        context->stats.max_callback_time_ms = callback_time_ms;
    }
    
    context->stats.callback_time_samples++;
    double alpha = 1.0 / context->stats.callback_time_samples;
    if (alpha < 0.001) alpha = 0.001;
    
    context->stats.avg_callback_time_ms = 
        (1.0 - alpha) * context->stats.avg_callback_time_ms + alpha * callback_time_ms;
}

// Clean up audio unit
static void cleanup_audio_unit(mac_audio_context_t* context) {
    if (context->output_unit) {
        AudioUnitUninitialize(context->output_unit);
        AudioComponentInstanceDispose(context->output_unit);
        context->output_unit = NULL;
    }
}

// Print audio error with context
static void print_audio_error(OSStatus error, const char* operation) {
    char error_string[5];
    *(UInt32*)error_string = CFSwapInt32HostToBig(error);
    error_string[4] = '\0';
    
    bool is_4char = true;
    for (int i = 0; i < 4; i++) {
        if (error_string[i] < 32 || error_string[i] > 126) {
            is_4char = false;
            break;
        }
    }
    
    if (is_4char) {
        NSLog(@"❌ Audio Error in %s: '%s' (%d)", operation, error_string, (int)error);
    } else {
        NSLog(@"❌ Audio Error in %s: %d", operation, (int)error);
    }
    
    // Helpful context for common errors
    switch (error) {
        case kAudioUnitErr_InvalidProperty:
            NSLog(@"   💡 Invalid property - check if property is supported by this unit type");
            break;
        case kAudioUnitErr_InvalidParameter:
            NSLog(@"   💡 Invalid parameter value");
            break;
        case kAudioUnitErr_InvalidElement:
            NSLog(@"   💡 Invalid element/bus index");
            break;
        case kAudioUnitErr_NoConnection:
            NSLog(@"   💡 No connection established");
            break;
    }
}
