#pragma once

#include "MemoryReader.h"
#include <vector>
#include <string>

class PatternScanner {
public:
    PatternScanner(KernelMemoryReader* memoryReader);
    ~PatternScanner();

    // Pattern scanning with wildcards
    std::vector<ULONG_PTR> Scan(const std::string& pattern, const std::string& mask,
                                ULONG_PTR startAddress, ULONG_PTR endAddress);
    
    // Scan entire module
    std::vector<ULONG_PTR> ScanModule(const std::string& moduleName, const std::string& pattern, const std::string& mask);
    
    // Scan multiple patterns at once
    std::vector<ULONG_PTR> ScanMultiple(const std::vector<std::pair<std::string, std::string>>& patterns,
                                       ULONG_PTR startAddress, ULONG_PTR endAddress);
    
    // Find unique pattern (returns first match)
    ULONG_PTR FindUnique(const std::string& pattern, const std::string& mask,
                        ULONG_PTR startAddress, ULONG_PTR endAddress);
    
    // Pattern validation
    bool ValidatePattern(const std::string& pattern, const std::string& mask);

private:
    KernelMemoryReader* m_MemoryReader;
    
    std::string PatternToBytes(const std::string& pattern);
    std::string CreateMask(const std::string& pattern);
    bool MatchPattern(const BYTE* data, const BYTE* pattern, const BYTE* mask, SIZE_T size);
};
