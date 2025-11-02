function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
	Obj.Scale.X = 3.00
    Obj.Scale.Y = 3.00
    Obj.Scale.Z = 3.00

end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
    --[[Obj:PrintLocation()]]--
end

function Tick(dt)
    -- Obj.Location = Obj.Location + Obj.Velocity * dt
    
    -- TODO 
    if(/*fifreballManager에서 fireball 생성 가능한지 체크*/)
        {
            --fireballManager의 AddFireball를 통해서 fireball 생성 
         
            --z위치는 FireballArea와 동일하게
            --x위치는 [Location.x - Scale.x, Location.x + Scale.x]랜덤 생성            
            --y위치는 [Location.y - Scale.y, Location.y + Scale.y]랜덤 생성
        
        }
    
    --[[Obj:PrintLocation()]]--
    --[[print("[Tick] ")]]--
end