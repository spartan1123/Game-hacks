#pragma once

#include "MemoryReader.h"
#include <vector>
#include <string>
#include <unordered_map>

struct OffsetInfo {
    std::string Name;
    ULONG_PTR Address;
    ULONG_PTR Offset;
    std::string ModuleName;
    std::string Pattern;
    std::string Description;
};

class OffsetFinder {
public:
    OffsetFinder(KernelMemoryReader* memoryReader);
    ~OffsetFinder();

    // Find offset by pattern
    ULONG_PTR FindOffset(const std::string& pattern, const std::string& mask,
                        const std::string& moduleName = "", ULONG offsetFromPattern = 0);
    
    // Find offset with multiple patterns (returns first match)
    ULONG_PTR FindOffsetMultiple(const std::vector<std::pair<std::string, std::string>>& patterns,
                                 const std::string& moduleName = "", ULONG offsetFromPattern = 0);
    
    // Register known offset
    void RegisterOffset(const std::string& name, ULONG_PTR offset, const std::string& moduleName = "");
    
    // Get registered offset
    ULONG_PTR GetRegisteredOffset(const std::string& name);
    
    // Auto-discover common game offsets
    std::vector<OffsetInfo> AutoDiscoverOffsets();
    
    // Find entity list offset
    ULONG_PTR FindEntityListOffset();
    
    // Find local player offset
    ULONG_PTR FindLocalPlayerOffset();
    
    // Find view matrix offset
    ULONG_PTR FindViewMatrixOffset();
    
    // Find health offset
    ULONG_PTR FindHealthOffset();
    
    // Find position offset
    ULONG_PTR FindPositionOffset();
    
    // Find bone matrix offset
    ULONG_PTR FindBoneMatrixOffset();
    
    // Save offsets to file
    bool SaveOffsets(const std::string& filename);
    
    // Load offsets from file
    bool LoadOffsets(const std::string& filename);

private:
    KernelMemoryReader* m_MemoryReader;
    std::unordered_map<std::string, OffsetInfo> m_RegisteredOffsets;
    
    // Common game patterns (examples)
    struct GamePattern {
        std::string name;
        std::string pattern;
        std::string mask;
        ULONG offset;
    };
    
    std::vector<GamePattern> GetCommonPatterns();
};
