function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    print(Obj.Tag)
    Obj.Velocity = Vector(0,0, -10) 
    Obj.bIsActive = true
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
 
     
    if not Obj.bIsActive then
        return 
    end

    Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end