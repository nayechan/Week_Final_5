-- ────────────────────────────────────────────────────────────────────────────
-- PlayerMovement.lua
-- 간단한 WASD 캐릭터 이동 스크립트
-- ────────────────────────────────────────────────────────────────────────────

local Character = nil

-- 이동 속도
local MoveSpeed = 1.0
local RunSpeed = 2.0

function BeginPlay()
    print("[PlayerMovement] BeginPlay")

    -- Owner를 ACharacter로 캐스팅
    Character = GetOwnerAs(Obj, "ACharacter")
    if Character then
        print("[PlayerMovement] Character found!")
    else
        print("[PlayerMovement] ERROR: Could not get Character")
    end
end

function Tick(DeltaTime)
    if not Character then
        return
    end

    -- 달리기 체크
    local speed = MoveSpeed
    if InputManager:IsKeyDown('SHIFT') then
        speed = RunSpeed
    end

    -- WASD 이동
    if InputManager:IsKeyDown('W') then
        Character:MoveForward(speed)
    end
    if InputManager:IsKeyDown('S') then
        Character:MoveForward(-speed)
    end
    if InputManager:IsKeyDown('A') then
        Character:MoveRight(-speed)
    end
    if InputManager:IsKeyDown('D') then
        Character:MoveRight(speed)
    end

    -- 점프
    if InputManager:IsKeyPressed(' ') then
        if Character:CanJump() then
            Character:Jump()
        end
    end
    if InputManager:IsKeyReleased(' ') then
        Character:StopJumping()
    end
end

function EndPlay()
    print("[PlayerMovement] EndPlay")
    Character = nil
end
