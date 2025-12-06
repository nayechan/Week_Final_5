#pragma once

/**
 * EPhysicsUserDataType
 *
 * PhysX Actor의 userData에 저장될 수 있는 타입들을 식별하는 열거형
 * FPhysicsUserData를 상속받는 모든 타입은 이 enum에 등록되어야 함
 */
enum class EPhysicsUserDataType : uint8
{
    Invalid = 0,
    BodyInstance,       // FBodyInstance (일반 물리 바디)
    ControllerInstance, // FControllerInstance (Character Controller)
};

/**
 * FPhysicsUserData
 *
 * PhysX Actor의 userData로 저장되는 모든 타입의 기반 구조체
 * 타입 안전한 캐스팅을 위해 Type 멤버를 통해 실제 타입을 식별
 *
 * 사용 예:
 *   void* UserData = actor->userData;
 *   if (FPhysicsUserData* PhysUserData = static_cast<FPhysicsUserData*>(UserData))
 *   {
 *       if (PhysUserData->Type == EPhysicsUserDataType::BodyInstance)
 *       {
 *           FBodyInstance* Body = static_cast<FBodyInstance*>(PhysUserData);
 *           // 안전하게 사용
 *       }
 *   }
 */
struct FPhysicsUserData
{
    /** 이 인스턴스의 실제 타입 */
    EPhysicsUserDataType Type = EPhysicsUserDataType::Invalid;

protected:
    /** 파생 클래스에서만 생성 가능 */
    FPhysicsUserData(EPhysicsUserDataType InType) : Type(InType) {}
    FPhysicsUserData() = default;
};

/**
 * 타입 안전한 캐스팅 헬퍼 함수
 *
 * @param UserData PhysX actor->userData
 * @return 요청한 타입으로 캐스팅된 포인터, 타입이 다르면 nullptr
 */
template<typename T>
T* PhysicsUserDataCast(void* UserData)
{
    if (!UserData)
    {
        return nullptr;
    }

    FPhysicsUserData* BaseData = static_cast<FPhysicsUserData*>(UserData);

    // 타입별 검증 (컴파일 타임에 타입에 맞는 enum 값 결정)
    if constexpr (std::is_same_v<T, struct FBodyInstance>)
    {
        if (BaseData->Type == EPhysicsUserDataType::BodyInstance)
        {
            return static_cast<T*>(BaseData);
        }
    }
    else if constexpr (std::is_same_v<T, struct FControllerInstance>)
    {
        if (BaseData->Type == EPhysicsUserDataType::ControllerInstance)
        {
            return static_cast<T*>(BaseData);
        }
    }

    return nullptr;
}
