-- Fireball이 생성되는 위치를 정해주는 스크립트입니다
-- 현재는 X,Y의 바운드를 정할 수 있고, Z는 FireballArea Actor에서 가져옵니다.
local SpawnInterval = 1
local TimeAcc = 0.0

function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID) 
    Obj.Scale = Vector(10, 10, 1)

    GlobalConfig.SpawnAreaPos = Obj.Location

    -- GlobalConfig.SpawnAreaPosX = Obj.Location.X
    -- GlobalConfig.SpawnAreaPosY = Obj.Location.Y
    -- GlobalConfig.SpawnAreaPosZ = Obj.Location.Z
    
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    -- Overlap hook if needed
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
        Obj.Location * Vector(1,0,0) 

        --local ScaleX, ScaleY = Obj.Scale.X, Obj.Scale.Y

        --local RangeX = RandomInRange(LocalPos.X - LocalScale.X * 0.5, LocalPos.X + LocalScale.X * 0.5)
        --local RangeY = RandomInRange(LocalPos.Y - LocalScale.Y * 0.5, LocalPos.Y + LocalScale.Y * 0.5)
        
        local PosX = RangeX
        local PosY = RangeY
        local PosZ = LocalZ
 
        local Pos = Vector(RangeX, RangeY, LocalZ)

        GlobalConfig.SpawnFireballAt(Pos)
        print("Spawn Fireball !!!!!!!")
    end 

   --[[print("[Tick] ")]]--
end