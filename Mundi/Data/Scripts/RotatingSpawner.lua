function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnBeginOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function OnEndOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
    local rc = GetComponent(Obj, "URotatingMovementComponent") 

    
    NewLevel = GlobalConfig.CoachLevel
    
    if(NewLevel == 1)  then
        rc.RotationRate = Vector(1,0,0) * 10.0
    elseif(NewLevel == 2) then
        rc.RotationRate = Vector(1,0,0) * 80.0
    elseif(NewLevel == 3) then
        rc.RotationRate = Vector(1,0,0) * 100.0
    elseif(NewLevel == 4) then
        rc.RotationRate = Vector(1,0,0) * 600.0
    end

    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end