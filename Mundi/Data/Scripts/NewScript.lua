function BeginPlay()
    print("[BeginPlay]")

    local v1 = Vector()
    local v2 = Vector(10, 10, 10)
    local v3 = Obj.Location
    print(v2.X)
    Obj:PrintLocation();
    StartCoroutine(EditAfterOneSec)
end

function EditAfterOneSec()
    coroutine.yield("wait_time", 2.0)
    GlobalConfig.Gravity2 = 5
end

function EndPlay()
    print("[EndPlay]")
end

function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end

function Tick(dt)
    -- Obj.Location = Obj.Location + Obj.Velocity * dt
end