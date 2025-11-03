-- Fireball이 생성되는 위치를 정해주는 스크립트입니다
-- 현재는 X,Y의 바운드를 정할 수 있고, Z는 FireballArea Actor에서 가져옵니다.
local SpawnInterval = 1
local TimeAcc = 0.0

function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID) 
    --Obj.Scale = Vector(10, 10, 1)
    GlobalConfig.SpawnAreaPos = Obj.Location
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end

function RandomInRange(MinRange, MaxRange)
    return MinRange + (MaxRange - MinRange) * math.random()
end

function Tick(dt)
    
    TimeAcc = TimeAcc + dt

    if not GlobalConfig or not GlobalConfig.SpawnFireballAt then 
        return
    end

    while TimeAcc >= SpawnInterval do
        TimeAcc = TimeAcc - SpawnInterval

        local LocalPos = Obj.Location
        local LocalScale = Obj.Scale
        --local ScaleX, ScaleY = Obj.Scale.X, Obj.Scale
        
        local RangeX = RandomInRange(LocalPos.X - LocalScale.X * 0.5, LocalPos.X + LocalScale.X * 0.5)
        local RangeY = RandomInRange(LocalPos.Y - LocalScale.Y * 0.5, LocalPos.Y + LocalScale.Y * 0.5)

        local Pos = Vector(RangeX, RangeY, LocalPos.Z)
        
        local Front = Obj:GetForward()
        local SpawnCheck =  GlobalConfig.SpawnFireballAt(Pos, Front)
        if  SpawnCheck then
            --print("Spawn Fireball !!!!!!!")
        end

    end 

   --[[print("[Tick] ")]]--
end
