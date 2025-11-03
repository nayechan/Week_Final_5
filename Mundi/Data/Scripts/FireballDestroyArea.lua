function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnEndOverlap(OtherActor)
end

function OnBeginOverlap(OtherActor)
    print(OtherActor.Tag)
    --[[Obj:PrintLocation()]]--
    print("Destoy Logic")
    GlobalConfig.ResetFireballs(OtherActor)
end

function Tick(dt)
    --Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end