function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    local tile = SpawnPrefab("Data/Prefabs/Tile.prefab")
    -- local tileSize = 1
    -- local gridWidth = 10
    -- local gridHeight = 10

    -- for y = 0, gridHeight - 1 do
    --     for x = 0, gridWidth - 1 do
    --         local tile = SpawnPrefab("Data/Prefabs/Tile.prefab")
    --         tile.Location.X = x * tileSize
    --         tile.Location.Y = y * tileSize
    --         tile.Location.Z = 0
    --     end
    -- end
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end