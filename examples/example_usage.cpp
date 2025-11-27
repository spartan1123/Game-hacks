/*
 * Example Usage of Kernel-Level Game Memory Reader
 * 
 * This file demonstrates how to use the various features
 * of the memory reader for game modification research.
 */

#include "../src/MemoryReader.h"
#include <iostream>
#include <vector>

void ExampleBasicMemoryRead()
{
    std::cout << "=== Example: Basic Memory Read ===" << std::endl;
    
    KernelMemoryReader reader;
    if (!reader.Initialize()) {
        std::cout << "Failed to initialize reader!" << std::endl;
        return;
    }
    
    // Attach to target process
    if (!reader.AttachToProcess("csgo.exe")) {
        std::cout << "Failed to attach to process!" << std::endl;
        return;
    }
    
    // Read a value from memory
    int health = 0;
    ULONG_PTR healthAddress = 0x12345678; // Example address
    if (reader.Read(healthAddress, health)) {
        std::cout << "Health: " << health << std::endl;
    }
}

void ExamplePatternScanning()
{
    std::cout << "\n=== Example: Pattern Scanning ===" << std::endl;
    
    KernelMemoryReader reader;
    reader.Initialize();
    reader.AttachToProcess("csgo.exe");
    
    // Scan for entity list pattern
    std::string pattern = "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 ??";
    std::string mask = "FF FF FF 00 00 00 00 FF FF FF FF FF";
    
    auto results = reader.ScanPattern(pattern, mask);
    std::cout << "Found " << results.size() << " matches" << std::endl;
    
    for (const auto& result : results) {
        std::cout << "Match at: 0x" << std::hex << result.Address << std::dec << std::endl;
    }
}

void ExamplePointerResolution()
{
    std::cout << "\n=== Example: Multi-Level Pointer Resolution ===" << std::endl;
    
    KernelMemoryReader reader;
    reader.Initialize();
    reader.AttachToProcess("csgo.exe");
    
    // Resolve pointer chain: Base -> Offset1 -> Offset2 -> Offset3
    PointerChain chain;
    chain.BaseAddress = 0x12345678; // Base address
    chain.Offsets = { 0x10, 0x20, 0x30 }; // Offsets for each level
    
    ULONG_PTR resolved = reader.ResolvePointerChain(chain);
    if (resolved != 0) {
        std::cout << "Resolved address: 0x" << std::hex << resolved << std::dec << std::endl;
    }
}

void ExampleEntityTracking()
{
    std::cout << "\n=== Example: Entity Tracking ===" << std::endl;
    
    KernelMemoryReader reader;
    reader.Initialize();
    reader.AttachToProcess("csgo.exe");
    
    // Update entity list
    std::vector<EntityInfo> entities;
    if (reader.UpdateEntityList(entities)) {
        std::cout << "Found " << entities.size() << " entities" << std::endl;
        
        // Get local player
        EntityInfo localPlayer;
        if (reader.GetLocalPlayer(localPlayer)) {
            std::cout << "Local Player Health: " << localPlayer.Health 
                      << "/" << localPlayer.MaxHealth << std::endl;
        }
        
        // Find enemies
        for (const auto& entity : entities) {
            if (entity.IsEnemy && entity.IsAlive) {
                std::cout << "Enemy found at distance: " << entity.Distance << std::endl;
            }
        }
    }
}

void ExampleESPCalculation()
{
    std::cout << "\n=== Example: ESP Box Calculation ===" << std::endl;
    
    KernelMemoryReader reader;
    reader.Initialize();
    reader.AttachToProcess("csgo.exe");
    
    // Get game state
    GameState gameState;
    if (!reader.UpdateGameState(gameState)) {
        return;
    }
    
    // Calculate ESP boxes for enemies
    int screenWidth = 1920;
    int screenHeight = 1080;
    
    for (const auto& entity : gameState.Entities) {
        if (entity.IsEnemy && entity.IsAlive) {
            float box[4];
            if (reader.CalculateESPBox(entity, gameState.ViewMatrix, 
                                      screenWidth, screenHeight, box)) {
                std::cout << "ESP Box - X: " << box[0] << ", Y: " << box[1]
                          << ", W: " << box[2] << ", H: " << box[3] << std::endl;
            }
        }
    }
}

void ExampleAimbotTarget()
{
    std::cout << "\n=== Example: Aimbot Target Selection ===" << std::endl;
    
    KernelMemoryReader reader;
    reader.Initialize();
    reader.AttachToProcess("csgo.exe");
    
    EntityInfo localPlayer;
    if (!reader.GetLocalPlayer(localPlayer)) {
        return;
    }
    
    std::vector<EntityInfo> entities;
    if (!reader.UpdateEntityList(entities)) {
        return;
    }
    
    // Find closest visible enemy
    EntityInfo bestTarget;
    float bestDistance = FLT_MAX;
    
    for (const auto& entity : entities) {
        if (entity.IsEnemy && entity.IsAlive && entity.IsVisible) {
            if (entity.Distance < bestDistance) {
                bestDistance = entity.Distance;
                bestTarget = entity;
            }
        }
    }
    
    if (bestTarget.EntityAddress != 0) {
        // Calculate aim angles
        float delta[3] = {
            bestTarget.Position[0] - localPlayer.Position[0],
            bestTarget.Position[1] - localPlayer.Position[1],
            bestTarget.Position[2] - localPlayer.Position[2]
        };
        
        float yaw = atan2f(delta[1], delta[0]) * 180.0f / 3.14159f;
        float pitch = -atan2f(delta[2], sqrtf(delta[0] * delta[0] + delta[1] * delta[1])) 
                      * 180.0f / 3.14159f;
        
        std::cout << "Best Target - Distance: " << bestDistance << std::endl;
        std::cout << "Aim Angles - Yaw: " << yaw << ", Pitch: " << pitch << std::endl;
    }
}

void ExampleOffsetDiscovery()
{
    std::cout << "\n=== Example: Automatic Offset Discovery ===" << std::endl;
    
    KernelMemoryReader reader;
    reader.Initialize();
    reader.AttachToProcess("csgo.exe");
    
    OffsetFinder finder(&reader);
    
    // Auto-discover common offsets
    auto offsets = finder.AutoDiscoverOffsets();
    std::cout << "Discovered " << offsets.size() << " offsets:" << std::endl;
    
    for (const auto& offset : offsets) {
        std::cout << "  " << offset.Name << ": 0x" << std::hex << offset.Offset 
                  << std::dec << " (" << offset.ModuleName << ")" << std::endl;
    }
    
    // Save to file
    finder.SaveOffsets("offsets.json");
}

void ExampleVulnerableDriver()
{
    std::cout << "\n=== Example: Vulnerable Driver Exploitation ===" << std::endl;
    
    VulnerableDriverExploit exploit;
    
    // Enumerate available vulnerable drivers
    auto drivers = exploit.EnumerateVulnerableDrivers();
    std::cout << "Found " << drivers.size() << " vulnerable driver(s)" << std::endl;
    
    // Try to exploit Intel PPM driver
    if (exploit.ExploitIntelPPM()) {
        std::cout << "Successfully exploited Intel PPM driver" << std::endl;
        
        // Use exploit for memory operations
        ULONG_PTR testAddress = 0x1000;
        ULONG_PTR value = 0;
        if (exploit.ReadMemoryViaExploit(testAddress, &value, sizeof(value))) {
            std::cout << "Read value: 0x" << std::hex << value << std::dec << std::endl;
        }
    }
}

void ExampleMemoryRegionEnumeration()
{
    std::cout << "\n=== Example: Memory Region Enumeration ===" << std::endl;
    
    KernelMemoryReader reader;
    reader.Initialize();
    
    DWORD processId = reader.FindProcessId("csgo.exe");
    if (processId == 0) {
        return;
    }
    
    // Enumerate memory regions
    auto regions = reader.EnumerateMemoryRegions(processId);
    std::cout << "Found " << regions.size() << " memory regions" << std::endl;
    
    // Enumerate modules
    auto modules = reader.EnumerateModules(processId);
    std::cout << "Found " << modules.size() << " modules:" << std::endl;
    
    for (const auto& module : modules) {
        std::cout << "  " << module.ModuleName << " - Base: 0x" << std::hex 
                  << module.BaseAddress << ", Size: 0x" << module.Size << std::dec << std::endl;
    }
}

int main()
{
    std::cout << "Kernel-Level Game Memory Reader - Usage Examples\n" << std::endl;
    
    // Run examples (comment out ones you don't want to test)
    // ExampleBasicMemoryRead();
    // ExamplePatternScanning();
    // ExamplePointerResolution();
    // ExampleEntityTracking();
    // ExampleESPCalculation();
    // ExampleAimbotTarget();
    // ExampleOffsetDiscovery();
    // ExampleVulnerableDriver();
    // ExampleMemoryRegionEnumeration();
    
    std::cout << "\nExamples completed. Modify main() to run specific examples." << std::endl;
    return 0;
}
