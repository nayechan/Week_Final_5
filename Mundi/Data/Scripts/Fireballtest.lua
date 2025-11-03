function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    -- firball = SpawnPrefab("Data/Prefabs/Fireball.prefab")
    -- firball = SpawnPrefab("Data/Prefabs/Fireball.prefab")
    StartCoroutine(AI)
end

function AI()
    for i = 1, 10 do 
        f = SpawnPrefab("Data/Prefabs/Fireball.prefab")
        f.Location.Z = i
        coroutine.yield("wait_time", 1.0)
    end
end
    
function EndPlay()
    
end

function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end

function Tick(dt)
    -- SpawnPrefab("Data/Prefabs/Fireball.prefab")
    -- firball.Z = dt;
    Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end