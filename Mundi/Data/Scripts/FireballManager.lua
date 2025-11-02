-- Fireball 생성, 삭제를 관리해주는 스크립트입니다. 
local MaxFireNumber = 5
local CurrentFireNumber = 0
local FireballsPool = {} 

local function PushFireball(Fireball) 
    Fireball.bIsActive = false
    Fireball.Location = GlobalConfig.SpawnAreaPos

    FireballsPool[#FireballsPool + 1] = Fireball

    CurrentFireNumber =  CurrentFireNumber - 1 
end 

local function PopFireball()
    local Count = #FireballsPool
    if Count == 0 then
        return nil
    end

    local Fireball = FireballsPool[Count]
    Fireball.bIsActive = true
    FireballsPool[Count] = nil
    
    CurrentFireNumber = CurrentFireNumber + 1
    return Fireball
end


function BeginPlay()
    print("[FireballManager BeginPlay] " .. Obj.UUID)

    for i = 1, MaxFireNumber do 
        local Fireball = SpawnPrefab("Data/Prefabs/Fireball.prefab")
        
        if Fireball then
            PushFireball(Fireball) 
        end

    end

    -- Fireball 생성 함수를 전역에 등록
    GlobalConfig.SpawnFireballAt = function(Pos)

        -- 최대 개수 조절
        if CurrentFireNumber >= MaxFireNumber then
            return false
        end
        
        NewFireball = PopFireball()

        if NewFireball ~= nil then
            NewFireball.Location = Pos
        end
 
        return true
    end 

    -- Fireball 생성 가능여부 함수를 전역에 등록
    GlobalConfig.IsCanSpawnFireball = function()
        return CurrentFireNumber < MaxFireNumber
    end  

    GlobalConfig.ResetFireballs = function(InactiveFireball)
        PushFireball(InactiveFireball)       
    end

end 

function EndPlay()
    print("[FireballManager EndPlay] " .. Obj.UUID)
    -- Cleanup exposed functions
    GlobalConfig.SpawnFireballAt = nil
    GlobalConfig.IsCanSpawnFireball = nil
    GlobalConfig.ResetFireballs = nil
end

function Tick(dt)
    -- Manager could handle lifetime/cleanup here if needed
    
        
end

