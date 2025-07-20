#ifndef MIDI_MANAGER_H
#define MIDI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// MIDI device info structure
typedef struct {
    char name[128];
    char manufacturer[64];
    char model[64];
    char display_name[256];
    uint32_t unique_id;
    int device_id;
    bool online;
    bool external;
} midi_device_info_t;

// MIDI device list structure
typedef struct {
    midi_device_info_t *input_devices;
    midi_device_info_t *output_devices;
    int input_count;
    int output_count;
} midi_device_list_t;

// MIDI callback function type
typedef void (*midi_input_callback_t)(const uint8_t *data, size_t length, 
                                      uint64_t timestamp, void *context);

// Platform-specific MIDI functions
bool midi_platform_initialize(void);
void midi_platform_shutdown(void);
midi_device_list_t* midi_get_device_list(void);
void midi_free_device_list(midi_device_list_t *list);
bool midi_platform_open_input_device(int device_index, void **device_handle);
bool midi_platform_open_output_device(int device_index, void **device_handle);
void midi_platform_close_device(void *device_handle);
bool midi_platform_start_input(void *device_handle, void *callback_context);
void midi_platform_stop_input(void *device_handle);
bool midi_platform_send_data(void *device_handle, const uint8_t *data, size_t length);
void midi_platform_set_input_callback(midi_input_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // MIDI_MANAGER_H
