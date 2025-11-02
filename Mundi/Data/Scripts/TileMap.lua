local Tiles = {}
local TileMeta = {}
local TileSize = 2;
local GridWidth = 11
local GridHeight = 11

-- Lua 내부용 가짜 UUID 생성기
local uuidCounter = 0
local function GenerateFakeUUID()
    uuidCounter = uuidCounter + 1
    return uuidCounter
end

function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    GenerateGrid()

    GlobalConfig.TileMapInfo = {
        TileSize = TileSize,
        GridWidth = GridWidth,
        GridHeight = GridHeight,
        Tiles = Tiles,
        TileMeta = TileMeta
    }

    -- UUID 기반 제거 함수 등록
    GlobalConfig.RemoveTileByUUID = function(tileUUID)
        local meta = TileMeta[tileUUID]
        if not meta then
            -- print("No tile found for UUID: " .. tostring(tileUUID))
            return
        end

        local tile = meta.tile
        if not tile then
            -- print("Tile object not found: " .. tostring(tileUUID))
            return
        end

        -- 비활성화 (렌더링, 충돌, 위치 이동)
        -- if tile.Mesh then tile.Mesh.bVisible = false end
        -- if tile.Collider then tile.Collider.bEnabled = false end
        tile.bIsActive = false
        tile.Location.Z = tile.Location.Z - 10

        -- -- 메타테이블에서 제거
        Tiles[meta.index] = nil
        TileMeta[tileUUID] = nil

        -- print("Tile removed (UUID): " .. tileUUID)
    end
end

function GenerateGrid()
    local offsetX = (GridWidth - 1) * TileSize / 2
    local offsetY = (GridHeight - 1) * TileSize / 2
    local index = 1

    for y = 0, GridHeight - 1 do
        for x = 0, GridWidth - 1 do
            local tile = SpawnPrefab("Data/Prefabs/NormalTile.prefab")
            tile.Location = Vector(x * TileSize - offsetX, y * TileSize - offsetY, 0)

            -- tile.UUID가 없으면 직접 만들어줌
            if not tile.UUID or tile.UUID == 0 then
                tile.UUID = GenerateFakeUUID()
            end

            Tiles[index] = tile
            TileMeta[tile.UUID] = { tile = tile, x = x, y = y, index = index }

            index = index + 1
        end
    end
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
    GlobalConfig.TileMapInfo = nil
    GlobalConfig.RemoveTileByUUID = nil
end

function OnOverlap(OtherActor)
end

function Tick(dt)
end