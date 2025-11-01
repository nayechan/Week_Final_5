-- StartCoroutine : Cpp API, Coroutine 생성

function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj:PrintLocation()
    
    Obj.Velocity.X = 4
    StartCoroutine(AI)
    StartCoroutine(AI2)
end

function AI()
    print("AI start")
    coroutine.yield("wait_time", 1.0)
    print("Patrol End")
end

function AI2()
    print("AI2222222 start")
    coroutine.yield("wait_predicate", function()
        return Obj.Velocity.X <= 5
    end)
    print("Patrol End2")
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
end