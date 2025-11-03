-- CGC 무기 회전 애니메이션 스크립트
-- 순차 회전: X축 → Y축 → Z축 순서로 90도씩 회전

local CoolTime = 4.0
local AccTime = 0.0

-- 애니메이션 상태
local Animating = false
local AnimTime = 0.0
local AnimDuration = 0.6 -- 각 축당 회전 시간

-- 회전 큐 시스템
local RotationQueue = {} -- {axis="X", amount=90}, {axis="Y", amount=180}, ...
local CurrentRotation = nil
local StartRot = nil
local TargetRot = nil

local function Clamp01(x)
    if x < 0 then return 0 elseif x > 1 then return 1 else return x end
end

local function Lerp(a, b, t)
    return a + (b - a) * t
end

local function SmoothStep(t)
    return t * t * (3 - 2 * t)
end

-- 다음 회전 큐에서 가져와서 애니메이션 시작
local function StartNextRotation()
    if #RotationQueue == 0 then
        Animating = false
        AccTime = 0.0
        return
    end
    
    CurrentRotation = table.remove(RotationQueue, 1) -- 큐에서 꺼내기
    StartRot = Obj.Rotation
    
    TargetRot = Vector(StartRot.X, StartRot.Y, StartRot.Z)
    if CurrentRotation.axis == "X" then
        TargetRot.X = StartRot.X + CurrentRotation.amount
    elseif CurrentRotation.axis == "Y" then
        TargetRot.Y = StartRot.Y + CurrentRotation.amount
    elseif CurrentRotation.axis == "Z" then
        TargetRot.Z = StartRot.Z + CurrentRotation.amount
    end
    
    AnimTime = 0.0
    Animating = true
end

-- 새 회전 시퀀스 생성 (X → Y → Z 순서)
local function GenerateRotationSequence()
    RotationQueue = {}
    
    local rotX = math.random(0, 3) * 90
    local rotY = math.random(0, 3) * 90
    local rotZ = math.random(0, 3) * 90
    
    -- X축 회전 추가 (0이 아닐 때만)
    if rotX > 0 then
        table.insert(RotationQueue, {axis = "X", amount = rotX})
    end
    
    -- Y축 회전 추가
    if rotY > 0 then
        table.insert(RotationQueue, {axis = "Y", amount = rotY})
    end
    
    -- Z축 회전 추가
    if rotZ > 0 then
        table.insert(RotationQueue, {axis = "Z", amount = rotZ})
    end
    
    -- 큐가 비어있으면 최소 1개 회전 추가
    if #RotationQueue == 0 then
        local axes = {"X", "Y", "Z"}
        local randomAxis = axes[math.random(1, 3)]
        table.insert(RotationQueue, {axis = randomAxis, amount = 90})
    end
    
    StartNextRotation()
end

function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    GenerateRotationSequence()
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function Tick(dt)
    -- 회전 애니메이션 진행
    if Animating and StartRot and TargetRot then
        AnimTime = AnimTime + dt
        local t = SmoothStep(Clamp01(AnimTime / AnimDuration))

        local cur = Obj.Rotation
        cur.X = Lerp(StartRot.X, TargetRot.X, t)
        cur.Y = Lerp(StartRot.Y, TargetRot.Y, t)
        cur.Z = Lerp(StartRot.Z, TargetRot.Z, t)
        Obj.Rotation = cur

        if AnimTime >= AnimDuration then
            -- 현재 회전 완료
            Obj.Rotation = TargetRot
            StartNextRotation() -- 다음 회전 시작
        end
        return
    end

    -- 애니 종료 후 쿨다운
    AccTime = AccTime + dt
    if AccTime >= CoolTime then
        GenerateRotationSequence()
    end
end