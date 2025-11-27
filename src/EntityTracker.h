#pragma once

#include "MemoryReader.h"
#include <vector>
#include <string>

class EntityTracker {
public:
    EntityTracker(KernelMemoryReader* memoryReader);
    ~EntityTracker();

    // Entity list management
    bool UpdateEntityList(std::vector<EntityInfo>& entities);
    bool GetEntityByIndex(ULONG index, EntityInfo& entity);
    bool GetEntityById(ULONG entityId, EntityInfo& entity);
    
    // Local player operations
    bool GetLocalPlayer(EntityInfo& localPlayer);
    bool UpdateLocalPlayer();
    
    // View matrix operations
    bool GetViewMatrix(ViewMatrix& viewMatrix);
    bool UpdateViewMatrix();
    
    // Game state operations
    bool UpdateGameState(GameState& gameState);
    bool IsInGame();
    bool IsInMenu();
    
    // Entity filtering
    std::vector<EntityInfo> GetEnemies();
    std::vector<EntityInfo> GetTeammates();
    std::vector<EntityInfo> GetVisibleEntities();
    std::vector<EntityInfo> GetAliveEntities();
    
    // Entity information
    bool GetEntityHealth(ULONG_PTR entityAddress, float& health, float& maxHealth);
    bool GetEntityPosition(ULONG_PTR entityAddress, float position[3]);
    bool GetEntityViewAngles(ULONG_PTR entityAddress, float angles[3]);
    bool GetEntityBonePosition(ULONG_PTR entityAddress, ULONG boneIndex, float position[3]);
    bool IsEntityAlive(ULONG_PTR entityAddress);
    bool IsEntityVisible(ULONG_PTR entityAddress, ULONG_PTR localPlayerAddress);
    ULONG GetEntityTeam(ULONG_PTR entityAddress);
    
    // Offset configuration
    void SetEntityListOffset(ULONG_PTR offset) { m_EntityListOffset = offset; }
    void SetLocalPlayerOffset(ULONG_PTR offset) { m_LocalPlayerOffset = offset; }
    void SetViewMatrixOffset(ULONG_PTR offset) { m_ViewMatrixOffset = offset; }
    void SetEntitySize(ULONG size) { m_EntitySize = size; }
    void SetMaxEntities(ULONG max) { m_MaxEntities = max; }
    
    // Health offset
    void SetHealthOffset(ULONG offset) { m_HealthOffset = offset; }
    void SetMaxHealthOffset(ULONG offset) { m_MaxHealthOffset = offset; }
    
    // Position offset
    void SetPositionOffset(ULONG offset) { m_PositionOffset = offset; }
    
    // View angles offset
    void SetViewAnglesOffset(ULONG offset) { m_ViewAnglesOffset = offset; }
    
    // Team offset
    void SetTeamOffset(ULONG offset) { m_TeamOffset = offset; }
    
    // Bone matrix offset
    void SetBoneMatrixOffset(ULONG offset) { m_BoneMatrixOffset = offset; }
    
    // Entity ID offset
    void SetEntityIdOffset(ULONG offset) { m_EntityIdOffset = offset; }

private:
    KernelMemoryReader* m_MemoryReader;
    
    // Offsets (game-specific, should be discovered)
    ULONG_PTR m_EntityListOffset;
    ULONG_PTR m_LocalPlayerOffset;
    ULONG_PTR m_ViewMatrixOffset;
    ULONG m_EntitySize;
    ULONG m_MaxEntities;
    
    // Entity structure offsets
    ULONG m_HealthOffset;
    ULONG m_MaxHealthOffset;
    ULONG m_PositionOffset;
    ULONG m_ViewAnglesOffset;
    ULONG m_TeamOffset;
    ULONG m_BoneMatrixOffset;
    ULONG m_EntityIdOffset;
    
    // Cached data
    EntityInfo m_CachedLocalPlayer;
    ViewMatrix m_CachedViewMatrix;
    std::vector<EntityInfo> m_CachedEntities;
    ULONG m_LastUpdateTime;
    
    // Helper functions
    bool ReadEntityData(ULONG_PTR entityAddress, EntityInfo& entity);
    bool IsValidEntity(ULONG_PTR entityAddress);
    float CalculateDistance(const float pos1[3], const float pos2[3]);
};
