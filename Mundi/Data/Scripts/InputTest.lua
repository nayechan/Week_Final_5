function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj.Velocity = Vector(0, 0, 0)

    AddComponent(Obj, "UPointLightComponent")
    PointLight = GetComponent(Obj, "UPointLightComponent")
    PointLight.Intensity = 50
    PointLight.RelativeLocation = Vector(5, 5, 5)
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    MoveCamera(Obj.Location)

    local MoveDelta = 0.1
    if InputManager:IsKeyDown('W') then
        Obj.Location = Obj.Location + Vector(MoveDelta, 0, 0)
    elseif InputManager:IsKeyDown('S') then
        Obj.Location = Obj.Location + Vector(-1*MoveDelta, 0, 0)
    elseif InputManager:IsKeyDown('A') then
        Obj.Location = Obj.Location + Vector(0, -1*MoveDelta, 0)
    elseif InputManager:IsKeyDown('D') then
        Obj.Location = Obj.Location + Vector(0, MoveDelta, 0)
    end
end

function MoveCamera()
    local Cam = GetCamera()
    if Cam then
        Cam:SetLocation(Obj.Location + PointLight.RelativeLocation)
    else
        print("fail")
    end
    -- GetCamera():SetLocation(Location)
end