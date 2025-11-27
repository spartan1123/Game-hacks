# Technical Documentation

## Kernel Driver Architecture

### Driver Entry Point

The driver uses WDF (Windows Driver Framework) for improved stability and easier development:

```c
NTSTATUS DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
)
```

### Device Creation

The driver creates a device object accessible from user-space:
- Device Name: `\Device\GameMemoryReader`
- Symbolic Link: `\DosDevices\GameMemoryReader`
- User-space path: `\\.\GameMemoryReader`

### IOCTL Communication

All communication between user-space and kernel-space uses IOCTL codes:

```c
#define IOCTL_READ_MEMORY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
```

**Security**: Uses `METHOD_BUFFERED` for automatic buffer validation.

## Memory Reading Implementation

### Process Attachment

```c
NTSTATUS ReadProcessMemory(ULONG ProcessId, ULONG_PTR Address, PVOID Buffer, ULONG Size)
{
    PEPROCESS process = GetProcessByPid(ProcessId);
    KeStackAttachProcess(process, &apcState);
    // Read memory directly
    RtlCopyMemory(Buffer, (PVOID)Address, Size);
    KeUnstackDetachProcess(&apcState);
}
```

**Key Points**:
- Uses `KeStackAttachProcess` to attach to target process context
- Direct memory copy in kernel mode
- Exception handling for invalid addresses

### Memory Writing

Uses MDL (Memory Descriptor List) for safe writing:

```c
mdl = IoAllocateMdl((PVOID)Address, Size, FALSE, FALSE, NULL);
MmProbeAndLockPages(mdl, KernelMode, IoModifyAccess);
mappedAddress = MmMapLockedPagesSpecifyCache(mdl, KernelMode, MmNonCached, NULL, FALSE, NormalPagePriority);
RtlCopyMemory(mappedAddress, Buffer, Size);
```

## Pattern Scanning Algorithm

### Algorithm Overview

1. **Memory Region Enumeration**: Identify readable memory regions
2. **Chunk Reading**: Read memory in 4KB chunks
3. **Pattern Matching**: Compare against pattern with mask
4. **Result Collection**: Store matching addresses

### Pattern Format

- **Pattern**: Hex bytes separated by spaces (e.g., "48 8B 05 ?? ?? ?? ??")
- **Mask**: FF for match, 00 for wildcard (e.g., "FF FF FF 00 00 00 00")

### Implementation

```cpp
bool MatchPattern(const BYTE* data, const BYTE* pattern, const BYTE* mask, SIZE_T size)
{
    for (SIZE_T i = 0; i < size; i++) {
        if (mask[i] == 0xFF && data[i] != pattern[i]) {
            return false;
        }
    }
    return true;
}
```

## Multi-Level Pointer Resolution

### Algorithm

1. Read base address
2. Dereference to get next address
3. Add offset
4. Repeat for each level

### Implementation

```c
NTSTATUS ResolveMultiLevelPointer(...)
{
    ULONG_PTR currentAddress = BaseAddress;
    for (ULONG i = 0; i < OffsetCount; i++) {
        currentAddress = *(PULONG_PTR)currentAddress;
        currentAddress += Offsets[i];
    }
    *Result = currentAddress;
}
```

### Example

```
Base: 0x12345678
Level 1: Read 0x12345678 -> 0xABCD0000, add 0x10 -> 0xABCD0010
Level 2: Read 0xABCD0010 -> 0xEF000000, add 0x20 -> 0xEF000020
Result: 0xEF000020
```

## Entity Tracking System

### Entity Structure

```cpp
struct EntityInfo {
    ULONG_PTR EntityAddress;
    float Position[3];
    float ViewAngles[3];
    float Health;
    float MaxHealth;
    bool IsAlive;
    bool IsVisible;
    bool IsEnemy;
    ULONG TeamId;
    float BoneMatrix[3][4];
    float Distance;
};
```

### Entity List Traversal

1. Read entity list base address
2. Iterate through entity array
3. Validate each entity
4. Read entity data structure
5. Calculate derived values (distance, visibility)

### ESP Box Calculation

```cpp
bool CalculateESPBox(const EntityInfo& entity, const ViewMatrix& viewMatrix,
                   int screenWidth, int screenHeight, float box[4])
{
    float top[2], bottom[2];
    float topPos[3] = { entity.Position[0], entity.Position[1], entity.Position[2] + 70.0f };
    float bottomPos[3] = { entity.Position[0], entity.Position[1], entity.Position[2] };
    
    WorldToScreen(topPos, viewMatrix, screenWidth, screenHeight, top);
    WorldToScreen(bottomPos, viewMatrix, screenWidth, screenHeight, bottom);
    
    float height = bottom[1] - top[1];
    float width = height / 2.0f;
    
    box[0] = top[0] - width / 2.0f;  // x
    box[1] = top[1];                  // y
    box[2] = width;                   // width
    box[3] = height;                  // height
}
```

## View Matrix Transformation

### World-to-Screen Conversion

```cpp
bool WorldToScreen(const float worldPos[3], const ViewMatrix& viewMatrix,
                  int screenWidth, int screenHeight, float screenPos[2])
{
    float w = matrix[3][0] * worldPos[0] + matrix[3][1] * worldPos[1] + 
              matrix[3][2] * worldPos[2] + matrix[3][3];
    
    if (w < 0.001f) return false; // Behind camera
    
    float x = matrix[0][0] * worldPos[0] + matrix[0][1] * worldPos[1] + 
              matrix[0][2] * worldPos[2] + matrix[0][3];
    float y = matrix[1][0] * worldPos[0] + matrix[1][1] * worldPos[1] + 
              matrix[1][2] * worldPos[2] + matrix[1][3];
    
    screenPos[0] = (screenWidth / 2.0f) + (0.5f * x / w * screenWidth + 0.5f);
    screenPos[1] = (screenHeight / 2.0f) - (0.5f * y / w * screenHeight + 0.5f);
    
    return true;
}
```

## Vulnerable Driver Exploitation

### Driver Enumeration

```cpp
bool CheckDriverAvailable(const std::string& driverName)
{
    SC_HANDLE scManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    SC_HANDLE service = OpenServiceA(scManager, driverName.c_str(), SERVICE_QUERY_STATUS);
    bool available = (service != NULL);
    // Cleanup...
    return available;
}
```

### IOCTL Discovery

The exploit attempts common IOCTL codes used by vulnerable drivers:

```cpp
std::vector<DWORD> ioctls = {
    0x80002000, 0x80002004, 0x80002008,  // Intel PPM
    0x9C402040, 0x9C402044, 0x9C402048,  // AWEAlloc
    0x22001C, 0x220020, 0x220024         // MSI Afterburner
};
```

### Memory Operations via Exploit

```cpp
bool ReadMemoryViaExploit(ULONG_PTR address, PVOID buffer, SIZE_T size)
{
    struct {
        ULONG_PTR Address;
        ULONG Size;
    } request = { address, (ULONG)size };
    
    // Try each IOCTL until one works
    for (DWORD ioctl : ioctls) {
        if (DeviceIoControl(m_DriverHandle, ioctl, &request, sizeof(request),
                           buffer, (DWORD)size, &bytesReturned, NULL)) {
            return bytesReturned == size;
        }
    }
    return false;
}
```

## Anti-Cheat Bypass Techniques

### Page Guard Bypass

```cpp
bool BypassPageGuard(ULONG_PTR address)
{
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery((LPCVOID)address, &mbi, sizeof(mbi));
    
    DWORD oldProtect;
    return VirtualProtect((LPVOID)address, mbi.RegionSize, 
                         PAGE_EXECUTE_READWRITE, &oldProtect) != FALSE;
}
```

### Copy-on-Write Bypass

```cpp
bool BypassCopyOnWrite(ULONG_PTR address)
{
    // Force write to trigger copy-on-write
    return WriteMemory(address, &address, sizeof(ULONG_PTR));
}
```

## Offset Discovery Methodology

### Pattern-Based Discovery

1. **Identify Target**: Determine what offset to find (e.g., entity list)
2. **Find Pattern**: Locate unique byte pattern in memory
3. **Calculate Offset**: Determine offset from pattern match
4. **Validate**: Verify offset works across game versions

### Common Patterns

- **Entity List**: `48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ??`
- **Local Player**: `48 8B 05 ?? ?? ?? ?? 48 85 C0 75 ??`
- **View Matrix**: `F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 45`

### Cross-Reference Analysis

Compare patterns across multiple game instances to find consistent offsets.

## Error Handling

### Kernel-Mode Exceptions

```c
__try {
    // Kernel operations
    ProbeForRead((PVOID)Address, Size, 1);
    RtlCopyMemory(Buffer, (PVOID)Address, Size);
}
__except(EXCEPTION_EXECUTE_HANDLER) {
    status = GetExceptionCode();
    return status;
}
```

### User-Mode Validation

```cpp
if (m_DriverHandle == INVALID_HANDLE_VALUE || m_AttachedProcessId == 0) {
    return false;
}
```

## Performance Considerations

### Memory Scanning Optimization

- Scan only committed memory regions
- Use larger read chunks (4KB+)
- Skip non-readable regions
- Cache frequently accessed addresses

### Entity Update Optimization

- Update entities only when needed
- Cache local player data
- Batch memory reads
- Use multi-threading for independent operations

## Security Considerations

### Driver Signing

- Development: Test signing certificate
- Production: Proper code signing certificate required
- Kernel-mode code requires elevated privileges

### Access Control

- Driver checks process permissions
- IOCTL validation prevents unauthorized access
- Memory operations validate addresses

## Debugging

### Kernel Debugging

Use WinDbg with kernel debugging:
```
kd> bp GameMemoryReader!ReadProcessMemory
kd> g
```

### User-Space Debugging

Standard Visual Studio debugging works for user-space application.

## Future Enhancements

- Raycast-based visibility detection
- Advanced aimbot algorithms
- Automatic offset updates
- Multi-process support
- GUI overlay rendering
