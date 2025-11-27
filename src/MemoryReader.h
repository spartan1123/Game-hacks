#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>

// Forward declarations
class ProcessMemory;
class PatternScanner;
class OffsetFinder;
class EntityTracker;

// Memory protection types
enum class MemoryProtection {
    PAGE_NOACCESS = PAGE_NOACCESS,
    PAGE_READONLY = PAGE_READONLY,
    PAGE_READWRITE = PAGE_READWRITE,
    PAGE_WRITECOPY = PAGE_WRITECOPY,
    PAGE_EXECUTE = PAGE_EXECUTE,
    PAGE_EXECUTE_READ = PAGE_EXECUTE_READ,
    PAGE_EXECUTE_READWRITE = PAGE_EXECUTE_READWRITE,
    PAGE_EXECUTE_WRITECOPY = PAGE_EXECUTE_WRITECOPY,
    PAGE_GUARD = PAGE_GUARD,
    PAGE_NOCACHE = PAGE_NOCACHE,
    PAGE_WRITECOMBINE = PAGE_WRITECOMBINE
};

// Memory region information
struct MemoryRegion {
    ULONG_PTR BaseAddress;
    ULONG_PTR Size;
    MemoryProtection Protection;
    DWORD State;
    DWORD Type;
    std::string ModuleName;
};

// Pattern scan result
struct PatternResult {
    ULONG_PTR Address;
    ULONG_PTR Offset;
    std::string ModuleName;
};

// Multi-level pointer chain
struct PointerChain {
    ULONG_PTR BaseAddress;
    std::vector<ULONG> Offsets;
    ULONG_PTR ResolvedAddress;
};

// Entity information for ESP/Aimbot
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
    ULONG EntityId;
    float BoneMatrix[3][4]; // 3x4 bone matrix for aimbot
    float Distance;
};

// View matrix for ESP rendering
struct ViewMatrix {
    float matrix[4][4];
    
    bool WorldToScreen(const float worldPos[3], float screenPos[2], int screenWidth, int screenHeight) const;
};

// Game state information
struct GameState {
    bool InGame;
    bool InMenu;
    ULONG RoundNumber;
    ULONG LocalPlayerTeam;
    ULONG_PTR LocalPlayerAddress;
    ViewMatrix ViewMatrix;
    std::vector<EntityInfo> Entities;
};

class KernelMemoryReader {
public:
    KernelMemoryReader();
    ~KernelMemoryReader();

    bool Initialize();
    void Shutdown();

    // Driver communication
    bool OpenDriver();
    void CloseDriver();
    bool IsDriverLoaded() const { return m_DriverHandle != INVALID_HANDLE_VALUE; }

    // Process operations
    DWORD FindProcessId(const std::string& processName);
    bool AttachToProcess(DWORD processId);
    bool AttachToProcess(const std::string& processName);
    ULONG_PTR GetProcessBaseAddress(DWORD processId);
    ULONG_PTR GetModuleBaseAddress(const std::string& moduleName);

    // Memory operations
    bool ReadMemory(ULONG_PTR address, PVOID buffer, SIZE_T size);
    bool WriteMemory(ULONG_PTR address, const PVOID buffer, SIZE_T size);
    template<typename T>
    bool Read(ULONG_PTR address, T& value) {
        return ReadMemory(address, &value, sizeof(T));
    }
    template<typename T>
    bool Write(ULONG_PTR address, const T& value) {
        return WriteMemory(address, &value, sizeof(T));
    }

    // Memory region enumeration
    std::vector<MemoryRegion> EnumerateMemoryRegions(DWORD processId);
    std::vector<MemoryRegion> EnumerateModules(DWORD processId);

    // Pattern scanning
    std::vector<PatternResult> ScanPattern(const std::string& pattern, const std::string& mask, 
                                          ULONG_PTR startAddress = 0, ULONG_PTR endAddress = 0);
    std::vector<PatternResult> ScanPatternInModule(const std::string& pattern, const std::string& mask,
                                                   const std::string& moduleName);

    // Pointer resolution
    ULONG_PTR ResolvePointerChain(const PointerChain& chain);
    ULONG_PTR ResolvePointerChain(ULONG_PTR baseAddress, const std::vector<ULONG>& offsets);

    // Offset finding
    ULONG_PTR FindOffset(const std::string& pattern, const std::string& mask, ULONG offsetFromPattern = 0);
    ULONG_PTR FindOffsetInModule(const std::string& pattern, const std::string& mask,
                                 const std::string& moduleName, ULONG offsetFromPattern = 0);

    // Entity tracking
    bool UpdateEntityList(std::vector<EntityInfo>& entities);
    bool GetLocalPlayer(EntityInfo& localPlayer);
    bool GetViewMatrix(ViewMatrix& viewMatrix);
    bool UpdateGameState(GameState& gameState);

    // ESP/Aimbot helpers
    bool CalculateESPBox(const EntityInfo& entity, const ViewMatrix& viewMatrix,
                       int screenWidth, int screenHeight, float box[4]);
    bool WorldToScreen(const float worldPos[3], const ViewMatrix& viewMatrix,
                      int screenWidth, int screenHeight, float screenPos[2]);
    float CalculateDistance(const float pos1[3], const float pos2[3]);
    bool IsEntityVisible(const EntityInfo& entity, const EntityInfo& localPlayer);

    // Bone matrix extraction for aimbot
    bool GetBoneMatrix(ULONG_PTR entityAddress, ULONG boneIndex, float matrix[3][4]);

    // Anti-cheat bypass helpers
    bool BypassPageGuard(ULONG_PTR address);
    bool BypassCopyOnWrite(ULONG_PTR address);
    bool HideDriver();
    bool UnhookNtdll();

private:
    HANDLE m_DriverHandle;
    DWORD m_AttachedProcessId;
    ULONG_PTR m_ProcessBaseAddress;
    std::unique_ptr<PatternScanner> m_PatternScanner;
    std::unique_ptr<OffsetFinder> m_OffsetFinder;
    std::unique_ptr<EntityTracker> m_EntityTracker;

    bool SendIoctl(DWORD ioctlCode, PVOID inputBuffer, DWORD inputSize,
                   PVOID outputBuffer, DWORD outputSize, PDWORD bytesReturned);
    std::string PatternStringToBytes(const std::string& pattern);
    std::string CreateMaskFromPattern(const std::string& pattern);
};
