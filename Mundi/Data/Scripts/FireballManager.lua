function BeginPlay()
    MaxFireNumber = 30
    CurrentFireNumber = 0 


    print("[BeginPlay] " .. Obj.UUID)
    -- 전역 변수로 개수 설정
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function AddFireball()
    --z위치는 FireballArea와 동일하게
        --x위치는 [Location.x - Scale.x, Location.x + Scale.x]랜덤 생성            
        --y위치는 [Location.y - Scale.y, Location.y + Scale.y]랜덤 생성
end 

function IsCanSpawnFireball()
    --if(CurrentFireNumber < MaxFireNumber)
    --    return true;
    --return false;
end

function ResetFireball()

end



function Tick(dt)
    Obj.Location = Obj.Location + Obj.Velocity * dt
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end