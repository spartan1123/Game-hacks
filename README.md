# Kernel-Level Game Memory Reader

Advanced Windows kernel-mode memory reading application with automatic offset discovery for game modification research.

## ⚠️ WARNING

**This software is for educational and research purposes only.**

Using kernel-level drivers to access game memory may:
- Violate game terms of service
- Result in permanent account bans
- Have legal consequences
- Compromise system security

**Use only in authorized testing environments or for legitimate security research.**

## Features

### Kernel-Level Access
- Windows Driver Framework (WDF) kernel driver
- Direct Kernel Object Manipulation (DKOM)
- Physical memory mapping
- Memory Descriptor List (MDL) operations
- Vulnerable driver exploitation support

### Memory Operations
- Process memory reading/writing
- Pattern-based memory scanning
- Multi-level pointer resolution
- Memory region enumeration
- Module base address detection

### Offset Discovery
- Automatic pattern scanning
- Multi-level pointer chain resolution
- Offset database storage/loading
- Game-specific offset detection
- Cross-reference analysis

### Game Features
- Entity list traversal
- ESP (Extra Sensory Perception) box calculation
- Aimbot target detection
- View matrix extraction
- Bone matrix reading
- Health/position/team detection

### Anti-Cheat Bypass
- Page guard bypass
- Copy-on-write bypass
- Driver hiding techniques
- NTDLL unhooking support

## Architecture

```
┌─────────────────────────────────────────┐
│     User-Space Application              │
│  (main.cpp, MemoryReader.cpp)           │
└──────────────┬──────────────────────────┘
               │ IOCTL Communication
               ▼
┌─────────────────────────────────────────┐
│     Kernel Driver                       │
│  (kernel_driver.c)                      │
│  - Memory Read/Write                    │
│  - Pattern Scanning                     │
│  - Pointer Resolution                   │
└──────────────┬──────────────────────────┘
               │ Kernel APIs
               ▼
┌─────────────────────────────────────────┐
│     Target Process Memory               │
└─────────────────────────────────────────┘
```

## Components

### Core Classes

- **KernelMemoryReader**: Main interface for memory operations
- **PatternScanner**: Signature-based pattern scanning
- **OffsetFinder**: Automatic offset discovery
- **EntityTracker**: Entity list management and game state
- **VulnerableDriverExploit**: Exploitation of vulnerable drivers

### Driver Communication

The driver exposes the following IOCTL codes:
- `IOCTL_READ_MEMORY`: Read process memory
- `IOCTL_WRITE_MEMORY`: Write process memory
- `IOCTL_SCAN_PATTERN`: Scan memory for patterns
- `IOCTL_RESOLVE_POINTER`: Resolve multi-level pointers
- `IOCTL_GET_PROCESS_BASE`: Get process base address
- `IOCTL_MAP_PHYSICAL_MEMORY`: Map physical memory

## Usage

### Basic Example

```cpp
#include "MemoryReader.h"

KernelMemoryReader reader;
reader.Initialize();
reader.AttachToProcess("csgo.exe");

// Read memory
int health = 0;
reader.Read(0x12345678, health);

// Scan pattern
auto results = reader.ScanPattern("48 8B 05 ?? ?? ?? ??", "FF FF FF 00 00 00 00");

// Get entities
std::vector<EntityInfo> entities;
reader.UpdateEntityList(entities);
```

### Pattern Scanning

```cpp
// Scan for pattern with wildcards
std::string pattern = "48 8B 05 ?? ?? ?? ?? 48 85 C0";
std::string mask = "FF FF FF 00 00 00 00 FF FF FF";
auto results = reader.ScanPattern(pattern, mask);
```

### Pointer Resolution

```cpp
// Resolve multi-level pointer
PointerChain chain;
chain.BaseAddress = 0x12345678;
chain.Offsets = { 0x10, 0x20, 0x30 };
ULONG_PTR resolved = reader.ResolvePointerChain(chain);
```

### Entity Tracking

```cpp
// Update game state
GameState gameState;
reader.UpdateGameState(gameState);

// Get enemies
for (const auto& entity : gameState.Entities) {
    if (entity.IsEnemy && entity.IsAlive) {
        // Process enemy entity
    }
}
```

## Vulnerable Driver Support

The application can exploit known vulnerable drivers as an alternative to a custom driver:

- Intel PPM Driver (ppm.sys)
- AWEAlloc Driver
- MSI Afterburner (RTCore64.sys)
- Gigabyte AORUS (GLCKIO2.sys)
- ASUS GPU Tweak (ATKIO.sys)

## Build Instructions

See [BUILD.md](BUILD.md) for detailed build instructions.

Quick start:
```bash
# User-space application
mkdir build && cd build
cmake ..
cmake --build . --config Release

# Kernel driver (requires WDK)
cd driver
build -cZ
```

## Technical Details

### Memory Protection Bypass

The driver implements several techniques to bypass memory protection:
- **PAGE_GUARD**: Removed via VirtualProtect
- **COPY_ON_WRITE**: Forced write to trigger copy
- **Kernel-mode access**: Direct memory access bypasses user-mode protections

### Pattern Scanning Algorithm

1. Enumerate memory regions
2. Read memory in chunks
3. Compare against pattern with mask
4. Return matching addresses

### Pointer Resolution

1. Read base address
2. Dereference pointer
3. Add offset
4. Repeat for each level
5. Return final address

## Supported Game Engines

The application includes patterns and detection for:
- Unity Engine
- Unreal Engine
- Source Engine
- Custom engines (via pattern configuration)

## Offset Database

Offsets can be saved/loaded from JSON files:
```json
{
  "EntityList": "0x12345678",
  "LocalPlayer": "0x1234567C",
  "ViewMatrix": "0x12345680"
}
```

## Legal Disclaimer

This software is provided for educational and research purposes only. The authors are not responsible for:
- Any misuse of this software
- Violations of terms of service
- Legal consequences of use
- System damage or security breaches

Users are solely responsible for ensuring their use complies with applicable laws and regulations.

## License

This project is provided as-is for educational purposes. No warranty is provided.

## Contributing

This is an educational project. Contributions should focus on:
- Code quality improvements
- Documentation enhancements
- Security research methodologies
- Educational examples

## References

- Windows Driver Kit Documentation
- Windows Kernel Programming by Pavel Yosifovich
- Game Hacking by Nick Cano
- Reverse Engineering resources

## Support

For educational questions and technical discussions, please refer to:
- Windows Driver Development forums
- Reverse Engineering communities
- Security research groups

---

**Remember**: Use responsibly and only in authorized environments.
