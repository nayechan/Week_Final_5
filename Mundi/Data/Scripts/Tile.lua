function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnBeginOverlap(OtherActor)
    if OtherActor.Tag == "fireball" then
        if GlobalConfig and GlobalConfig.RemoveTileByUUID then
            GlobalConfig.RemoveTileByUUID(Obj.UUID)
        end
    end

end

function OnEndOverlap(OtherActor)
end

function Tick(dt)
    -- Obj.Location = Obj.Location + Obj.Velocity * dt
end