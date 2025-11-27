#include "MemoryReader.h"
#include "VulnerableDriverExploit.h"
#include "OffsetFinder.h"
#include "EntityTracker.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

void PrintBanner()
{
    std::cout << R"(
    ╔═══════════════════════════════════════════════════════════╗
    ║     Kernel-Level Game Memory Reader v1.0                  ║
    ║     Advanced Memory Reading & Offset Discovery            ║
    ╚═══════════════════════════════════════════════════════════╝
    )" << std::endl;
}

void PrintMenu()
{
    std::cout << "\n=== Main Menu ===" << std::endl;
    std::cout << "1. Initialize Driver" << std::endl;
    std::cout << "2. Attach to Process" << std::endl;
    std::cout << "3. Scan Memory Pattern" << std::endl;
    std::cout << "4. Find Offsets" << std::endl;
    std::cout << "5. Update Entity List" << std::endl;
    std::cout << "6. ESP Features" << std::endl;
    std::cout << "7. Aimbot Features" << std::endl;
    std::cout << "8. Use Vulnerable Driver" << std::endl;
    std::cout << "9. Auto-Discover Offsets" << std::endl;
    std::cout << "0. Exit" << std::endl;
    std::cout << "Choice: ";
}

bool InitializeDriver(KernelMemoryReader& reader)
{
    std::cout << "\n[+] Initializing kernel driver..." << std::endl;
    
    if (!reader.Initialize()) {
        std::cout << "[-] Failed to initialize driver. Trying vulnerable driver exploit..." << std::endl;
        
        VulnerableDriverExploit exploit;
        auto drivers = exploit.EnumerateVulnerableDrivers();
        
        if (drivers.empty()) {
            std::cout << "[-] No vulnerable drivers found. Please install driver manually." << std::endl;
            return false;
        }
        
        std::cout << "[+] Found " << drivers.size() << " vulnerable driver(s):" << std::endl;
        for (size_t i = 0; i < drivers.size(); i++) {
            std::cout << "  " << (i + 1) << ". " << drivers[i].Name << std::endl;
        }
        
        std::cout << "\n[+] Attempting to exploit first available driver..." << std::endl;
        if (exploit.ExploitGenericDriver(drivers[0])) {
            std::cout << "[+] Successfully exploited " << drivers[0].Name << std::endl;
            return true;
        }
        
        return false;
    }
    
    std::cout << "[+] Driver initialized successfully!" << std::endl;
    return true;
}

bool AttachToProcess(KernelMemoryReader& reader)
{
    std::string processName;
    std::cout << "\nEnter process name (e.g., csgo.exe): ";
    std::cin >> processName;
    
    std::cout << "[+] Searching for process..." << std::endl;
    DWORD processId = reader.FindProcessId(processName);
    
    if (processId == 0) {
        std::cout << "[-] Process not found!" << std::endl;
        return false;
    }
    
    std::cout << "[+] Found process: PID " << processId << std::endl;
    
    if (!reader.AttachToProcess(processId)) {
        std::cout << "[-] Failed to attach to process!" << std::endl;
        return false;
    }
    
    ULONG_PTR baseAddress = reader.GetProcessBaseAddress(processId);
    std::cout << "[+] Process base address: 0x" << std::hex << baseAddress << std::dec << std::endl;
    
    std::cout << "[+] Successfully attached to process!" << std::endl;
    return true;
}

void ScanPattern(KernelMemoryReader& reader)
{
    std::string pattern, mask, moduleName;
    std::cout << "\nEnter pattern (hex bytes, space-separated, ?? for wildcard): ";
    std::cin.ignore();
    std::getline(std::cin, pattern);
    
    std::cout << "Enter mask (FF for match, 00 for wildcard): ";
    std::getline(std::cin, mask);
    
    std::cout << "Enter module name (empty for all modules): ";
    std::getline(std::cin, moduleName);
    
    std::cout << "[+] Scanning memory..." << std::endl;
    
    std::vector<PatternResult> results;
    if (moduleName.empty()) {
        results = reader.ScanPattern(pattern, mask);
    } else {
        results = reader.ScanPatternInModule(pattern, mask, moduleName);
    }
    
    std::cout << "[+] Found " << results.size() << " match(es):" << std::endl;
    for (size_t i = 0; i < results.size() && i < 10; i++) {
        std::cout << "  " << (i + 1) << ". Address: 0x" << std::hex << results[i].Address 
                  << ", Offset: 0x" << results[i].Offset << std::dec << std::endl;
    }
    
    if (results.size() > 10) {
        std::cout << "  ... and " << (results.size() - 10) << " more" << std::endl;
    }
}

void FindOffsets(KernelMemoryReader& reader)
{
    std::cout << "\n[+] Finding common game offsets..." << std::endl;
    
    OffsetFinder finder(&reader);
    
    ULONG_PTR entityList = finder.FindEntityListOffset();
    ULONG_PTR localPlayer = finder.FindLocalPlayerOffset();
    ULONG_PTR viewMatrix = finder.FindViewMatrixOffset();
    ULONG_PTR health = finder.FindHealthOffset();
    ULONG_PTR position = finder.FindPositionOffset();
    ULONG_PTR boneMatrix = finder.FindBoneMatrixOffset();
    
    std::cout << "\n[+] Offset Discovery Results:" << std::endl;
    std::cout << "  Entity List: 0x" << std::hex << entityList << std::dec << std::endl;
    std::cout << "  Local Player: 0x" << std::hex << localPlayer << std::dec << std::endl;
    std::cout << "  View Matrix: 0x" << std::hex << viewMatrix << std::dec << std::endl;
    std::cout << "  Health: 0x" << std::hex << health << std::dec << std::endl;
    std::cout << "  Position: 0x" << std::hex << position << std::dec << std::endl;
    std::cout << "  Bone Matrix: 0x" << std::hex << boneMatrix << std::dec << std::endl;
}

void UpdateEntityList(KernelMemoryReader& reader)
{
    std::cout << "\n[+] Updating entity list..." << std::endl;
    
    std::vector<EntityInfo> entities;
    if (!reader.UpdateEntityList(entities)) {
        std::cout << "[-] Failed to update entity list!" << std::endl;
        return;
    }
    
    std::cout << "[+] Found " << entities.size() << " entities:" << std::endl;
    
    EntityInfo localPlayer;
    if (reader.GetLocalPlayer(localPlayer)) {
        std::cout << "\n[+] Local Player:" << std::endl;
        std::cout << "  Address: 0x" << std::hex << localPlayer.EntityAddress << std::dec << std::endl;
        std::cout << "  Health: " << localPlayer.Health << "/" << localPlayer.MaxHealth << std::endl;
        std::cout << "  Position: (" << localPlayer.Position[0] << ", " 
                  << localPlayer.Position[1] << ", " << localPlayer.Position[2] << ")" << std::endl;
        std::cout << "  Team: " << localPlayer.TeamId << std::endl;
    }
    
    std::cout << "\n[+] Entities (first 10):" << std::endl;
    for (size_t i = 0; i < entities.size() && i < 10; i++) {
        const auto& entity = entities[i];
        std::cout << "  Entity " << (i + 1) << ":" << std::endl;
        std::cout << "    Address: 0x" << std::hex << entity.EntityAddress << std::dec << std::endl;
        std::cout << "    Health: " << entity.Health << "/" << entity.MaxHealth << std::endl;
        std::cout << "    Position: (" << entity.Position[0] << ", " 
                  << entity.Position[1] << ", " << entity.Position[2] << ")" << std::endl;
        std::cout << "    Distance: " << entity.Distance << std::endl;
        std::cout << "    Is Enemy: " << (entity.IsEnemy ? "Yes" : "No") << std::endl;
        std::cout << "    Is Visible: " << (entity.IsVisible ? "Yes" : "No") << std::endl;
    }
}

void ESPFeatures(KernelMemoryReader& reader)
{
    std::cout << "\n[+] ESP Features" << std::endl;
    std::cout << "Updating entity list and calculating ESP boxes..." << std::endl;
    
    GameState gameState;
    if (!reader.UpdateGameState(gameState)) {
        std::cout << "[-] Failed to update game state!" << std::endl;
        return;
    }
    
    std::cout << "[+] Game State:" << std::endl;
    std::cout << "  In Game: " << (gameState.InGame ? "Yes" : "No") << std::endl;
    std::cout << "  Local Player Team: " << gameState.LocalPlayerTeam << std::endl;
    std::cout << "  Entity Count: " << gameState.Entities.size() << std::endl;
    
    std::cout << "\n[+] ESP Box Calculations (enemies only):" << std::endl;
    int screenWidth = 1920, screenHeight = 1080;
    
    for (const auto& entity : gameState.Entities) {
        if (entity.IsEnemy && entity.IsAlive) {
            float box[4];
            if (reader.CalculateESPBox(entity, gameState.ViewMatrix, screenWidth, screenHeight, box)) {
                std::cout << "  Enemy at (" << box[0] << ", " << box[1] 
                          << ") size: " << box[2] << "x" << box[3] << std::endl;
            }
        }
    }
}

void AimbotFeatures(KernelMemoryReader& reader)
{
    std::cout << "\n[+] Aimbot Features" << std::endl;
    
    EntityInfo localPlayer;
    if (!reader.GetLocalPlayer(localPlayer)) {
        std::cout << "[-] Failed to get local player!" << std::endl;
        return;
    }
    
    std::vector<EntityInfo> entities;
    if (!reader.UpdateEntityList(entities)) {
        std::cout << "[-] Failed to update entity list!" << std::endl;
        return;
    }
    
    std::cout << "[+] Finding best target for aimbot..." << std::endl;
    
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
        std::cout << "[+] Best Target:" << std::endl;
        std::cout << "  Address: 0x" << std::hex << bestTarget.EntityAddress << std::dec << std::endl;
        std::cout << "  Distance: " << bestTarget.Distance << std::endl;
        std::cout << "  Position: (" << bestTarget.Position[0] << ", " 
                  << bestTarget.Position[1] << ", " << bestTarget.Position[2] << ")" << std::endl;
        std::cout << "  Bone Matrix available: Yes" << std::endl;
        
        // Calculate aim angles (simplified)
        float delta[3] = {
            bestTarget.Position[0] - localPlayer.Position[0],
            bestTarget.Position[1] - localPlayer.Position[1],
            bestTarget.Position[2] - localPlayer.Position[2]
        };
        
        float yaw = atan2f(delta[1], delta[0]) * 180.0f / 3.14159f;
        float pitch = -atan2f(delta[2], sqrtf(delta[0] * delta[0] + delta[1] * delta[1])) * 180.0f / 3.14159f;
        
        std::cout << "  Calculated Angles: Yaw=" << yaw << ", Pitch=" << pitch << std::endl;
    } else {
        std::cout << "[-] No valid target found!" << std::endl;
    }
}

void UseVulnerableDriver()
{
    std::cout << "\n[+] Vulnerable Driver Exploitation" << std::endl;
    
    VulnerableDriverExploit exploit;
    auto drivers = exploit.EnumerateVulnerableDrivers();
    
    if (drivers.empty()) {
        std::cout << "[-] No vulnerable drivers found!" << std::endl;
        return;
    }
    
    std::cout << "[+] Found " << drivers.size() << " vulnerable driver(s):" << std::endl;
    for (size_t i = 0; i < drivers.size(); i++) {
        std::cout << "  " << (i + 1) << ". " << drivers[i].Name 
                  << " (" << drivers[i].ServiceName << ")" << std::endl;
    }
    
    std::cout << "\n[+] Attempting to exploit drivers..." << std::endl;
    for (const auto& driver : drivers) {
        std::cout << "  Trying " << driver.Name << "... ";
        if (exploit.ExploitGenericDriver(driver)) {
            std::cout << "SUCCESS" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }
}

void AutoDiscoverOffsets(KernelMemoryReader& reader)
{
    std::cout << "\n[+] Auto-discovering offsets..." << std::endl;
    
    OffsetFinder finder(&reader);
    auto offsets = finder.AutoDiscoverOffsets();
    
    std::cout << "[+] Discovered " << offsets.size() << " offset(s):" << std::endl;
    for (const auto& offset : offsets) {
        std::cout << "  " << offset.Name << ": 0x" << std::hex << offset.Offset 
                  << " (" << offset.ModuleName << ")" << std::dec << std::endl;
    }
    
    if (!offsets.empty()) {
        std::string filename;
        std::cout << "\nSave offsets to file (filename, or empty to skip): ";
        std::cin.ignore();
        std::getline(std::cin, filename);
        
        if (!filename.empty()) {
            if (finder.SaveOffsets(filename)) {
                std::cout << "[+] Offsets saved to " << filename << std::endl;
            } else {
                std::cout << "[-] Failed to save offsets!" << std::endl;
            }
        }
    }
}

int main()
{
    PrintBanner();
    
    KernelMemoryReader reader;
    bool driverInitialized = false;
    
    int choice;
    do {
        PrintMenu();
        std::cin >> choice;
        
        switch (choice) {
        case 1:
            driverInitialized = InitializeDriver(reader);
            break;
        case 2:
            if (!driverInitialized) {
                std::cout << "[-] Please initialize driver first!" << std::endl;
                break;
            }
            AttachToProcess(reader);
            break;
        case 3:
            if (!driverInitialized) {
                std::cout << "[-] Please initialize driver first!" << std::endl;
                break;
            }
            ScanPattern(reader);
            break;
        case 4:
            if (!driverInitialized) {
                std::cout << "[-] Please initialize driver first!" << std::endl;
                break;
            }
            FindOffsets(reader);
            break;
        case 5:
            if (!driverInitialized) {
                std::cout << "[-] Please initialize driver first!" << std::endl;
                break;
            }
            UpdateEntityList(reader);
            break;
        case 6:
            if (!driverInitialized) {
                std::cout << "[-] Please initialize driver first!" << std::endl;
                break;
            }
            ESPFeatures(reader);
            break;
        case 7:
            if (!driverInitialized) {
                std::cout << "[-] Please initialize driver first!" << std::endl;
                break;
            }
            AimbotFeatures(reader);
            break;
        case 8:
            UseVulnerableDriver();
            break;
        case 9:
            if (!driverInitialized) {
                std::cout << "[-] Please initialize driver first!" << std::endl;
                break;
            }
            AutoDiscoverOffsets(reader);
            break;
        case 0:
            std::cout << "\n[+] Exiting..." << std::endl;
            break;
        default:
            std::cout << "[-] Invalid choice!" << std::endl;
            break;
        }
        
        if (choice != 0) {
            std::cout << "\nPress Enter to continue...";
            std::cin.ignore();
            std::cin.get();
        }
    } while (choice != 0);
    
    reader.Shutdown();
    return 0;
}
