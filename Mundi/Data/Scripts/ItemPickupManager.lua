-- ItemPickupManager.lua
-- 아이템 줍기 시스템 (캐릭터에 부착)
--
-- 기능:
-- - 범위 내 아이템 감지 (SphereComponent Overlap 이벤트)
-- - 가장 가까운 아이템 하이라이트
-- - 마우스 클릭으로 아이템 줍기
-- - 인벤토리(GameInstance)에 아이템 추가

-- ═══════════════════════════════════════════════════════════════════════════
-- 상태 변수
-- ═══════════════════════════════════════════════════════════════════════════

local ItemsInRange = {}        -- 범위 내 아이템 목록 (FGameObject)
local ClosestItem = nil        -- 현재 가장 가까운 아이템
local PickupAnimPath = ""      -- 줍기 애니메이션 경로 (나중에 설정)

-- ═══════════════════════════════════════════════════════════════════════════
-- 헬퍼 함수
-- ═══════════════════════════════════════════════════════════════════════════

-- 두 위치 사이의 거리 제곱 계산 (성능 최적화)
local function DistanceSquared(loc1, loc2)
    local dx = loc1.X - loc2.X
    local dy = loc1.Y - loc2.Y
    local dz = loc1.Z - loc2.Z
    return dx*dx + dy*dy + dz*dz
end

-- 아이템의 ItemComponent 가져오기
local function GetItemInfo(gameObject)
    if not gameObject then return nil end
    local itemComp = GetItemComponent(gameObject)
    if not itemComp then return nil end
    return {
        Name = itemComp.ItemName,
        Type = itemComp.ItemType,
        Quantity = itemComp.Quantity,
        CanPickUp = itemComp.bCanPickUp
    }
end

-- 아이템 목록에서 제거
local function RemoveFromList(item)
    for i, v in ipairs(ItemsInRange) do
        if v == item then
            table.remove(ItemsInRange, i)
            return true
        end
    end
    return false
end

-- ═══════════════════════════════════════════════════════════════════════════
-- 하이라이트 관리
-- ═══════════════════════════════════════════════════════════════════════════

-- 하이라이트 색상 (노란색/황금색)
local HighlightColor = { R = 1.0, G = 0.8, B = 0.2, A = 1.0 }

local function SetHighlight(gameObject, bHighlight)
    if not gameObject then return end
    local itemComp = GetItemComponent(gameObject)
    if itemComp then
        itemComp.bIsHighlighted = bHighlight

        -- 렌더러에 하이라이트 등록/해제 (아웃라인 셰이더 연동)
        if bHighlight then
            AddHighlight(gameObject, HighlightColor)
        else
            RemoveHighlight(gameObject)
        end
    end
end

local function UpdateClosestItem()
    local myLoc = Obj.Location
    local closest = nil
    local closestDistSq = math.huge

    for _, item in ipairs(ItemsInRange) do
        if item and item.bIsActive then
            local itemInfo = GetItemInfo(item)
            if itemInfo and itemInfo.CanPickUp then
                local distSq = DistanceSquared(myLoc, item.Location)
                if distSq < closestDistSq then
                    closestDistSq = distSq
                    closest = item
                end
            end
        end
    end

    -- 이전 하이라이트 해제
    if ClosestItem and ClosestItem ~= closest then
        SetHighlight(ClosestItem, false)
    end

    -- 새 하이라이트 적용
    if closest then
        SetHighlight(closest, true)
    end

    ClosestItem = closest
end

-- ═══════════════════════════════════════════════════════════════════════════
-- 아이템 줍기
-- ═══════════════════════════════════════════════════════════════════════════

local function TryPickup()
    if not ClosestItem then
        -- 줍기 애니메이션만 재생 (아이템 없음)
        -- TODO: 빈손 줍기 애니메이션
        return
    end

    local itemInfo = GetItemInfo(ClosestItem)
    if not itemInfo or not itemInfo.CanPickUp then
        return
    end

    -- 인벤토리에 추가
    local gameInstance = GetGameInstance()
    if gameInstance then
        gameInstance:AddItem(itemInfo.Name, itemInfo.Quantity)
        -- 타입 정보도 저장
        gameInstance:SetString("ItemType_" .. itemInfo.Name, tostring(itemInfo.Type))
        print("[ItemPickup] Picked up: " .. itemInfo.Name .. " x" .. itemInfo.Quantity)
    end

    -- 픽업할 아이템 임시 저장
    local pickedItem = ClosestItem

    -- 목록에서 먼저 제거
    RemoveFromList(pickedItem)
    ClosestItem = nil

    -- 아이템 파괴 (PendingDestroy 마킹 -> 물리 바디 정리 -> 하이라이트 제거 -> 지연 삭제)
    DestroyObject(pickedItem)

    -- 가장 가까운 아이템 다시 계산
    UpdateClosestItem()
end

-- ═══════════════════════════════════════════════════════════════════════════
-- 생명주기 콜백
-- ═══════════════════════════════════════════════════════════════════════════

function OnBeginPlay()
    print("[ItemPickupManager] Initialized")
    ItemsInRange = {}
    ClosestItem = nil
end

function OnBeginOverlap(OtherObj)
    if not OtherObj then return end

    -- ItemComponent가 있는지 확인
    if HasItemComponent(OtherObj) then
        local itemInfo = GetItemInfo(OtherObj)
        if itemInfo and itemInfo.CanPickUp then
            table.insert(ItemsInRange, OtherObj)
            UpdateClosestItem()
            print("[ItemPickupManager] Item in range: " .. (itemInfo.Name or "Unknown"))
        end
    end
end

function OnEndOverlap(OtherObj)
    if not OtherObj then return end

    -- 목록에서 제거
    local wasRemoved = RemoveFromList(OtherObj)
    if wasRemoved then
        SetHighlight(OtherObj, false)
        if ClosestItem == OtherObj then
            ClosestItem = nil
        end
        UpdateClosestItem()
        print("[ItemPickupManager] Item left range")
    end
end

function Update(dt)
    -- 마우스 왼쪽 클릭 감지
    if Input:IsMouseButtonPressed(MouseButton.Left) then
        TryPickup()
    end

    -- 가장 가까운 아이템 매 프레임 갱신 (플레이어/아이템 이동 대응)
    UpdateClosestItem()
end

function OnEndPlay()
    -- PIE 종료 시에는 개별 오브젝트 접근하지 않음 (이미 파괴되었을 수 있음)
    -- 렌더러의 하이라이트 목록만 클리어
    ClearHighlights()

    -- 로컬 상태만 정리
    ItemsInRange = {}
    ClosestItem = nil
    print("[ItemPickupManager] Cleaned up")
end
