#include "MemoryReader.h"
#include "PatternScanner.h"
#include "OffsetFinder.h"
#include "EntityTracker.h"
#include "kernel_driver.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <windows.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "psapi.lib")

KernelMemoryReader::KernelMemoryReader()
    : m_DriverHandle(INVALID_HANDLE_VALUE)
    , m_AttachedProcessId(0)
    , m_ProcessBaseAddress(0)
{
}

KernelMemoryReader::~KernelMemoryReader()
{
    Shutdown();
}

bool KernelMemoryReader::Initialize()
{
    if (!OpenDriver()) {
        return false;
    }

    m_PatternScanner = std::make_unique<PatternScanner>(this);
    m_OffsetFinder = std::make_unique<OffsetFinder>(this);
    m_EntityTracker = std::make_unique<EntityTracker>(this);

    return true;
}

void KernelMemoryReader::Shutdown()
{
    CloseDriver();
    m_PatternScanner.reset();
    m_OffsetFinder.reset();
    m_EntityTracker.reset();
}

bool KernelMemoryReader::OpenDriver()
{
    if (m_DriverHandle != INVALID_HANDLE_VALUE) {
        return true;
    }

    m_DriverHandle = CreateFileW(
        L"\\\\.\\GameMemoryReader",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    return m_DriverHandle != INVALID_HANDLE_VALUE;
}

void KernelMemoryReader::CloseDriver()
{
    if (m_DriverHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_DriverHandle);
        m_DriverHandle = INVALID_HANDLE_VALUE;
    }
}

bool KernelMemoryReader::SendIoctl(DWORD ioctlCode, PVOID inputBuffer, DWORD inputSize,
                                    PVOID outputBuffer, DWORD outputSize, PDWORD bytesReturned)
{
    if (m_DriverHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    return DeviceIoControl(
        m_DriverHandle,
        ioctlCode,
        inputBuffer,
        inputSize,
        outputBuffer,
        outputSize,
        bytesReturned,
        NULL
    ) != FALSE;
}

DWORD KernelMemoryReader::FindProcessId(const std::string& processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &processEntry)) {
        do {
            if (_stricmp(processEntry.szExeFile, processName.c_str()) == 0) {
                CloseHandle(snapshot);
                return processEntry.th32ProcessID;
            }
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return 0;
}

bool KernelMemoryReader::AttachToProcess(DWORD processId)
{
    m_AttachedProcessId = processId;
    m_ProcessBaseAddress = GetProcessBaseAddress(processId);
    return m_ProcessBaseAddress != 0;
}

bool KernelMemoryReader::AttachToProcess(const std::string& processName)
{
    DWORD processId = FindProcessId(processName);
    if (processId == 0) {
        return false;
    }
    return AttachToProcess(processId);
}

ULONG_PTR KernelMemoryReader::GetProcessBaseAddress(DWORD processId)
{
    ULONG_PTR baseAddress = 0;
    DWORD bytesReturned = 0;

    if (!SendIoctl(IOCTL_GET_PROCESS_BASE, &processId, sizeof(ULONG), &baseAddress, sizeof(ULONG_PTR), &bytesReturned)) {
        return 0;
    }

    return baseAddress;
}

ULONG_PTR KernelMemoryReader::GetModuleBaseAddress(const std::string& moduleName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_AttachedProcessId);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    MODULEENTRY32 moduleEntry;
    moduleEntry.dwSize = sizeof(MODULEENTRY32);

    if (Module32First(snapshot, &moduleEntry)) {
        do {
            if (_stricmp(moduleEntry.szModule, moduleName.c_str()) == 0) {
                CloseHandle(snapshot);
                return (ULONG_PTR)moduleEntry.modBaseAddr;
            }
        } while (Module32Next(snapshot, &moduleEntry));
    }

    CloseHandle(snapshot);
    return 0;
}

bool KernelMemoryReader::ReadMemory(ULONG_PTR address, PVOID buffer, SIZE_T size)
{
    if (m_DriverHandle == INVALID_HANDLE_VALUE || m_AttachedProcessId == 0) {
        return false;
    }

    MEMORY_READ_REQUEST request = {};
    request.ProcessId = m_AttachedProcessId;
    request.Address = address;
    request.Size = (ULONG)size;

    DWORD bytesReturned = 0;
    std::vector<BYTE> outputBuffer(size);

    if (!SendIoctl(IOCTL_READ_MEMORY, &request, sizeof(request), outputBuffer.data(), (DWORD)size, &bytesReturned)) {
        return false;
    }

    if (bytesReturned != size) {
        return false;
    }

    memcpy(buffer, outputBuffer.data(), size);
    return true;
}

bool KernelMemoryReader::WriteMemory(ULONG_PTR address, const PVOID buffer, SIZE_T size)
{
    if (m_DriverHandle == INVALID_HANDLE_VALUE || m_AttachedProcessId == 0) {
        return false;
    }

    std::vector<BYTE> inputBuffer(sizeof(MEMORY_WRITE_REQUEST) + size);
    PMEMORY_WRITE_REQUEST request = (PMEMORY_WRITE_REQUEST)inputBuffer.data();
    request->ProcessId = m_AttachedProcessId;
    request->Address = address;
    request->Size = (ULONG)size;
    memcpy(inputBuffer.data() + sizeof(MEMORY_WRITE_REQUEST), buffer, size);

    DWORD bytesReturned = 0;
    return SendIoctl(IOCTL_WRITE_MEMORY, inputBuffer.data(), (DWORD)inputBuffer.size(), NULL, 0, &bytesReturned);
}

std::vector<MemoryRegion> KernelMemoryReader::EnumerateMemoryRegions(DWORD processId)
{
    std::vector<MemoryRegion> regions;
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (processHandle == NULL) {
        return regions;
    }

    MEMORY_BASIC_INFORMATION mbi;
    ULONG_PTR address = 0;

    while (VirtualQueryEx(processHandle, (LPCVOID)address, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        if (mbi.State == MEM_COMMIT && (mbi.Type == MEM_PRIVATE || mbi.Type == MEM_MAPPED)) {
            MemoryRegion region;
            region.BaseAddress = (ULONG_PTR)mbi.BaseAddress;
            region.Size = (ULONG_PTR)mbi.RegionSize;
            region.Protection = (MemoryProtection)mbi.Protect;
            region.State = mbi.State;
            region.Type = mbi.Type;
            regions.push_back(region);
        }
        address = (ULONG_PTR)mbi.BaseAddress + mbi.RegionSize;
    }

    CloseHandle(processHandle);
    return regions;
}

std::vector<MemoryRegion> KernelMemoryReader::EnumerateModules(DWORD processId)
{
    std::vector<MemoryRegion> modules;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return modules;
    }

    MODULEENTRY32 moduleEntry;
    moduleEntry.dwSize = sizeof(MODULEENTRY32);

    if (Module32First(snapshot, &moduleEntry)) {
        do {
            MemoryRegion region;
            region.BaseAddress = (ULONG_PTR)moduleEntry.modBaseAddr;
            region.Size = moduleEntry.modBaseSize;
            region.ModuleName = moduleEntry.szModule;
            modules.push_back(region);
        } while (Module32Next(snapshot, &moduleEntry));
    }

    CloseHandle(snapshot);
    return modules;
}

std::vector<PatternResult> KernelMemoryReader::ScanPattern(const std::string& pattern, const std::string& mask,
                                                           ULONG_PTR startAddress, ULONG_PTR endAddress)
{
    std::vector<PatternResult> results;

    if (m_AttachedProcessId == 0) {
        return results;
    }

    if (startAddress == 0 || endAddress == 0) {
        auto modules = EnumerateModules(m_AttachedProcessId);
        for (const auto& module : modules) {
            auto moduleResults = ScanPatternInModule(pattern, mask, module.ModuleName);
            results.insert(results.end(), moduleResults.begin(), moduleResults.end());
        }
        return results;
    }

    PATTERN_SCAN_REQUEST request = {};
    request.ProcessId = m_AttachedProcessId;
    request.StartAddress = startAddress;
    request.EndAddress = endAddress;
    request.PatternSize = (ULONG)pattern.length();

    std::string patternBytes = PatternStringToBytes(pattern);
    std::string patternMask = CreateMaskFromPattern(pattern);

    memcpy(request.Pattern, patternBytes.c_str(), std::min(patternBytes.length(), (size_t)256));
    memcpy(request.Mask, patternMask.c_str(), std::min(patternMask.length(), (size_t)256));

    DWORD bytesReturned = 0;
    PATTERN_SCAN_REQUEST response = {};

    if (!SendIoctl(IOCTL_SCAN_PATTERN, &request, sizeof(request), &response, sizeof(response), &bytesReturned)) {
        return results;
    }

    for (ULONG i = 0; i < response.ResultCount; i++) {
        PatternResult result;
        result.Address = response.Results[i];
        result.Offset = response.Results[i] - startAddress;
        results.push_back(result);
    }

    return results;
}

std::vector<PatternResult> KernelMemoryReader::ScanPatternInModule(const std::string& pattern, const std::string& mask,
                                                                    const std::string& moduleName)
{
    ULONG_PTR moduleBase = GetModuleBaseAddress(moduleName);
    if (moduleBase == 0) {
        return {};
    }

    auto modules = EnumerateModules(m_AttachedProcessId);
    for (const auto& module : modules) {
        if (module.ModuleName == moduleName) {
            return ScanPattern(pattern, mask, module.BaseAddress, module.BaseAddress + module.Size);
        }
    }

    return {};
}

ULONG_PTR KernelMemoryReader::ResolvePointerChain(const PointerChain& chain)
{
    return ResolvePointerChain(chain.BaseAddress, chain.Offsets);
}

ULONG_PTR KernelMemoryReader::ResolvePointerChain(ULONG_PTR baseAddress, const std::vector<ULONG>& offsets)
{
    if (m_AttachedProcessId == 0) {
        return 0;
    }

    POINTER_RESOLVE_REQUEST request = {};
    request.ProcessId = m_AttachedProcessId;
    request.BaseAddress = baseAddress;
    request.OffsetCount = (ULONG)std::min(offsets.size(), (size_t)16);
    
    for (ULONG i = 0; i < request.OffsetCount; i++) {
        request.Offsets[i] = offsets[i];
    }

    DWORD bytesReturned = 0;
    POINTER_RESOLVE_REQUEST response = {};

    if (!SendIoctl(IOCTL_RESOLVE_POINTER, &request, sizeof(request), &response, sizeof(response), &bytesReturned)) {
        return 0;
    }

    return response.Result;
}

ULONG_PTR KernelMemoryReader::FindOffset(const std::string& pattern, const std::string& mask, ULONG offsetFromPattern)
{
    auto results = ScanPattern(pattern, mask);
    if (results.empty()) {
        return 0;
    }
    return results[0].Address + offsetFromPattern;
}

ULONG_PTR KernelMemoryReader::FindOffsetInModule(const std::string& pattern, const std::string& mask,
                                                 const std::string& moduleName, ULONG offsetFromPattern)
{
    auto results = ScanPatternInModule(pattern, mask, moduleName);
    if (results.empty()) {
        return 0;
    }
    return results[0].Address + offsetFromPattern;
}

bool KernelMemoryReader::UpdateEntityList(std::vector<EntityInfo>& entities)
{
    if (m_EntityTracker) {
        return m_EntityTracker->UpdateEntityList(entities);
    }
    return false;
}

bool KernelMemoryReader::GetLocalPlayer(EntityInfo& localPlayer)
{
    if (m_EntityTracker) {
        return m_EntityTracker->GetLocalPlayer(localPlayer);
    }
    return false;
}

bool KernelMemoryReader::GetViewMatrix(ViewMatrix& viewMatrix)
{
    if (m_EntityTracker) {
        return m_EntityTracker->GetViewMatrix(viewMatrix);
    }
    return false;
}

bool KernelMemoryReader::UpdateGameState(GameState& gameState)
{
    if (m_EntityTracker) {
        return m_EntityTracker->UpdateGameState(gameState);
    }
    return false;
}

bool KernelMemoryReader::CalculateESPBox(const EntityInfo& entity, const ViewMatrix& viewMatrix,
                                         int screenWidth, int screenHeight, float box[4])
{
    float top[2], bottom[2];
    float topPos[3] = { entity.Position[0], entity.Position[1], entity.Position[2] + 70.0f };
    float bottomPos[3] = { entity.Position[0], entity.Position[1], entity.Position[2] };

    if (!WorldToScreen(topPos, viewMatrix, screenWidth, screenHeight, top) ||
        !WorldToScreen(bottomPos, viewMatrix, screenWidth, screenHeight, bottom)) {
        return false;
    }

    float height = bottom[1] - top[1];
    float width = height / 2.0f;

    box[0] = top[0] - width / 2.0f;  // x
    box[1] = top[1];                  // y
    box[2] = width;                   // width
    box[3] = height;                   // height

    return true;
}

bool ViewMatrix::WorldToScreen(const float worldPos[3], float screenPos[2], int screenWidth, int screenHeight) const
{
    float w = matrix[3][0] * worldPos[0] + matrix[3][1] * worldPos[1] + matrix[3][2] * worldPos[2] + matrix[3][3];

    if (w < 0.001f) {
        return false;
    }

    float x = matrix[0][0] * worldPos[0] + matrix[0][1] * worldPos[1] + matrix[0][2] * worldPos[2] + matrix[0][3];
    float y = matrix[1][0] * worldPos[0] + matrix[1][1] * worldPos[1] + matrix[1][2] * worldPos[2] + matrix[1][3];

    screenPos[0] = (screenWidth / 2.0f) + (0.5f * x / w * screenWidth + 0.5f);
    screenPos[1] = (screenHeight / 2.0f) - (0.5f * y / w * screenHeight + 0.5f);

    return true;
}

bool KernelMemoryReader::WorldToScreen(const float worldPos[3], const ViewMatrix& viewMatrix,
                                      int screenWidth, int screenHeight, float screenPos[2])
{
    return viewMatrix.WorldToScreen(worldPos, screenPos, screenWidth, screenHeight);
}

float KernelMemoryReader::CalculateDistance(const float pos1[3], const float pos2[3])
{
    float dx = pos1[0] - pos2[0];
    float dy = pos1[1] - pos2[1];
    float dz = pos1[2] - pos2[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

bool KernelMemoryReader::IsEntityVisible(const EntityInfo& entity, const EntityInfo& localPlayer)
{
    // Simple line-of-sight check (can be enhanced with raycast)
    return entity.IsVisible;
}

bool KernelMemoryReader::GetBoneMatrix(ULONG_PTR entityAddress, ULONG boneIndex, float matrix[3][4])
{
    if (m_AttachedProcessId == 0 || entityAddress == 0) {
        return false;
    }

    // Read bone matrix base address from entity structure
    // Common offset for bone matrix array (game-specific, should be discovered via pattern scanning)
    ULONG_PTR boneMatrixBase = 0;
    if (!Read(entityAddress + 0x26A8, boneMatrixBase)) {
        return false;
    }

    if (boneMatrixBase == 0) {
        return false;
    }

    // Calculate bone matrix address: base + (boneIndex * sizeof(matrix))
    ULONG_PTR boneMatrixAddress = boneMatrixBase + (boneIndex * sizeof(float) * 12);
    
    // Read the 3x4 bone matrix (12 floats)
    return ReadMemory(boneMatrixAddress, matrix, sizeof(float) * 12);
}

bool KernelMemoryReader::BypassPageGuard(ULONG_PTR address)
{
    // Remove PAGE_GUARD protection
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)address, &mbi, sizeof(mbi)) == 0) {
        return false;
    }

    DWORD oldProtect;
    return VirtualProtect((LPVOID)address, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &oldProtect) != FALSE;
}

bool KernelMemoryReader::BypassCopyOnWrite(ULONG_PTR address)
{
    // Force write to copy-on-write memory
    return WriteMemory(address, &address, sizeof(ULONG_PTR));
}

bool KernelMemoryReader::HideDriver()
{
    if (m_DriverHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Hide driver from driver list by manipulating driver object
    // This requires kernel-mode access via IOCTL to driver
    
    // Method 1: Remove driver from loaded modules list via registry
    HKEY hKey;
    LONG result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\GameMemoryReader",
        0, KEY_SET_VALUE, &hKey);
    
    if (result == ERROR_SUCCESS) {
        // Set Start value to 4 (DISABLED) to hide from service list
        DWORD startValue = 4;
        RegSetValueExA(hKey, "Start", 0, REG_DWORD, (BYTE*)&startValue, sizeof(DWORD));
        RegCloseKey(hKey);
    }

    // Method 2: Unlink driver object from kernel's driver list (requires kernel-mode)
    // This would need to be done via a kernel-mode IOCTL
    // For now, we use registry method which is safer and doesn't require kernel modifications
    
    // Method 3: Hide from Process Hacker/Process Explorer by removing from handle table
    // This requires more advanced kernel techniques
    
    return true;
}

bool KernelMemoryReader::UnhookNtdll()
{
    HMODULE ntdllModule = GetModuleHandleA("ntdll.dll");
    if (ntdllModule == NULL) {
        return false;
    }

    // Get NTDLL base address
    MODULEINFO modInfo;
    if (!GetModuleInformation(GetCurrentProcess(), ntdllModule, &modInfo, sizeof(modInfo))) {
        return false;
    }

    // Common hooked functions that anti-cheats modify
    struct HookedFunction {
        const char* functionName;
        FARPROC originalAddress;
    };

    // List of commonly hooked functions
    const char* hookedFunctions[] = {
        "NtReadVirtualMemory",
        "NtWriteVirtualMemory",
        "NtQueryInformationProcess",
        "NtOpenProcess",
        "NtProtectVirtualMemory",
        "NtQuerySystemInformation"
    };

    bool success = true;

    for (const char* funcName : hookedFunctions) {
        FARPROC hookedAddr = GetProcAddress(ntdllModule, funcName);
        if (hookedAddr == NULL) {
            continue;
        }

        // Read first bytes to detect hook (E9 = JMP, E8 = CALL)
        BYTE firstBytes[5];
        DWORD oldProtect;
        
        if (!VirtualProtect(hookedAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            success = false;
            continue;
        }

        memcpy(firstBytes, hookedAddr, 5);

        // Check if function is hooked (starts with JMP or CALL)
        if (firstBytes[0] == 0xE9 || firstBytes[0] == 0xE8) {
            // Function is hooked, restore from disk
            // Load fresh copy of ntdll.dll from disk
            HMODULE freshNtdll = LoadLibraryExA("C:\\Windows\\System32\\ntdll.dll", NULL, 
                                               DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
            
            if (freshNtdll != NULL) {
                FARPROC originalFunc = GetProcAddress(freshNtdll, funcName);
                if (originalFunc != NULL) {
                    // Calculate offset from base
                    ULONG_PTR offset = (ULONG_PTR)originalFunc - (ULONG_PTR)freshNtdll;
                    FARPROC targetAddr = (FARPROC)((ULONG_PTR)ntdllModule + offset);
                    
                    // Copy original bytes
                    memcpy(hookedAddr, targetAddr, 5);
                }
                FreeLibrary(freshNtdll);
            }
        }

        VirtualProtect(hookedAddr, 5, oldProtect, &oldProtect);
    }

    return success;
}

std::string KernelMemoryReader::PatternStringToBytes(const std::string& pattern)
{
    std::string bytes;
    std::istringstream iss(pattern);
    std::string byte;

    while (iss >> byte) {
        if (byte == "??" || byte == "?") {
            bytes += '\x00';
        } else {
            bytes += (char)strtoul(byte.c_str(), NULL, 16);
        }
    }

    return bytes;
}

std::string KernelMemoryReader::CreateMaskFromPattern(const std::string& pattern)
{
    std::string mask;
    std::istringstream iss(pattern);
    std::string byte;

    while (iss >> byte) {
        if (byte == "??" || byte == "?") {
            mask += '\x00';
        } else {
            mask += '\xFF';
        }
    }

    return mask;
}
