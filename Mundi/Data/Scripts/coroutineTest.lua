-- StartCoroutine : Cpp API, Coroutine 생성

function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj.Velocity.X = 6
    StartCoroutine(Gravity)
    
end

function Gravity()
    coroutine.yield("wait_predicate", function()
        return GlobalConfig.Gravity2 ==4
    end)
    print("Gravity")
end

function AI()
    print("AI start")
    coroutine.yield("wait_time", 1.0)
    print("Patrol End")
end

function AI2()
    print("AI start")
    coroutine.yield("wait_predicate", function()
        return Obj.Velocity.X <= 5
    end)
    print("Patrol End")
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end


function OnEndOverlap(OtherActor)
end

function OnBeginOverlap(OtherActor)
    Obj.Velocity.X = 0
    print("OnOverlap")
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    if GlobalConfig and GlobalConfig.Gravity2 == 5 then
        GlobalConfig.Gravity2 = 4;
    end
end