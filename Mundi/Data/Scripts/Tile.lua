function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    -- if (OtherActor == fireball)
    --     delete (Obj)
    Obj.Location.Z = -5
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end