function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    -- SpawnPrefab("Data/Prefabs/Fireball.prefab")
	Obj.Velocity.X = 3.0
    Obj.Scale.X = 10.0
    Obj.Scale.Y = 10.0
    Obj.Scale.Z = 10.0
    StartCoroutine(AI)
end

function AI()
    local fireball = SpawnPrefab("Data/Prefabs/Fireball.prefab")
    -- fireball.Location.Y = 5        -- 초기 위치 (원하는 높이)
    fireball.Location.Z = 30       -- 하늘에서 떨어지게
    
    local fallSpeed = 2.0          -- 초당 이동 속도
    local groundZ = 0.0            -- 바닥 기준
    
    -- 매 프레임 이동
    while fireball.Location.Z > groundZ do
        fireball.Location.Z = fireball.Location.Z - fallSpeed
        coroutine.yield("wait_time", 0.2)
    end
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end
function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end


function Tick(dt)
    -- Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end