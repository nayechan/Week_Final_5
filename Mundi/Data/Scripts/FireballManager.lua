-- Fireball 생성, 삭제를 관리해주는 스크립트입니다. 
local MaxFireNumber = 10
local CurrentFireNumber = 0
local FireballsPool = {} 

local MinVelocity = 30
local MaxVelocity = 60


local function PushFireball(Fireball) 
    Fireball.bIsActive = false
    Fireball.Velocity = Vector(0,0,0)
    --여기에 문제가 있는 것 같은,, 초기화 이슈 
    --Fireball.Location = GlobalConfig.SpawnAreaPos

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
    -- Velocity will be set by spawner using its front direction
    FireballsPool[Count] = nil
    
    CurrentFireNumber = CurrentFireNumber + 1
    return Fireball
end

function DestroyFireball(Fireball)
    coroutine.yield("wait_time", 3.0)
    PushFireball(Fireball)
end


function BeginPlay()
    print("[FireballManager BeginPlay] " .. Obj.UUID)

    for i = 1, MaxFireNumber do 
        local Fireball = SpawnPrefab("Data/Prefabs/Fireball.prefab")
        CurrentFireNumber = CurrentFireNumber + 1
        if Fireball then
            PushFireball(Fireball) 
        end

    end

    -- Fireball 생성 함수를 전역에 등록
    GlobalConfig.SpawnFireballAt = function(Pos, Dir)

        -- 최대 개수 조절
        if CurrentFireNumber >= MaxFireNumber then
            -- print("Full")
            return false
        end
        
        print(CurrentFireNumber)

        NewFireball = PopFireball()
        
        if NewFireball ~= nil then
            NewFireball.Location = Pos
            local speed = (MinVelocity + (MaxVelocity - MinVelocity) * math.random())
            if Dir == nil then
                Dir = Vector(0, 0, -1)
            end
            NewFireball.Velocity = Dir * speed
            StartCoroutine(function()
            DestroyFireball(NewFireball)
            end)
            -- TODO 삭제 코루틴
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

