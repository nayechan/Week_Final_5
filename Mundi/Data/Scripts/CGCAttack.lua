-- Minimal: create FireballSpawnArea count equal to CoachLevel.
-- Only create on increases; do not delete.

local SpawnedCount = 0
local CurrentLevel = 0

local function EnsureSpawned(target)
    target = math.max(0, tonumber(target or 0) or 0)
    while SpawnedCount < target do
        local s = SpawnPrefab("Data/Prefabs/FireballSpawnArea.prefab")
        s.Location = Obj.Location + Vector(4.5 * target ,0,0)
        if s then
            SpawnedCount = SpawnedCount + 1
        else
            break
        end
    end
end

function BeginPlay()
    print("[CGCAttak BeginPlay] " .. Obj.UUID)
    CurrentLevel = GlobalConfig.CoachLevel or 0
    EnsureSpawned(CurrentLevel)
end

function EndPlay()
    print("[CGCAttak EndPlay] " .. Obj.UUID)
end

function Tick(dt)
    local NewLevel = GlobalConfig.CoachLevel or 0
    if NewLevel ~= CurrentLevel then
        EnsureSpawned(NewLevel)
        CurrentLevel = NewLevel
    end
end

