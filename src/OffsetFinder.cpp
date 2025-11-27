#include "OffsetFinder.h"
#include <fstream>
#include <sstream>
#include <algorithm>

OffsetFinder::OffsetFinder(KernelMemoryReader* memoryReader)
    : m_MemoryReader(memoryReader)
{
}

OffsetFinder::~OffsetFinder()
{
}

ULONG_PTR OffsetFinder::FindOffset(const std::string& pattern, const std::string& mask,
                                   const std::string& moduleName, ULONG offsetFromPattern)
{
    if (!m_MemoryReader) {
        return 0;
    }

    if (moduleName.empty()) {
        return m_MemoryReader->FindOffset(pattern, mask, offsetFromPattern);
    } else {
        return m_MemoryReader->FindOffsetInModule(pattern, mask, moduleName, offsetFromPattern);
    }
}

ULONG_PTR OffsetFinder::FindOffsetMultiple(const std::vector<std::pair<std::string, std::string>>& patterns,
                                           const std::string& moduleName, ULONG offsetFromPattern)
{
    for (const auto& patternPair : patterns) {
        ULONG_PTR offset = FindOffset(patternPair.first, patternPair.second, moduleName, offsetFromPattern);
        if (offset != 0) {
            return offset;
        }
    }
    return 0;
}

void OffsetFinder::RegisterOffset(const std::string& name, ULONG_PTR offset, const std::string& moduleName)
{
    OffsetInfo info;
    info.Name = name;
    info.Address = offset;
    info.Offset = offset;
    info.ModuleName = moduleName;
    m_RegisteredOffsets[name] = info;
}

ULONG_PTR OffsetFinder::GetRegisteredOffset(const std::string& name)
{
    auto it = m_RegisteredOffsets.find(name);
    if (it != m_RegisteredOffsets.end()) {
        return it->second.Offset;
    }
    return 0;
}

std::vector<OffsetInfo> OffsetFinder::AutoDiscoverOffsets()
{
    std::vector<OffsetInfo> discoveredOffsets;
    
    if (!m_MemoryReader) {
        return discoveredOffsets;
    }

    auto patterns = GetCommonPatterns();
    
    for (const auto& gamePattern : patterns) {
        ULONG_PTR address = FindOffset(gamePattern.pattern, gamePattern.mask);
        if (address != 0) {
            OffsetInfo info;
            info.Name = gamePattern.name;
            info.Address = address;
            info.Offset = address - m_MemoryReader->GetProcessBaseAddress(m_MemoryReader->FindProcessId(""));
            info.Pattern = gamePattern.pattern;
            info.Description = "Auto-discovered offset";
            discoveredOffsets.push_back(info);
            RegisterOffset(gamePattern.name, address);
        }
    }

    return discoveredOffsets;
}

ULONG_PTR OffsetFinder::FindEntityListOffset()
{
    // Common entity list patterns for various game engines
    std::vector<std::pair<std::string, std::string>> patterns = {
        { "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B 48 ??", "FF FF FF 00 00 FF FF FF FF FF FF FF FF FF FF FF" }, // Common pattern
        { "48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B D8", "FF FF FF 00 00 00 00 FF FF FF FF FF FF FF" },
        { "8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B F8", "FF FF 00 00 00 00 FF FF FF FF FF FF FF" }
    };
    
    return FindOffsetMultiple(patterns);
}

ULONG_PTR OffsetFinder::FindLocalPlayerOffset()
{
    std::vector<std::pair<std::string, std::string>> patterns = {
        { "48 8B 05 ?? ?? ?? ?? 48 85 C0 75 ?? 48 8B 0D", "FF FF FF 00 00 00 00 FF FF FF FF FF FF FF FF" },
        { "8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? 48 8B 01", "FF FF 00 00 00 00 FF FF FF FF FF FF FF FF" }
    };
    
    return FindOffsetMultiple(patterns);
}

ULONG_PTR OffsetFinder::FindViewMatrixOffset()
{
    std::vector<std::pair<std::string, std::string>> patterns = {
        { "F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 45 ?? 8B 45", "FF FF FF FF 00 00 00 00 FF FF FF FF FF FF FF FF" },
        { "48 8D 0D ?? ?? ?? ?? 0F 10 00", "FF FF FF 00 00 00 00 FF FF FF" }
    };
    
    return FindOffsetMultiple(patterns);
}

ULONG_PTR OffsetFinder::FindHealthOffset()
{
    std::vector<std::pair<std::string, std::string>> patterns = {
        { "F3 0F 10 81 ?? ?? ?? ?? F3 0F 11 45", "FF FF FF FF 00 00 00 00 FF FF FF FF" },
        { "8B 81 ?? ?? ?? ?? 89 45", "FF FF 00 00 00 00 FF FF" }
    };
    
    return FindOffsetMultiple(patterns);
}

ULONG_PTR OffsetFinder::FindPositionOffset()
{
    std::vector<std::pair<std::string, std::string>> patterns = {
        { "F3 0F 10 81 ?? ?? ?? ?? F3 0F 11 85", "FF FF FF FF 00 00 00 00 FF FF FF FF" },
        { "48 8B 81 ?? ?? ?? ?? 48 89 45", "FF FF FF 00 00 00 00 FF FF FF" }
    };
    
    return FindOffsetMultiple(patterns);
}

ULONG_PTR OffsetFinder::FindBoneMatrixOffset()
{
    std::vector<std::pair<std::string, std::string>> patterns = {
        { "48 8B 81 ?? ?? ?? ?? 48 85 C0 74 ?? F3 0F 10 00", "FF FF FF 00 00 00 00 FF FF FF FF FF FF FF FF FF FF" },
        { "F3 0F 10 81 ?? ?? ?? ?? F3 0F 11 45", "FF FF FF FF 00 00 00 00 FF FF FF FF" }
    };
    
    return FindOffsetMultiple(patterns);
}

bool OffsetFinder::SaveOffsets(const std::string& filename)
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& pair : m_RegisteredOffsets) {
        const auto& info = pair.second;
        file << info.Name << "|" << std::hex << info.Offset << "|" << info.ModuleName 
             << "|" << info.Pattern << "|" << info.Description << "\n";
    }

    file.close();
    return true;
}

bool OffsetFinder::LoadOffsets(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(iss, token, '|')) {
            tokens.push_back(token);
        }

        if (tokens.size() >= 2) {
            OffsetInfo info;
            info.Name = tokens[0];
            info.Offset = std::stoull(tokens[1], nullptr, 16);
            if (tokens.size() > 2) info.ModuleName = tokens[2];
            if (tokens.size() > 3) info.Pattern = tokens[3];
            if (tokens.size() > 4) info.Description = tokens[4];
            m_RegisteredOffsets[info.Name] = info;
        }
    }

    file.close();
    return true;
}

std::vector<OffsetFinder::GamePattern> OffsetFinder::GetCommonPatterns()
{
    std::vector<GamePattern> patterns;
    
    // Example patterns - these would be game-specific
    patterns.push_back({ "EntityList", "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ??", "FF FF FF 00 00 00 00 FF FF FF FF", 0 });
    patterns.push_back({ "LocalPlayer", "48 8B 05 ?? ?? ?? ?? 48 85 C0 75 ??", "FF FF FF 00 00 00 00 FF FF FF FF", 0 });
    patterns.push_back({ "ViewMatrix", "F3 0F 10 05 ?? ?? ?? ??", "FF FF FF FF 00 00 00 00", 0 });
    
    return patterns;
}
