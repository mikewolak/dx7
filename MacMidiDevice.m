//
//  MacMidiDevice.m
//  MIDI SDS Command Line Tool - macOS Platform Implementation
//

#import <Foundation/Foundation.h>
#import <CoreMIDI/CoreMIDI.h>
#include "midi_manager.h"

// macOS-specific MIDI device context
typedef struct {
    MIDIClientRef client;
    MIDIPortRef input_port;
    MIDIPortRef output_port;
    MIDIEndpointRef input_endpoint;
    MIDIEndpointRef output_endpoint;
    midi_input_callback_t input_callback;
    void *callback_context;
    bool initialized;
} mac_midi_context_t;

// Global context for the platform
static mac_midi_context_t g_mac_midi_context = {0};

// C callback for CoreMIDI input
static void mac_midi_input_callback(const MIDIPacketList *packetList, 
                                   void *readProcRefCon, 
                                   void *srcConnRefCon) {
    (void)readProcRefCon;  // Unused parameter
    (void)srcConnRefCon;   // Unused parameter
    
    if (!g_mac_midi_context.input_callback || !packetList) {
        return;
    }
    
    const MIDIPacket *packet = &packetList->packet[0];
    for (UInt32 i = 0; i < packetList->numPackets; i++) {
        if (packet->length > 0) {
            g_mac_midi_context.input_callback(packet->data, 
                                             packet->length,
                                             packet->timeStamp,
                                             g_mac_midi_context.callback_context);
        }
        packet = MIDIPacketNext(packet);
    }
}

// C callback for CoreMIDI notifications
static void mac_midi_notify_callback(const MIDINotification *notification, 
                                     void *refCon) {
    (void)refCon; // Unused
    
    // Handle MIDI system notifications if needed
    switch (notification->messageID) {
        case kMIDIMsgSetupChanged:
        case kMIDIMsgObjectAdded:
        case kMIDIMsgObjectRemoved:
            // Could notify the main application about device changes
            break;
        default:
            break;
    }
}

// Get string property from MIDI object
static NSString* get_midi_string_property(MIDIObjectRef object, CFStringRef property) {
    CFStringRef stringRef = NULL;
    OSStatus status = MIDIObjectGetStringProperty(object, property, &stringRef);
    if (status == noErr && stringRef) {
        return (__bridge_transfer NSString*)stringRef;
    }
    return @"";
}

// Get integer property from MIDI object
static SInt32 get_midi_integer_property(MIDIObjectRef object, CFStringRef property) {
    SInt32 value = 0;
    MIDIObjectGetIntegerProperty(object, property, &value);
    return value;
}

#pragma mark - Platform Interface Implementation

bool midi_platform_initialize(void) {
    if (g_mac_midi_context.initialized) {
        return true;
    }
    
    // Create MIDI client
    NSString *clientName = @"MidiSDS_CLI_C99";
    OSStatus status = MIDIClientCreate((__bridge CFStringRef)clientName,
                                      mac_midi_notify_callback,
                                      NULL,
                                      &g_mac_midi_context.client);
    
    if (status != noErr) {
        return false;
    }
    
    // Create input port
    NSString *inputPortName = @"MidiSDS_CLI_C99_Input";
    status = MIDIInputPortCreate(g_mac_midi_context.client,
                                (__bridge CFStringRef)inputPortName,
                                mac_midi_input_callback,
                                NULL,
                                &g_mac_midi_context.input_port);
    
    if (status != noErr) {
        MIDIClientDispose(g_mac_midi_context.client);
        return false;
    }
    
    // Create output port
    NSString *outputPortName = @"MidiSDS_CLI_C99_Output";
    status = MIDIOutputPortCreate(g_mac_midi_context.client,
                                 (__bridge CFStringRef)outputPortName,
                                 &g_mac_midi_context.output_port);
    
    if (status != noErr) {
        MIDIPortDispose(g_mac_midi_context.input_port);
        MIDIClientDispose(g_mac_midi_context.client);
        return false;
    }
    
    g_mac_midi_context.initialized = true;
    return true;
}

void midi_platform_shutdown(void) {
    if (!g_mac_midi_context.initialized) {
        return;
    }
    
    // Stop any active input
    if (g_mac_midi_context.input_endpoint) {
        MIDIPortDisconnectSource(g_mac_midi_context.input_port, 
                                g_mac_midi_context.input_endpoint);
    }
    
    // Clean up ports and client
    if (g_mac_midi_context.input_port) {
        MIDIPortDispose(g_mac_midi_context.input_port);
    }
    if (g_mac_midi_context.output_port) {
        MIDIPortDispose(g_mac_midi_context.output_port);
    }
    if (g_mac_midi_context.client) {
        MIDIClientDispose(g_mac_midi_context.client);
    }
    
    memset(&g_mac_midi_context, 0, sizeof(g_mac_midi_context));
}

midi_device_list_t* midi_get_device_list(void) {
    if (!midi_platform_initialize()) {
        return NULL;
    }
    
    ItemCount source_count = MIDIGetNumberOfSources();
    ItemCount dest_count = MIDIGetNumberOfDestinations();
    
    midi_device_list_t *list = malloc(sizeof(midi_device_list_t));
    if (!list) {
        return NULL;
    }
    
    list->input_devices = malloc(source_count * sizeof(midi_device_info_t));
    list->output_devices = malloc(dest_count * sizeof(midi_device_info_t));
    list->input_count = (int)source_count;
    list->output_count = (int)dest_count;
    
    if (!list->input_devices || !list->output_devices) {
        free(list->input_devices);
        free(list->output_devices);
        free(list);
        return NULL;
    }
    
    // Fill input devices
    for (ItemCount i = 0; i < source_count; i++) {
        MIDIEndpointRef source = MIDIGetSource(i);
        midi_device_info_t *device = &list->input_devices[i];
        
        // Get endpoint name
        NSString *name = get_midi_string_property(source, kMIDIPropertyName);
        strncpy(device->name, name.UTF8String, sizeof(device->name) - 1);
        device->name[sizeof(device->name) - 1] = '\0';
        
        // Get device and entity references
        MIDIEntityRef entity = 0;
        MIDIDeviceRef midiDevice = 0;
        MIDIEndpointGetEntity(source, &entity);
        if (entity) {
            MIDIEntityGetDevice(entity, &midiDevice);
        }
        
        // Get manufacturer and model
        if (midiDevice) {
            NSString *manufacturer = get_midi_string_property(midiDevice, kMIDIPropertyManufacturer);
            NSString *model = get_midi_string_property(midiDevice, kMIDIPropertyModel);
            
            strncpy(device->manufacturer, manufacturer.UTF8String, sizeof(device->manufacturer) - 1);
            strncpy(device->model, model.UTF8String, sizeof(device->model) - 1);
            device->manufacturer[sizeof(device->manufacturer) - 1] = '\0';
            device->model[sizeof(device->model) - 1] = '\0';
        } else {
            strcpy(device->manufacturer, "Unknown");
            strcpy(device->model, "Unknown");
        }
        
        // Create display name
        if (strlen(device->manufacturer) > 0 && strlen(device->model) > 0 &&
            strcmp(device->manufacturer, "Unknown") != 0 && strcmp(device->model, "Unknown") != 0) {
            snprintf(device->display_name, sizeof(device->display_name),
                    "%s %s - %s", device->manufacturer, device->model, device->name);
        } else {
            strncpy(device->display_name, device->name, sizeof(device->display_name) - 1);
        }
        device->display_name[sizeof(device->display_name) - 1] = '\0';
        
        // Get other properties
        device->unique_id = (uint32_t)get_midi_integer_property(source, kMIDIPropertyUniqueID);
        device->device_id = get_midi_integer_property(source, kMIDIPropertyDeviceID);
        device->online = !get_midi_integer_property(source, kMIDIPropertyOffline);
        device->external = midiDevice ? !get_midi_integer_property(midiDevice, kMIDIPropertyPrivate) : true;
    }
    
    // Fill output devices
    for (ItemCount i = 0; i < dest_count; i++) {
        MIDIEndpointRef destination = MIDIGetDestination(i);
        midi_device_info_t *device = &list->output_devices[i];
        
        // Get endpoint name
        NSString *name = get_midi_string_property(destination, kMIDIPropertyName);
        strncpy(device->name, name.UTF8String, sizeof(device->name) - 1);
        device->name[sizeof(device->name) - 1] = '\0';
        
        // Get device and entity references
        MIDIEntityRef entity = 0;
        MIDIDeviceRef midiDevice = 0;
        MIDIEndpointGetEntity(destination, &entity);
        if (entity) {
            MIDIEntityGetDevice(entity, &midiDevice);
        }
        
        // Get manufacturer and model
        if (midiDevice) {
            NSString *manufacturer = get_midi_string_property(midiDevice, kMIDIPropertyManufacturer);
            NSString *model = get_midi_string_property(midiDevice, kMIDIPropertyModel);
            
            strncpy(device->manufacturer, manufacturer.UTF8String, sizeof(device->manufacturer) - 1);
            strncpy(device->model, model.UTF8String, sizeof(device->model) - 1);
            device->manufacturer[sizeof(device->manufacturer) - 1] = '\0';
            device->model[sizeof(device->model) - 1] = '\0';
        } else {
            strcpy(device->manufacturer, "Unknown");
            strcpy(device->model, "Unknown");
        }
        
        // Create display name
        if (strlen(device->manufacturer) > 0 && strlen(device->model) > 0 &&
            strcmp(device->manufacturer, "Unknown") != 0 && strcmp(device->model, "Unknown") != 0) {
            snprintf(device->display_name, sizeof(device->display_name),
                    "%s %s - %s", device->manufacturer, device->model, device->name);
        } else {
            strncpy(device->display_name, device->name, sizeof(device->display_name) - 1);
        }
        device->display_name[sizeof(device->display_name) - 1] = '\0';
        
        // Get other properties
        device->unique_id = (uint32_t)get_midi_integer_property(destination, kMIDIPropertyUniqueID);
        device->device_id = get_midi_integer_property(destination, kMIDIPropertyDeviceID);
        device->online = !get_midi_integer_property(destination, kMIDIPropertyOffline);
        device->external = midiDevice ? !get_midi_integer_property(midiDevice, kMIDIPropertyPrivate) : true;
    }
    
    return list;
}

void midi_free_device_list(midi_device_list_t *list) {
    if (!list) return;
    
    free(list->input_devices);
    free(list->output_devices);
    free(list);
}

bool midi_platform_open_input_device(int device_index, void **device_handle) {
    if (!g_mac_midi_context.initialized) {
        return false;
    }
    
    ItemCount source_count = MIDIGetNumberOfSources();
    if (device_index < 0 || device_index >= (int)source_count) {
        return false;
    }
    
    MIDIEndpointRef source = MIDIGetSource((ItemCount)device_index);
    if (!source) {
        return false;
    }
    
    g_mac_midi_context.input_endpoint = source;
    *device_handle = (void*)(intptr_t)source; // Store endpoint reference
    
    return true;
}

bool midi_platform_open_output_device(int device_index, void **device_handle) {
    if (!g_mac_midi_context.initialized) {
        return false;
    }
    
    ItemCount dest_count = MIDIGetNumberOfDestinations();
    if (device_index < 0 || device_index >= (int)dest_count) {
        return false;
    }
    
    MIDIEndpointRef destination = MIDIGetDestination((ItemCount)device_index);
    if (!destination) {
        return false;
    }
    
    g_mac_midi_context.output_endpoint = destination;
    *device_handle = (void*)(intptr_t)destination; // Store endpoint reference
    
    return true;
}

void midi_platform_close_device(void *device_handle) {
    if (!device_handle) return;
    
    MIDIEndpointRef endpoint = (MIDIEndpointRef)(intptr_t)device_handle;
    
    // If this is the active input endpoint, disconnect it
    if (endpoint == g_mac_midi_context.input_endpoint) {
        MIDIPortDisconnectSource(g_mac_midi_context.input_port, endpoint);
        g_mac_midi_context.input_endpoint = 0;
    }
    
    // If this is the active output endpoint, clear it
    if (endpoint == g_mac_midi_context.output_endpoint) {
        g_mac_midi_context.output_endpoint = 0;
    }
}

bool midi_platform_start_input(void *device_handle, void *callback_context) {
    if (!device_handle || !g_mac_midi_context.initialized) {
        return false;
    }
    
    MIDIEndpointRef endpoint = (MIDIEndpointRef)(intptr_t)device_handle;
    
    // Store callback context
    g_mac_midi_context.callback_context = callback_context;
    
    // Connect the input source
    OSStatus status = MIDIPortConnectSource(g_mac_midi_context.input_port, endpoint, NULL);
    if (status != noErr) {
        return false;
    }
    
    g_mac_midi_context.input_endpoint = endpoint;
    return true;
}

void midi_platform_stop_input(void *device_handle) {
    if (!device_handle || !g_mac_midi_context.initialized) {
        return;
    }
    
    MIDIEndpointRef endpoint = (MIDIEndpointRef)(intptr_t)device_handle;
    
    // Disconnect the input source
    MIDIPortDisconnectSource(g_mac_midi_context.input_port, endpoint);
    
    if (endpoint == g_mac_midi_context.input_endpoint) {
        g_mac_midi_context.input_endpoint = 0;
    }
}

bool midi_platform_send_data(void *device_handle, const uint8_t *data, size_t length) {
    if (!device_handle || !data || length == 0 || !g_mac_midi_context.initialized) {
        return false;
    }
    
    MIDIEndpointRef endpoint = (MIDIEndpointRef)(intptr_t)device_handle;
    
    // Create MIDI packet list
    Byte packetBuffer[1024];
    MIDIPacketList *packetList = (MIDIPacketList*)packetBuffer;
    MIDIPacket *packet = MIDIPacketListInit(packetList);
    
    // Add data to packet (with timestamp 0 for immediate delivery)
    MIDITimeStamp timestamp = 0;
    packet = MIDIPacketListAdd(packetList, sizeof(packetBuffer), packet, 
                              timestamp, (UInt16)length, data);
    if (!packet) {
        return false;
    }
    
    // Send packet
    OSStatus status = MIDISend(g_mac_midi_context.output_port, endpoint, packetList);
    return (status == noErr);
}

// Set input callback - called by the cross-platform midi_manager
void midi_platform_set_input_callback(midi_input_callback_t callback) {
    g_mac_midi_context.input_callback = callback;
}
