# 🎹 Real-time MIDI Play Mode Guide

## 🚀 **Revolutionary Real-time Synthesis**

Your DX7 synthesizer now features **professional real-time MIDI input** with **low-latency Core Audio output**, transforming it into a complete virtual instrument capable of **live performance** and **studio recording**.

---

## ⚡ **Quick Start Guide**

### **1. List Available MIDI Devices**
```bash
# Discover all connected MIDI devices
./dx7synth -m
```

### **2. Start Real-time Play Mode**
```bash
# Basic play mode (uses first available MIDI input)
./dx7synth -p -c 1 epiano.patch

# Specify MIDI input device and channel
./dx7synth -p -i 0 -c 1 epiano.patch
```

### **3. Interactive Controls**
Once in play mode, use these commands:
- **Play your MIDI controller** - Notes, mod wheel, pitch bend, sustain pedal
- **`s` + Enter** - Show real-time statistics
- **`v` + Enter** - Show active voices
- **`h` + Enter** - Show help
- **`q` + Enter** - Quit play mode

---

## 🎛️ **Command Line Options**

### **Real-time Mode Options:**
| Option | Description | Example |
|--------|-------------|---------|
| `-p, --play` | Enable real-time MIDI play mode | `./dx7synth -p epiano.patch` |
| `-i, --midi-input <dev>` | MIDI input device index | `./dx7synth -p -i 0 epiano.patch` |
| `-c, --midi-channel <ch>` | MIDI channel (1-16) | `./dx7synth -p -c 2 epiano.patch` |
| `-s, --samplerate <hz>` | Audio sample rate | `./dx7synth -p -s 48000 epiano.patch` |

---

## 🎵 **MIDI Controller Support**

### **✅ Fully Supported MIDI Messages:**

#### **🎹 Note Messages:**
- **Note On/Off** - Full 0-127 note range with velocity sensitivity
- **Running Status** - Efficient MIDI parsing for low CPU usage
- **Polyphony** - Up to 16 simultaneous voices with intelligent voice stealing

#### **🎛️ Controllers:**
- **Pitch Bend** - ±2 semitones (configurable range)
- **Mod Wheel (CC 1)** - LFO amplitude modulation  
- **Volume (CC 7)** - Master volume control
- **Expression (CC 11)** - Dynamic volume changes
- **Sustain Pedal (CC 64)** - Note sustain with proper release
- **Breath Controller (CC 2)** - Placeholder for future features
- **Foot Controller (CC 4)** - Placeholder for future features

#### **🎚️ Special Functions:**
- **All Notes Off (CC 123)** - Emergency note cutoff
- **All Sound Off (CC 120)** - Immediate silence
- **All Controllers Off (CC 121)** - Reset controller values

#### **🎼 System Messages:**
- **Program Change** - Placeholder for patch switching
- **Channel Pressure** - Aftertouch support placeholder

---

## 🔊 **Audio System Features**

### **⚡ Low-Latency Core Audio:**
- **Professional audio quality** with configurable sample rates
- **Real-time processing** optimized for live performance
- **Automatic buffer management** for smooth playback
- **CPU monitoring** with real-time performance statistics

### **🎚️ Audio Specifications:**
- **Sample Rates**: 8kHz - 192kHz (default: 48kHz)
- **Latency**: Sub-10ms on modern hardware
- **Polyphony**: 16 voices with LRU voice stealing
- **Output**: Mono (easily expandable to stereo)
- **Format**: 32-bit floating point internal processing

---

## 🎼 **Voice Management**

### **🎹 Polyphonic Synthesis:**
```
Maximum Voices: 16 simultaneous notes
Voice Allocation: Least Recently Used (LRU) stealing
Note Tracking: Per-channel with velocity sensitivity
Sustain Handling: Proper pedal support with held note tracking
```

### **🔄 Voice Stealing Algorithm:**
1. **First**: Try to find an inactive voice
2. **Fallback**: Steal oldest active voice (LRU)
3. **Priority**: Preserve sustained notes when possible
4. **Statistics**: Track steal count for performance monitoring

---

## 🎛️ **Real-time Control Mapping**

### **🎹 Performance Controls:**
```bash
# MIDI Input → DX7 Parameter Mapping

Pitch Bend Wheel    → ±2 semitones pitch shift (all operators)
Mod Wheel (CC 1)    → LFO amplitude modulation depth
Volume (CC 7)       → Master output volume (0-127 → 0.0-1.0)
Expression (CC 11)  → Dynamic volume control
Sustain Pedal       → Note sustain with proper release timing
```

### **🎚️ Controller Value Ranges:**
```c
// MIDI to Float Conversion
CC Value 0-127   → 0.0 to 1.0 (unipolar)
CC Value 0-127   → -1.0 to +1.0 (bipolar, e.g., pan)
Pitch Bend       → -1.0 to +1.0 (±2 semitones)
Note Velocity    → 0.0 to 1.0 (amplitude scaling)
```

---

## 📊 **Real-time Monitoring**

### **🔍 Statistics Display (`s` command):**
```
🎹 MIDI System Statistics:
   Active voices: 3/16
   Notes played: 47
   Voice steals: 2
   MIDI errors: 0
   Channel: 1
   Pitch bend: 0.125
   Mod wheel: 0.750
   Volume: 1.000
   Sustain: OFF
```

### **🎵 Voice Display (`v` command):**
```
🎵 Active Voices:
   [0] Note:60 Vel:100 Ch:1
   [1] Note:64 Vel:127 Ch:1 (sustained)
   [2] Note:67 Vel:85 Ch:1
```

---

## 🎛️ **Advanced Usage Examples**

### **🎹 Live Performance Setup:**
```bash
# Professional live setup with 48kHz audio
./dx7synth -p -i 0 -c 1 -s 48000 huge_lead.patch

# Multi-timbral setup (different channels)
./dx7synth -p -i 0 -c 1 epiano.patch    # Channel 1: E.Piano
./dx7synth -p -i 0 -c 2 bass1.patch     # Channel 2: Bass (separate instance)
```

### **🎚️ Studio Recording:**
```bash
# High-resolution audio for recording
./dx7synth -p -i 0 -c 1 -s 96000 brass1.patch

# Lower latency for live monitoring
./dx7synth -p -i 0 -c 1 -s 44100 epiano.patch
```

### **🎮 Testing and Development:**
```bash
# Quick patch testing
./dx7synth -p -c 1 test_patch.patch

# Performance monitoring
./dx7synth -p -i 0 -c 1 -s 48000 wobble_bass.patch
# Then use 's' command to monitor CPU usage
```

---

## 🔧 **Troubleshooting**

### **🎹 No Sound Output:**
```bash
# Check MIDI devices
./dx7synth -m

# Verify MIDI input device index
./dx7synth -p -i 0 -c 1 epiano.patch

# Try different audio sample rate
./dx7synth -p -i 0 -c 1 -s 44100 epiano.patch
```

### **🎛️ MIDI Not Responding:**
```bash
# Check MIDI channel matching
./dx7synth -p -i 0 -c 1 epiano.patch  # Try channel 1
./dx7synth -p -i 0 -c 16 epiano.patch # Try channel 16 (omni)

# Verify device connection
./dx7synth -m  # Look for your MIDI controller
```

### **🔊 Audio Dropouts:**
```bash
# Reduce sample rate
./dx7synth -p -i 0 -c 1 -s 44100 epiano.patch

# Check CPU usage with 's' command
# If CPU load > 50%, reduce polyphony or simplify patch
```

### **⚡ High Latency:**
```bash
# Use standard sample rates
./dx7synth -p -i 0 -c 1 -s 48000 epiano.patch  # Professional
./dx7synth -p -i 0 -c 1 -s 44100 epiano.patch  # CD quality

# Check system audio settings for buffer size
```

---

## 🎼 **MIDI Implementation Chart**

### **📋 Supported MIDI Messages:**
| Message Type | Status | Data 1 | Data 2 | Implementation |
|--------------|--------|--------|--------|----------------|
| **Note Off** | 8n | Note | Velocity | ✅ Full support |
| **Note On** | 9n | Note | Velocity | ✅ Full support (vel=0 = note off) |
| **Control Change** | Bn | Controller | Value | ✅ See controller list |
| **Program Change** | Cn | Program | - | 🔄 Placeholder |
| **Channel Pressure** | Dn | Pressure | - | 🔄 Placeholder |
| **Pitch Bend** | En | LSB | MSB | ✅ ±2 semitone range |
| **System Exclusive** | F0 ... F7 | Data | - | 🔄 Future feature |

### **🎛️ Controller Implementation:**
| CC# | Name | Implementation | Range |
|-----|------|----------------|-------|
| **1** | Mod Wheel | ✅ LFO amplitude | 0.0 - 1.0 |
| **2** | Breath | 🔄 Placeholder | 0.0 - 1.0 |
| **4** | Foot | 🔄 Placeholder | 0.0 - 1.0 |
| **7** | Volume | ✅ Master volume | 0.0 - 1.0 |
| **10** | Pan | 🔄 Placeholder | -1.0 - +1.0 |
| **11** | Expression | ✅ Dynamic volume | 0.0 - 1.0 |
| **64** | Sustain | ✅ Note sustain | Off/On |
| **65** | Portamento | 🔄 Placeholder | Off/On |
| **120** | All Sound Off | ✅ Emergency stop | - |
| **121** | Reset Controllers | ✅ Reset all CCs | - |
| **123** | All Notes Off | ✅ Release all notes | - |

---

## 🚀 **Performance Tips**

### **⚡ Optimization:**
- **Use 48kHz** for best balance of quality and performance
- **Monitor CPU load** with `s` command (keep below 50%)
- **Close other audio applications** for best latency
- **Use simple patches** for maximum polyphony

### **🎹 Playing Techniques:**
- **Mod wheel** adds vibrato and movement to sounds
- **Pitch bend** great for expressive leads and basses
- **Sustain pedal** essential for realistic piano/organ sounds
- **Velocity sensitivity** brings patches to life

### **🔧 Hardware Recommendations:**
- **USB MIDI interface** for reliable connection
- **Audio interface** with ASIO drivers for lowest latency
- **SSD storage** for fast patch loading
- **Dedicated audio CPU** for complex patches

---

**🎹 Experience the future of virtual instruments - your DX7 is now a real-time synthesizer! 🎹**

*Connect your MIDI controller and start playing!*
