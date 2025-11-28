#pragma once

#include <memory>
#include <type_traits>

/**
 * @file PointerTypes.h
 * @brief Unreal Engine 스타일 스마트 포인터 타입 정의
 *
 * std::shared_ptr, std::weak_ptr를 래핑하여 UE 스타일의 API 제공
 */

// =====================================================
// TSharedPtr - 공유 소유권 스마트 포인터
// =====================================================

template<typename T>
class TSharedPtr
{
public:
    TSharedPtr() = default;
    TSharedPtr(std::nullptr_t) : Ptr(nullptr) {}

    explicit TSharedPtr(T* InPtr) : Ptr(InPtr) {}
    TSharedPtr(const std::shared_ptr<T>& InPtr) : Ptr(InPtr) {}
    TSharedPtr(std::shared_ptr<T>&& InPtr) : Ptr(std::move(InPtr)) {}

    // 복사/이동
    TSharedPtr(const TSharedPtr& Other) = default;
    TSharedPtr(TSharedPtr&& Other) noexcept = default;
    TSharedPtr& operator=(const TSharedPtr& Other) = default;
    TSharedPtr& operator=(TSharedPtr&& Other) noexcept = default;

    // 파생 클래스로부터 변환 (업캐스팅)
    template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    TSharedPtr(const TSharedPtr<U>& Other) : Ptr(Other.ToSharedPtr()) {}

    template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    TSharedPtr(TSharedPtr<U>&& Other) noexcept : Ptr(std::move(Other.ToSharedPtr())) {}

    template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    TSharedPtr& operator=(const TSharedPtr<U>& Other)
    {
        Ptr = Other.ToSharedPtr();
        return *this;
    }

    template<typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    TSharedPtr& operator=(TSharedPtr<U>&& Other) noexcept
    {
        Ptr = std::move(Other.ToSharedPtr());
        return *this;
    }

    // nullptr 대입
    TSharedPtr& operator=(std::nullptr_t)
    {
        Ptr.reset();
        return *this;
    }

    // 접근자
    T* Get() const { return Ptr.get(); }
    T* operator->() const { return Ptr.get(); }
    T& operator*() const { return *Ptr; }

    // 유효성 검사
    bool IsValid() const { return Ptr != nullptr; }
    explicit operator bool() const { return IsValid(); }

    // 비교 연산자
    bool operator==(const TSharedPtr& Other) const { return Ptr == Other.Ptr; }
    bool operator!=(const TSharedPtr& Other) const { return Ptr != Other.Ptr; }
    bool operator==(std::nullptr_t) const { return Ptr == nullptr; }
    bool operator!=(std::nullptr_t) const { return Ptr != nullptr; }

    // 참조 카운트
    long UseCount() const { return Ptr.use_count(); }
    bool IsUnique() const { return Ptr.unique(); }

    // 리셋
    void Reset() { Ptr.reset(); }
    void Reset(T* InPtr) { Ptr.reset(InPtr); }

    // 내부 std::shared_ptr 접근 (상호 운용성)
    const std::shared_ptr<T>& ToSharedPtr() const { return Ptr; }
    std::shared_ptr<T>& ToSharedPtr() { return Ptr; }

private:
    std::shared_ptr<T> Ptr;

    template<typename U>
    friend class TSharedPtr;

    template<typename U>
    friend class TWeakPtr;
};

// =====================================================
// TWeakPtr - 비소유 참조 스마트 포인터
// =====================================================

template<typename T>
class TWeakPtr
{
public:
    TWeakPtr() = default;
    TWeakPtr(std::nullptr_t) {}

    TWeakPtr(const TSharedPtr<T>& Shared) : Ptr(Shared.Ptr) {}
    TWeakPtr(const std::weak_ptr<T>& InPtr) : Ptr(InPtr) {}

    // 복사/이동
    TWeakPtr(const TWeakPtr& Other) = default;
    TWeakPtr(TWeakPtr&& Other) noexcept = default;
    TWeakPtr& operator=(const TWeakPtr& Other) = default;
    TWeakPtr& operator=(TWeakPtr&& Other) noexcept = default;

    // TSharedPtr로부터 대입
    TWeakPtr& operator=(const TSharedPtr<T>& Shared)
    {
        Ptr = Shared.Ptr;
        return *this;
    }

    // TSharedPtr로 승격 (Pin)
    TSharedPtr<T> Pin() const
    {
        return TSharedPtr<T>(Ptr.lock());
    }

    // 유효성 검사
    bool IsValid() const { return !Ptr.expired(); }
    explicit operator bool() const { return IsValid(); }

    // 리셋
    void Reset() { Ptr.reset(); }

private:
    std::weak_ptr<T> Ptr;
};

// =====================================================
// TSharedRef - 항상 유효한 공유 참조 (null 불가)
// =====================================================

template<typename T>
class TSharedRef
{
public:
    // nullptr로 생성 불가 - 반드시 유효한 객체 필요
    explicit TSharedRef(T* InPtr) : Ptr(InPtr)
    {
        assert(InPtr != nullptr && "TSharedRef cannot be null");
    }

    TSharedRef(const std::shared_ptr<T>& InPtr) : Ptr(InPtr)
    {
        assert(InPtr != nullptr && "TSharedRef cannot be null");
    }

    // TSharedPtr로부터 생성 (유효성 검사)
    explicit TSharedRef(const TSharedPtr<T>& Shared) : Ptr(Shared.ToSharedPtr())
    {
        assert(Ptr != nullptr && "TSharedRef cannot be created from null TSharedPtr");
    }

    // 복사/이동
    TSharedRef(const TSharedRef& Other) = default;
    TSharedRef(TSharedRef&& Other) noexcept = default;
    TSharedRef& operator=(const TSharedRef& Other) = default;
    TSharedRef& operator=(TSharedRef&& Other) noexcept = default;

    // 접근자
    T* operator->() const { return Ptr.get(); }
    T& operator*() const { return *Ptr; }
    T& Get() const { return *Ptr; }

    // TSharedPtr로 변환
    TSharedPtr<T> ToSharedPtr() const { return TSharedPtr<T>(Ptr); }

    // 비교 연산자
    bool operator==(const TSharedRef& Other) const { return Ptr == Other.Ptr; }
    bool operator!=(const TSharedRef& Other) const { return Ptr != Other.Ptr; }

private:
    std::shared_ptr<T> Ptr;
};

// =====================================================
// MakeShared - 스마트 포인터 생성 헬퍼
// =====================================================

template<typename T, typename... Args>
TSharedPtr<T> MakeShared(Args&&... args)
{
    return TSharedPtr<T>(std::make_shared<T>(std::forward<Args>(args)...));
}

template<typename T, typename... Args>
TSharedRef<T> MakeShareable(Args&&... args)
{
    return TSharedRef<T>(std::make_shared<T>(std::forward<Args>(args)...));
}

// =====================================================
// TUniquePtr - 단독 소유권 스마트 포인터
// =====================================================

template<typename T>
using TUniquePtr = std::unique_ptr<T>;

template<typename T, typename... Args>
TUniquePtr<T> MakeUnique(Args&&... args)
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}

// =====================================================
// TEnableSharedFromThis - shared_from_this 지원
// =====================================================

template<typename T>
using TEnableSharedFromThis = std::enable_shared_from_this<T>;
