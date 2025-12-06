-- ============================================================================
-- FirefighterController.lua
-- Firefighter 캐릭터 애니메이션 컨트롤러: WASD 입력으로 Idle/Walking/Running 전환
-- ============================================================================

-- Character 참조 (BeginPlay에서 초기화됨)
local Character = nil
local SkeletalMeshComp = nil
local StateMachine = nil
local MovementComp = nil

-- 이동 속도 설정
local WALK_SPEED = 6.0    -- 기본 걷기 속도
local RUN_SPEED = 8.0     -- 달리기 속도

-- 애니메이션 상태 이름
local STATE_IDLE = "Idle"
local STATE_WALKING = "Walking"
local STATE_RUNNING = "Running"
local STATE_PICKUP = "PickUp"

-- 애니메이션 에셋 경로 (FBX파일명_AnimStack이름 형식)
local ANIM_IDLE = "Data/firefighter/Firefighter_Without_Cloth_Idle_mixamo.com"
local ANIM_WALKING = "Data/firefighter/Firefighter_Without_Cloth_Walking_mixamo.com"
local ANIM_RUNNING = "Data/firefighter/Firefighter_Without_Cloth_Running_mixamo.com"
local ANIM_PICKUP = "Data/firefighter/Firefighter_Without_Cloth_Picking_Up_Object_mixamo.com"

-- 현재 이동 상태 추적
local bIsMoving = false
local bIsRunning = false
local bIsPickingUp = false  -- 줍기 애니메이션 재생 중

function OnBeginPlay()
    print("[FirefighterController] BeginPlay called")

    -- GetOwnerAs를 사용해 Obj의 Owner를 ACharacter로 캐스팅
    Character = GetOwnerAs(Obj, "ACharacter")
    if Character then
        print("[FirefighterController] Character found!")
    else
        print("[FirefighterController] ERROR: Could not cast owner to ACharacter")
        return
    end

    -- SkeletalMeshComponent 가져오기
    SkeletalMeshComp = GetComponent(Obj, "USkeletalMeshComponent")
    if not SkeletalMeshComp then
        print("[FirefighterController] ERROR: No USkeletalMeshComponent found")
        return
    end

    -- CharacterMovementComponent 가져오기
    MovementComp = GetComponent(Obj, "UCharacterMovementComponent")
    if MovementComp then
        print("[FirefighterController] MovementComponent found!")
    end

    -- 애니메이션 상태 머신 설정
    SetupAnimationStateMachine()
end

function SetupAnimationStateMachine()
    print("[FirefighterController] Setting up Animation State Machine")

    -- 상태 머신 사용 활성화 및 인스턴스 가져오기
    SkeletalMeshComp:UseStateMachine()
    StateMachine = SkeletalMeshComp:GetOrCreateStateMachine()

    if not StateMachine then
        print("[FirefighterController] ERROR: Failed to create state machine")
        return
    end

    -- 상태 추가 (이름, 에셋 경로, 재생 속도, 루핑 여부)
    local idleIdx = StateMachine:AddState(STATE_IDLE, ANIM_IDLE, 1.0, true)
    local walkIdx = StateMachine:AddState(STATE_WALKING, ANIM_WALKING, 1.0, true)
    local runIdx = StateMachine:AddState(STATE_RUNNING, ANIM_RUNNING, 1.5, true)  -- 달리기 속도 1.5배
    local pickupIdx = StateMachine:AddState(STATE_PICKUP, ANIM_PICKUP, 1.0, false)  -- 루핑 안함

    print("[FirefighterController] States added: Idle=" .. tostring(idleIdx) .. ", Walking=" .. tostring(walkIdx) .. ", Running=" .. tostring(runIdx) .. ", PickUp=" .. tostring(pickupIdx))

    -- 전이(Transition) 추가 (BlendTime 0.2초)
    -- Idle <-> Walking
    StateMachine:AddTransitionByName(STATE_IDLE, STATE_WALKING, 0.2)
    StateMachine:AddTransitionByName(STATE_WALKING, STATE_IDLE, 0.2)

    -- Idle <-> Running
    StateMachine:AddTransitionByName(STATE_IDLE, STATE_RUNNING, 0.2)
    StateMachine:AddTransitionByName(STATE_RUNNING, STATE_IDLE, 0.2)

    -- Walking <-> Running
    StateMachine:AddTransitionByName(STATE_WALKING, STATE_RUNNING, 0.15)
    StateMachine:AddTransitionByName(STATE_RUNNING, STATE_WALKING, 0.15)

    -- PickUp 전이 (모든 상태에서 PickUp으로, PickUp에서 Idle로)
    StateMachine:AddTransitionByName(STATE_IDLE, STATE_PICKUP, 0.1)
    StateMachine:AddTransitionByName(STATE_WALKING, STATE_PICKUP, 0.1)
    StateMachine:AddTransitionByName(STATE_RUNNING, STATE_PICKUP, 0.1)
    StateMachine:AddTransitionByName(STATE_PICKUP, STATE_IDLE, 0.2)

    -- 초기 상태: Idle
    StateMachine:SetState(STATE_IDLE, 0.0)

    print("[FirefighterController] Animation State Machine setup complete!")
end

function Update(DeltaTime)
    if not Character or not StateMachine then
        return
    end

    -- 현재 프레임 이동 상태 초기화
    bIsMoving = false
    bIsRunning = false

    -- PickUp 애니메이션 재생 중이면 이동 불가 및 다른 입력 무시
    -- (완료 처리는 OnAnimNotify에서 EndPickUp 노티파이로 처리)
    if bIsPickingUp then
        if MovementComp then
            MovementComp.MaxWalkSpeed = 0
        end
        return
    end

    -- 왼쪽 마우스 클릭으로 줍기 (LButton = 0)
    if Input:IsMouseButtonPressed(0) then
        print("[FirefighterController] PickUp triggered!")
        bIsPickingUp = true
        StateMachine:SetState(STATE_PICKUP, 0.1)
        return
    end

    -- Shift 키로 달리기 (VK_SHIFT = 16)
    if Input:IsKeyDown(16) then
        bIsRunning = true
    end

    -- WASD 입력 체크 (W=87, A=65, S=83, D=68)
    if Input:IsKeyDown(87) or Input:IsKeyDown(65) or
       Input:IsKeyDown(83) or Input:IsKeyDown(68) then
        bIsMoving = true
    end

    -- 이동 속도 조절
    if MovementComp then
        if bIsRunning then
            MovementComp.MaxWalkSpeed = RUN_SPEED
        else
            MovementComp.MaxWalkSpeed = WALK_SPEED
        end
    end

    -- 애니메이션 상태 업데이트
    UpdateAnimationState()
end

function UpdateAnimationState()
    if not StateMachine then
        return
    end

    local currentState = StateMachine:GetCurrentStateName()
    local desiredState = STATE_IDLE

    if bIsMoving then
        if bIsRunning then
            desiredState = STATE_RUNNING
        else
            desiredState = STATE_WALKING
        end
    end

    -- 상태가 다르면 전환
    if currentState ~= desiredState then
        StateMachine:SetState(desiredState, -1.0)  -- -1.0: 전이 시간 사용
    end
end

-- AnimNotify 콜백 (C++에서 호출됨)
function OnAnimNotify(NotifyName)
    print("[FirefighterController] AnimNotify received: " .. tostring(NotifyName))

    -- EndPickUp 노티파이 처리
    if NotifyName == "EndPickUp" then
        print("[FirefighterController] EndPickUp notify triggered! Returning to Idle.")
        bIsPickingUp = false
        -- 이동 속도 복원
        if MovementComp then
            MovementComp.MaxWalkSpeed = WALK_SPEED
        end
        if StateMachine then
            StateMachine:SetState(STATE_IDLE, 0.2)
        end
    end
end

function OnEndPlay()
    print("[FirefighterController] EndPlay called")
    Character = nil
    SkeletalMeshComp = nil
    StateMachine = nil
end
