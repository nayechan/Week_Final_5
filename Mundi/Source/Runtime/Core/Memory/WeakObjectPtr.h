#pragma once

constexpr uint32 INDEX_NONE = static_cast<uint32>(-1);

/*-----------------------------------------------------------------------------
    FWeakObjectPtr
 -----------------------------------------------------------------------------*/

/**
 * @class FWeakObjectPtr
 * @brief UObject의 소멸을 안전하게 감지하는 약한 포인터이다.
 *
 * 이 클래스는 UObject의 InternalIndex를 저장하여, GObjectArray를 통해
 * 해당 UObject가 여전히 유효한지(소멸되지 않았는지) 안전하게 검사한다.
 * @note 현재 구현은 UObject의 InternalIndex가 고유하며, GObjectArray의
 * 슬롯이 재사용되지 않음을 전제로 한다. (Stale Index 문제에 취약할 수 있음)
 */
class FWeakObjectPtr
{
public:
    FWeakObjectPtr();

    FWeakObjectPtr(UObject* InObject);

    FWeakObjectPtr(const FWeakObjectPtr& Other) = default;
    FWeakObjectPtr& operator=(const FWeakObjectPtr& Other) = default;

    FWeakObjectPtr(FWeakObjectPtr&& Other) = default;
    FWeakObjectPtr& operator=(FWeakObjectPtr&& Other) = default;
    
    ~FWeakObjectPtr() = default;

    /**
     * @brief 이 약한 포인터가 가리키는 실제 UObject 포인터를 반환한다.
     *
     * GObjectArray에서 InternalIndex를 조회하여 객체의 유효성을 검사한다.
     * 객체가 이미 소멸되었거나 유효하지 않은 인덱스인 경우 nullptr를 반환한다.
     *
     * @return 유효한 UObject 포인터이거나, 소멸된 경우 nullptr이다.
     */
    UObject* Get() const;

    /**
     * @brief 이 약한 포인터가 현재 유효한(살아있는) 객체를 가리키는지 확인한다.
     *
     * Get() 호출 결과가 nullptr가 아닌지 검사하는 것과 동일하다.
     *
     * @return 객체가 유효하면 true, 소멸되었거나 nullptr이면 false를 반환한다.
     */
    bool IsValid() const;

    // --- 연산자 오버로딩 ---
    
    explicit operator bool() const;

    UObject* operator->() const;

    UObject& operator*() const;

    bool operator==(const FWeakObjectPtr& Other) const;

    bool operator!=(const FWeakObjectPtr& Other) const;

    bool operator==(const UObject* InObject) const;

    bool operator!=(const UObject* InObject) const;

private:
    /** @brief GObjectArray 내 UObject의 고유 인덱스이다. */
    uint32 InternalIndex;
};