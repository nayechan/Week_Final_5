UpVector = Vector(0, 0, 1)

local YawSensitivity        = 0.005
local PitchSensitivity      = 0.0025
local PitchGuardDegrees     = 1.0
local VerticalDotLimit      = math.cos(math.rad(90 - PitchGuardDegrees)) -- ≈ cos(89°)

local MovementDelta = 0.01

local ForwardVector         = Vector(1, 0, 0)
local CameraLocation        = Vector(0, 0, 0)

local bGravity              = false;

------------------------------------------------------------
local function NormalizeCopy(V)
    local Out = Vector(V.X, V.Y, V.Z)
    Out:Normalize()
    return Out
end

local function RotateAroundAxis(VectorIn, Axis, Angle)
    local UnitAxis = NormalizeCopy(Axis)
    local CosA, SinA = math.cos(Angle), math.sin(Angle)
    local AxisCrossVector = FVector.Cross(UnitAxis, VectorIn)
    local AxisDotVector   = FVector.Dot(UnitAxis, VectorIn)
    
    return VectorIn * CosA + AxisCrossVector * SinA + UnitAxis * (AxisDotVector * (1.0 - CosA))
end

------------------------------------------------------------
function BeginPlay()
    print("[BeginPlay] " .. Obj.UUID)
    Obj.Location = Vector(0, 0, 0)
    Obj.Velocity = Vector(0, 0, 0)

    local Camera = GetCamera()
    if Camera then
        Camera:SetForward(ForwardVector)
    end

    ForwardVector = NormalizeCopy(ForwardVector)
end

function EndPlay()
    print("[EndPlay] " .. Obj.UUID)
end

function OnOverlap(OtherActor)
end

function Tick(Delta)
    
    Obj.Location = Obj.Location + Obj.Velocity * Delta

    if bGravity then 
        Obj.Velocity = Vector(0, 0, -9.8)
    else 
        Rotate()

        if InputManager:IsKeyDown('W') then MoveForward(MovementDelta) end
        if InputManager:IsKeyDown('S') then MoveForward(-MovementDelta) end
        if InputManager:IsKeyDown('A') then MoveRight(-MovementDelta) end
        if InputManager:IsKeyDown('D') then MoveRight(MovementDelta) end
        if InputManager:IsKeyDown('E') then bGravity = true end
        
        SetCamera()
        Billboard()
    end
end

------------------------------------------------------------
function Rotate()
    local MouseDelta = InputManager:GetMouseDelta()
    local MouseDeltaX = MouseDelta.X
    local MouseDeltaY = MouseDelta.Y

    local Yaw = MouseDeltaX * YawSensitivity
    ForwardVector = RotateAroundAxis(ForwardVector, UpVector, Yaw)
    ForwardVector = NormalizeCopy(ForwardVector)

    local RightVector = FVector.Cross(UpVector, ForwardVector)
    RightVector = NormalizeCopy(RightVector)

    local Pitch = MouseDeltaY * PitchSensitivity
    local Candidate = RotateAroundAxis(ForwardVector, RightVector, Pitch)

    -- 수직 잠김 방지
    if (Candidate.Z > 0.2) then
        Candidate.Z = 0.2
    end
    if (Candidate.Z < -0.6) then
        Candidate.Z = -0.6
    -- local DotUp = FVector.Dot(Candidate, UpVector)
    -- if math.abs(DotUp) > 0.999 then
    --     local Horizontal = Candidate - UpVector * DotUp

    --     Horizontal = NormalizeCopy(Horizontal)
    --     local HorizontalLength = math.sqrt(1.0 - VerticalDotLimit * VerticalDotLimit)
    --     Candidate = Horizontal * HorizontalLength + UpVector * (DotUp > 0 and VerticalDotLimit or -VerticalDotLimit)
    end

    ForwardVector = NormalizeCopy(Candidate)
end

function MoveForward(Delta)
    Obj.Location = Obj.Location + Vector(ForwardVector.X,ForwardVector.Y, 0)  * Delta
end

function MoveRight(Delta)
    local RightVector = FVector.Cross(UpVector, ForwardVector)
    RightVector = NormalizeCopy(RightVector)
    Obj.Location = Obj.Location + Vector(RightVector.X,RightVector.Y, 0) * Delta
end

---------------------------------------------------------

function Billboard()
    local Camera = GetCamera()
    if Camera then
        local Eye = CameraLocation
        local At = Obj.Location
        local Direction = Vector(At.X - Eye.X, At.Y - Eye.Y, At.Z - Eye.Z)
        Camera:SetForward(Direction)
    end
end

function SetCamera()
    local BackDistance = 7.0
    local UpDistance   = 2.0

    local Camera = GetCamera()
    if Camera then
        CameraLocation = Obj.Location + (ForwardVector * -BackDistance) + (UpVector * UpDistance)
        Camera:SetLocation(CameraLocation)
    end
end