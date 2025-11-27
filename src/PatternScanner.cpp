#include "PatternScanner.h"
#include <sstream>
#include <algorithm>

PatternScanner::PatternScanner(KernelMemoryReader* memoryReader)
    : m_MemoryReader(memoryReader)
{
}

PatternScanner::~PatternScanner()
{
}

std::vector<ULONG_PTR> PatternScanner::Scan(const std::string& pattern, const std::string& mask,
                                            ULONG_PTR startAddress, ULONG_PTR endAddress)
{
    std::vector<ULONG_PTR> results;
    
    if (!m_MemoryReader || !ValidatePattern(pattern, mask)) {
        return results;
    }

    std::string patternBytes = PatternToBytes(pattern);
    std::string patternMask = CreateMask(pattern);

    auto scanResults = m_MemoryReader->ScanPattern(pattern, mask, startAddress, endAddress);
    for (const auto& result : scanResults) {
        results.push_back(result.Address);
    }

    return results;
}

std::vector<ULONG_PTR> PatternScanner::ScanModule(const std::string& moduleName, const std::string& pattern, const std::string& mask)
{
    std::vector<ULONG_PTR> results;
    
    if (!m_MemoryReader || !ValidatePattern(pattern, mask)) {
        return results;
    }

    auto scanResults = m_MemoryReader->ScanPatternInModule(pattern, mask, moduleName);
    for (const auto& result : scanResults) {
        results.push_back(result.Address);
    }

    return results;
}

std::vector<ULONG_PTR> PatternScanner::ScanMultiple(const std::vector<std::pair<std::string, std::string>>& patterns,
                                                    ULONG_PTR startAddress, ULONG_PTR endAddress)
{
    std::vector<ULONG_PTR> results;

    for (const auto& patternPair : patterns) {
        auto patternResults = Scan(patternPair.first, patternPair.second, startAddress, endAddress);
        results.insert(results.end(), patternResults.begin(), patternResults.end());
    }

    return results;
}

ULONG_PTR PatternScanner::FindUnique(const std::string& pattern, const std::string& mask,
                                    ULONG_PTR startAddress, ULONG_PTR endAddress)
{
    auto results = Scan(pattern, mask, startAddress, endAddress);
    return results.empty() ? 0 : results[0];
}

bool PatternScanner::ValidatePattern(const std::string& pattern, const std::string& mask)
{
    if (pattern.empty() || mask.empty()) {
        return false;
    }

    if (pattern.length() != mask.length()) {
        return false;
    }

    return true;
}

std::string PatternScanner::PatternToBytes(const std::string& pattern)
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

std::string PatternScanner::CreateMask(const std::string& pattern)
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

bool PatternScanner::MatchPattern(const BYTE* data, const BYTE* pattern, const BYTE* mask, SIZE_T size)
{
    for (SIZE_T i = 0; i < size; i++) {
        if (mask[i] == 0xFF && data[i] != pattern[i]) {
            return false;
        }
    }
    return true;
}
