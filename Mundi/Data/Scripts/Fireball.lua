function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)

    Obj.Velocity = Vector(1 ,0,0 )
    Obj.Scale = Vector(10, 10, 10)
	-- Obj.Velocity.X = 3.0
    -- Obj.Scale.X = 10.0
    -- Obj.Scale.Y = 10.0
    -- Obj.Scale.Z = 10.0

    Obj.bIsActive = true
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
    if not Obj.bIsActive then
        return 
    end


    Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end