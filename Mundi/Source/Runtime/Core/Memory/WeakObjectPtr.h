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

/*-----------------------------------------------------------------------------
    TWeakObjectPtr<T>
 -----------------------------------------------------------------------------*/

/**
 * @class TWeakObjectPtr
 * @brief FWeakObjectPtr를 래핑한 템플릿 버전. T* 타입으로 안전하게 반환한다.
 * @tparam T    UObject를 상속받는 클래스여야 한다.
 */
template<typename T>
class TWeakObjectPtr
{
public:
    using ElementType = T;

    static_assert(std::is_base_of_v<UObject, T>);

    // --- 생성자 ---

    TWeakObjectPtr() = default;
    
    TWeakObjectPtr(std::nullptr_t) : WeakPtr() {}

    TWeakObjectPtr(T* InObject) : WeakPtr(InObject) {}

    template <typename OtherT>
    TWeakObjectPtr(const TWeakObjectPtr<OtherT>& Other) : WeakPtr(Other.GetWeakPtr()) {}

    // --- 대입 연산자 ---

    /** T* 대입 */
    TWeakObjectPtr& operator=(T* InObject)
    {
        WeakPtr = InObject;
        return *this;
    }

    /** nullptr 대입 */
    TWeakObjectPtr& operator=(std::nullptr_t)
    {
        WeakPtr = nullptr;
        return *this;
    }

    // --- 접근자 (Accessor) ---

    /** * @brief 유효한 T* 포인터를 반환한다.
     * @note FWeakObjectPtr::Get()이 유효하다면, 해당 슬롯은 올바른 타입의 객체임이 보장된다고 가정한다.
     */
    T* Get() const
    {
        return static_cast<T*>(WeakPtr.Get());
    }

    bool IsValid() const
    {
        return WeakPtr.IsValid();
    }

    // --- 연산자 오버로딩 ---

    explicit operator bool() const { return IsValid(); }

    T* operator->() const { return Get(); }
    T& operator*() const { return *Get(); }

    bool operator==(const TWeakObjectPtr& Other) const { return WeakPtr == Other.WeakPtr; }
    bool operator!=(const TWeakObjectPtr& Other) const { return WeakPtr != Other.WeakPtr; }

    bool operator==(const T* InObject) const { return Get() == InObject; }
    bool operator!=(const T* InObject) const { return Get() != InObject; }

    bool operator==(std::nullptr_t) const { return !IsValid(); }
    bool operator!=(std::nullptr_t) const { return IsValid(); }

private:
    FWeakObjectPtr WeakPtr;
};

/*-----------------------------------------------------------------------------
    std::hash 특수화
 -----------------------------------------------------------------------------*/
namespace std
{
    /** TWeakObjectPtr 해시 */
    template <typename T>
    struct hash<TWeakObjectPtr<T>>
    {
        size_t operator()(const TWeakObjectPtr<T>& Key) const noexcept
        {
            return hash<void*>()(static_cast<void*>(Key.Get()));
        }
    };

    /** FWeakObjectPtr 해시 */
    template <>
    struct hash<FWeakObjectPtr>
    {
        size_t operator()(const FWeakObjectPtr& Key) const noexcept
        {
            return hash<void*>()(static_cast<void*>(Key.Get()));
        }
    };
}