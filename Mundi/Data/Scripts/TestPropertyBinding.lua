function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj.Velocity.X = 0.0
    Obj.Velocity.Y = 0.0
    Obj.Velocity.Z = 0.0

    local A = GetComponent(Obj, "UPointLightComponent")
    if A then
        A.Intensity = 100.0
    end

    AddComponent(Obj, "USpotLightComponent")

    print("Yes Binding!")
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end
function OnBeginOverlap(OtherActor)
end

function OnEndOverlap(OtherActor)
end


function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end