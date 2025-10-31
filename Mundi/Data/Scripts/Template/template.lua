function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj:PrintLocation()
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
    Obj:PrintLocation()
end

function OnOverlap(OtherActor)
    OtherActor:PrintLocation();
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    Obj:PrintLocation()
end