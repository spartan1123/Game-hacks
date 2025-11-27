#include "EntityTracker.h"
#include <algorithm>
#include <cmath>

EntityTracker::EntityTracker(KernelMemoryReader* memoryReader)
    : m_MemoryReader(memoryReader)
    , m_EntityListOffset(0)
    , m_LocalPlayerOffset(0)
    , m_ViewMatrixOffset(0)
    , m_EntitySize(0x10)
    , m_MaxEntities(64)
    , m_HealthOffset(0x100)
    , m_MaxHealthOffset(0x104)
    , m_PositionOffset(0x138)
    , m_ViewAnglesOffset(0x140)
    , m_TeamOffset(0xF4)
    , m_BoneMatrixOffset(0x26A8)
    , m_EntityIdOffset(0x64)
    , m_LastUpdateTime(0)
{
}

EntityTracker::~EntityTracker()
{
}

bool EntityTracker::UpdateEntityList(std::vector<EntityInfo>& entities)
{
    entities.clear();
    
    if (!m_MemoryReader || m_EntityListOffset == 0) {
        return false;
    }

    DWORD processId = m_MemoryReader->FindProcessId("");
    if (processId == 0) {
        return false;
    }

    ULONG_PTR processBase = m_MemoryReader->GetProcessBaseAddress(processId);
    if (processBase == 0) {
        return false;
    }

    ULONG_PTR entityListAddress = 0;
    if (!m_MemoryReader->Read(processBase + m_EntityListOffset, entityListAddress)) {
        return false;
    }

    if (entityListAddress == 0) {
        return false;
    }

    for (ULONG i = 0; i < m_MaxEntities; i++) {
        ULONG_PTR entityAddress = 0;
        if (!m_MemoryReader->Read(entityListAddress + (i * sizeof(ULONG_PTR)), entityAddress)) {
            continue;
        }

        if (entityAddress == 0 || !IsValidEntity(entityAddress)) {
            continue;
        }

        EntityInfo entity;
        if (ReadEntityData(entityAddress, entity)) {
            entity.EntityAddress = entityAddress;
            entities.push_back(entity);
        }
    }

    m_CachedEntities = entities;
    return true;
}

bool EntityTracker::GetEntityByIndex(ULONG index, EntityInfo& entity)
{
    if (index >= m_CachedEntities.size()) {
        return false;
    }
    entity = m_CachedEntities[index];
    return true;
}

bool EntityTracker::GetEntityById(ULONG entityId, EntityInfo& entity)
{
    for (const auto& cachedEntity : m_CachedEntities) {
        if (cachedEntity.EntityId == entityId) {
            entity = cachedEntity;
            return true;
        }
    }
    return false;
}

bool EntityTracker::GetLocalPlayer(EntityInfo& localPlayer)
{
    if (!m_MemoryReader || m_LocalPlayerOffset == 0) {
        return false;
    }

    DWORD processId = m_MemoryReader->FindProcessId("");
    if (processId == 0) {
        return false;
    }

    ULONG_PTR processBase = m_MemoryReader->GetProcessBaseAddress(processId);
    if (processBase == 0) {
        return false;
    }

    ULONG_PTR localPlayerAddress = 0;
    if (!m_MemoryReader->Read(processBase + m_LocalPlayerOffset, localPlayerAddress)) {
        return false;
    }

    if (localPlayerAddress == 0) {
        return false;
    }

    if (ReadEntityData(localPlayerAddress, localPlayer)) {
        localPlayer.EntityAddress = localPlayerAddress;
        m_CachedLocalPlayer = localPlayer;
        return true;
    }

    return false;
}

bool EntityTracker::UpdateLocalPlayer()
{
    return GetLocalPlayer(m_CachedLocalPlayer);
}

bool EntityTracker::GetViewMatrix(ViewMatrix& viewMatrix)
{
    if (!m_MemoryReader || m_ViewMatrixOffset == 0) {
        return false;
    }

    DWORD processId = m_MemoryReader->FindProcessId("");
    if (processId == 0) {
        return false;
    }

    ULONG_PTR processBase = m_MemoryReader->GetProcessBaseAddress(processId);
    if (processBase == 0) {
        return false;
    }

    ULONG_PTR viewMatrixAddress = processBase + m_ViewMatrixOffset;
    if (!m_MemoryReader->ReadMemory(viewMatrixAddress, &viewMatrix.matrix, sizeof(viewMatrix.matrix))) {
        return false;
    }

    m_CachedViewMatrix = viewMatrix;
    return true;
}

bool EntityTracker::UpdateViewMatrix()
{
    return GetViewMatrix(m_CachedViewMatrix);
}

bool EntityTracker::UpdateGameState(GameState& gameState)
{
    if (!UpdateLocalPlayer()) {
        return false;
    }

    if (!UpdateViewMatrix()) {
        return false;
    }

    if (!UpdateEntityList(gameState.Entities)) {
        return false;
    }

    gameState.LocalPlayerAddress = m_CachedLocalPlayer.EntityAddress;
    gameState.ViewMatrix = m_CachedViewMatrix;
    gameState.LocalPlayerTeam = m_CachedLocalPlayer.TeamId;
    gameState.InGame = m_CachedLocalPlayer.EntityAddress != 0;
    gameState.InMenu = !gameState.InGame;

    return true;
}

bool EntityTracker::IsInGame()
{
    EntityInfo localPlayer;
    return GetLocalPlayer(localPlayer) && localPlayer.EntityAddress != 0;
}

bool EntityTracker::IsInMenu()
{
    return !IsInGame();
}

std::vector<EntityInfo> EntityTracker::GetEnemies()
{
    std::vector<EntityInfo> enemies;
    
    if (m_CachedLocalPlayer.EntityAddress == 0) {
        UpdateLocalPlayer();
    }

    for (const auto& entity : m_CachedEntities) {
        if (entity.TeamId != m_CachedLocalPlayer.TeamId && entity.IsAlive) {
            enemies.push_back(entity);
        }
    }

    return enemies;
}

std::vector<EntityInfo> EntityTracker::GetTeammates()
{
    std::vector<EntityInfo> teammates;
    
    if (m_CachedLocalPlayer.EntityAddress == 0) {
        UpdateLocalPlayer();
    }

    for (const auto& entity : m_CachedEntities) {
        if (entity.TeamId == m_CachedLocalPlayer.TeamId && 
            entity.EntityAddress != m_CachedLocalPlayer.EntityAddress && 
            entity.IsAlive) {
            teammates.push_back(entity);
        }
    }

    return teammates;
}

std::vector<EntityInfo> EntityTracker::GetVisibleEntities()
{
    std::vector<EntityInfo> visible;
    
    for (const auto& entity : m_CachedEntities) {
        if (entity.IsVisible && entity.IsAlive) {
            visible.push_back(entity);
        }
    }

    return visible;
}

std::vector<EntityInfo> EntityTracker::GetAliveEntities()
{
    std::vector<EntityInfo> alive;
    
    for (const auto& entity : m_CachedEntities) {
        if (entity.IsAlive) {
            alive.push_back(entity);
        }
    }

    return alive;
}

bool EntityTracker::GetEntityHealth(ULONG_PTR entityAddress, float& health, float& maxHealth)
{
    if (!m_MemoryReader) {
        return false;
    }

    if (!m_MemoryReader->Read(entityAddress + m_HealthOffset, health)) {
        return false;
    }

    if (!m_MemoryReader->Read(entityAddress + m_MaxHealthOffset, maxHealth)) {
        return false;
    }

    return true;
}

bool EntityTracker::GetEntityPosition(ULONG_PTR entityAddress, float position[3])
{
    if (!m_MemoryReader) {
        return false;
    }

    return m_MemoryReader->ReadMemory(entityAddress + m_PositionOffset, position, sizeof(float) * 3);
}

bool EntityTracker::GetEntityViewAngles(ULONG_PTR entityAddress, float angles[3])
{
    if (!m_MemoryReader) {
        return false;
    }

    return m_MemoryReader->ReadMemory(entityAddress + m_ViewAnglesOffset, angles, sizeof(float) * 3);
}

bool EntityTracker::GetEntityBonePosition(ULONG_PTR entityAddress, ULONG boneIndex, float position[3])
{
    if (!m_MemoryReader) {
        return false;
    }

    ULONG_PTR boneMatrixAddress = entityAddress + m_BoneMatrixOffset + (boneIndex * sizeof(float) * 12);
    float matrix[3][4];
    
    if (!m_MemoryReader->ReadMemory(boneMatrixAddress, matrix, sizeof(matrix))) {
        return false;
    }

    position[0] = matrix[0][3];
    position[1] = matrix[1][3];
    position[2] = matrix[2][3];

    return true;
}

bool EntityTracker::IsEntityAlive(ULONG_PTR entityAddress)
{
    float health = 0.0f;
    float maxHealth = 0.0f;
    
    if (!GetEntityHealth(entityAddress, health, maxHealth)) {
        return false;
    }

    return health > 0.0f;
}

bool EntityTracker::IsEntityVisible(ULONG_PTR entityAddress, ULONG_PTR localPlayerAddress)
{
    // Simple visibility check - can be enhanced with raycast
    // For now, assume all entities are potentially visible
    return true;
}

ULONG EntityTracker::GetEntityTeam(ULONG_PTR entityAddress)
{
    if (!m_MemoryReader) {
        return 0;
    }

    ULONG teamId = 0;
    m_MemoryReader->Read(entityAddress + m_TeamOffset, teamId);
    return teamId;
}

bool EntityTracker::ReadEntityData(ULONG_PTR entityAddress, EntityInfo& entity)
{
    if (!m_MemoryReader) {
        return false;
    }

    // Read health
    if (!GetEntityHealth(entityAddress, entity.Health, entity.MaxHealth)) {
        return false;
    }

    // Read position
    if (!GetEntityPosition(entityAddress, entity.Position)) {
        return false;
    }

    // Read view angles
    if (!GetEntityViewAngles(entityAddress, entity.ViewAngles)) {
        return false;
    }

    // Read team
    entity.TeamId = GetEntityTeam(entityAddress);

    // Read entity ID
    if (!m_MemoryReader->Read(entityAddress + m_EntityIdOffset, entity.EntityId)) {
        return false;
    }

    // Read bone matrix (head bone, index 6 is common)
    if (!m_MemoryReader->ReadMemory(entityAddress + m_BoneMatrixOffset + (6 * sizeof(float) * 12),
                                    entity.BoneMatrix, sizeof(entity.BoneMatrix))) {
        // Bone matrix might not be available, continue anyway
    }

    // Determine if alive
    entity.IsAlive = IsEntityAlive(entityAddress);

    // Calculate distance from local player
    if (m_CachedLocalPlayer.EntityAddress != 0) {
        entity.Distance = CalculateDistance(entity.Position, m_CachedLocalPlayer.Position);
    } else {
        entity.Distance = 0.0f;
    }

    // Determine if enemy
    if (m_CachedLocalPlayer.EntityAddress != 0) {
        entity.IsEnemy = entity.TeamId != m_CachedLocalPlayer.TeamId;
    } else {
        entity.IsEnemy = false;
    }

    // Visibility check
    entity.IsVisible = IsEntityVisible(entityAddress, m_CachedLocalPlayer.EntityAddress);

    return true;
}

bool EntityTracker::IsValidEntity(ULONG_PTR entityAddress)
{
    if (entityAddress == 0) {
        return false;
    }

    // Check if entity is alive (basic validation)
    return IsEntityAlive(entityAddress);
}

float EntityTracker::CalculateDistance(const float pos1[3], const float pos2[3])
{
    float dx = pos1[0] - pos2[0];
    float dy = pos1[1] - pos2[1];
    float dz = pos1[2] - pos2[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}
