-- DanceSwitcher.lua
-- Toggle between two animations using a state machine on key press.

local STATE_A = "DanceA"
local STATE_B = "DanceB"
local ASSET_A = "Data/DancingRacer_mixamo.com"
local ASSET_B = "Data/SillyDancing_mixamo.com"
local TOGGLE_KEY = 'F'   -- press F to toggle

local M = {}
M.Comp = nil

function BeginPlay()
    -- Get the skeletal mesh component from this actor
    local comp = GetComponent(Obj, "USkeletalMeshComponent")
    if comp == nil then
        print("[DanceSwitcher] No USkeletalMeshComponent on actor")
        return
    end

    -- Ensure there is a state machine instance attached and fetch it
    comp:UseStateMachine()
    local sm = comp:GetOrCreateStateMachine()

    -- Build two states and transitions on the instance
    sm:AddState(STATE_A, ASSET_A, 1.0, true)
    sm:AddState(STATE_B, ASSET_B, 1.0, true)

    sm:AddTransitionByName(STATE_A, STATE_B, 0.25)
    sm:AddTransitionByName(STATE_B, STATE_A, 0.25)

    -- Start in A
    sm:SetState(STATE_A, 0.0)

    M.Comp = comp
    print("[DanceSwitcher] Ready. Press '" .. TOGGLE_KEY .. "' to switch")
end

function Tick(Delta)
    local comp = M.Comp
    if comp == nil then return end

    if InputManager:IsKeyPressed(TOGGLE_KEY) then
        print("[DanceSwitcher] pressing the F key")
        local sm = comp:GetOrCreateStateMachine()
        local curr = sm:GetCurrentStateName()
        if curr == STATE_A then
            sm:SetState(STATE_B, -1.0) -- use transition default blend
        else
            sm:SetState(STATE_A, -1.0)
        end
    end
end

return M
