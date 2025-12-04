#include "pch.h"
#include "SkeletalMeshComponent.h"
#include "PlatformTime.h"
#include "AnimSequence.h"
#include "FbxLoader.h"
#include "AnimNodeBase.h"
#include "AnimSingleNodeInstance.h"
#include "AnimStateMachineInstance.h"
#include "AnimBlendSpaceInstance.h"

// Ragdoll Physics
#include "BodyInstance.h"
#include "CapsuleComponent.h"
#include "Character.h"
#include "ConstraintInstance.h"
#include "PhysicsAsset.h"
#include "PhysicsScene.h"
#include "SkeletalBodySetup.h"
#include "PhysicsConstraintTemplate.h"
#include "World.h"
#include "InputManager.h"
#include "VehicleActor.h"
#include "VehicleComponent.h"
#include "Renderer.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
    // Keep constructor lightweight for editor/viewer usage.
    // Load a simple default test mesh if available; viewer UI can override.
    SetSkeletalMesh(GDataDir + "/DancingRacer.fbx");    
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
    TermPhysics();
}

void USkeletalMeshComponent::BeginPlay()
{
    Super::BeginPlay();

    if (GetWorld()->bPie)
    {
        UAnimationAsset* AnimationAsset = UResourceManager::GetInstance().Get<UAnimSequence>("Data/DancingRacer_mixamo.com");
        PlayAnimation(AnimationAsset, true, 1.f);
    }
    if (AnimInstance)
    {
        AnimInstance->NativeUpdateAnimation(0.0f);
        
        FPoseContext OutputPose;
        OutputPose.Initialize(this, SkeletalMesh->GetSkeleton(), 0.0f);
        AnimInstance->EvaluateAnimation(OutputPose);
        
        CurrentLocalSpacePose = OutputPose.LocalSpacePose;
    }
    // 이제 정확한 포즈 상태에서 물리 초기화
    InitPhysics();
    ResetToRefPose();
    GetOwner()->OnComponentHit.AddDynamic(this, &USkeletalMeshComponent::OnHit);
    PhysicsBlendState = EPhysicsBlendState::Disabled;
}

void USkeletalMeshComponent::EndPlay()
{
    TermPhysics();
    Super::EndPlay();
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (!SkeletalMesh) { return; }
    
    UInputManager& Input = UInputManager::GetInstance();

    // R키: 래그돌 토글    
    // if (Input.IsKeyPressed('R'))
    // {
    //     if (PhysicsBlendState == EPhysicsBlendState::Disabled)
    //     {
    //         UE_LOG("[Physics] Starting Ragdoll");
    //         SetSimulatePhysics(true); // true = 래그돌 켜기
    //     }
    //     else
    //     {
    //         UE_LOG("[Physics] Stopping Ragdoll");
    //         SetSimulatePhysics(false); // false = 래그돌 끄기
    //     }
    // }
    //
    // // Space키: 위쪽 임펄스 (래그돌 상태일 때만)
    // if (Input.IsKeyPressed(VK_SPACE) && IsSimulatingPhysics())
    // {
    //     // 루트 바디(골반)에 힘을 가함
    //     AddImpulse(FVector(0, 0, 50.0f)); 
    // }

    // Ragdoll이 활성화된 경우 물리 시뮬레이션 처리
    if (PhysicsBlendState != EPhysicsBlendState::Disabled)
    {
        return; // Ragdoll 중에는 애니메이션 처리 건너뛰기
    }

    // Drive animation instance if present
    if (bUseAnimation && AnimInstance && SkeletalMesh && SkeletalMesh->GetSkeleton())
    {
        AnimInstance->NativeUpdateAnimation(DeltaTime);

        FPoseContext OutputPose;
        OutputPose.Initialize(this, SkeletalMesh->GetSkeleton(), DeltaTime);
        AnimInstance->EvaluateAnimation(OutputPose);

        // Apply local-space pose to component and rebuild skinning
        // 애니메이션 포즈를 BaseAnimationPose에 저장 (additive 적용 전 리셋용)
        BaseAnimationPose = OutputPose.LocalSpacePose;
        CurrentLocalSpacePose = OutputPose.LocalSpacePose;
        ForceRecomputePose();

        if (!Bodies.IsEmpty())
        {
            UpdateKinematicBonesToAnim();
        }
    }
}

void USkeletalMeshComponent::PostPhysicsTick(float DeltaTime)
{
    Super::PostPhysicsTick(DeltaTime);
    if (PhysicsBlendState != EPhysicsBlendState::Disabled)
    {
        // PhysX 결과 읽기 + 블렌딩 + 스키닝 업데이트
        UpdatePhysicsBlend(DeltaTime);
    }
}

void USkeletalMeshComponent::SetSkeletalMesh(const FString& PathFileName)
{
    Super::SetSkeletalMesh(PathFileName);

    if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshData())
    {
        const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
        const int32 NumBones = Skeleton.Bones.Num();

        CurrentLocalSpacePose.SetNum(NumBones);
        CurrentComponentSpacePose.SetNum(NumBones);
        TempFinalSkinningMatrices.SetNum(NumBones);

        for (int32 i = 0; i < NumBones; ++i)
        {
            const FBone& ThisBone = Skeleton.Bones[i];
            const int32 ParentIndex = ThisBone.ParentIndex;
            FMatrix LocalBindMatrix;

            if (ParentIndex == -1) // 루트 본
            {
                LocalBindMatrix = ThisBone.BindPose;
            }
            else // 자식 본
            {
                const FMatrix& ParentInverseBindPose = Skeleton.Bones[ParentIndex].InverseBindPose;
                LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
            }
            // 계산된 로컬 행렬을 로컬 트랜스폼으로 변환
            CurrentLocalSpacePose[i] = FTransform(LocalBindMatrix); 
        }
        RefPose = CurrentLocalSpacePose;
        ForceRecomputePose();

        // Rebind anim instance to new skeleton
        if (AnimInstance)
        {
            AnimInstance->InitializeAnimation(this);
        }
    }
    else
    {
        // 메시 로드 실패 시 버퍼 비우기
        CurrentLocalSpacePose.Empty();
        CurrentComponentSpacePose.Empty();
        TempFinalSkinningMatrices.Empty();
        PhysicsAsset = nullptr;
    }
}

void USkeletalMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
    PhysicsBlendState = EPhysicsBlendState::Disabled;
    PhysicsBlendWeight = 0.0f;
    
    Constraints.Empty();
    Bodies.Empty();
    BodyIndexMap.Empty();
    PhysicsBlendPoseSaved.Empty();
    PhysicsResultPose.Empty();
    bUseAnimation = true;
    bIsHit = false;
    PhysicsBlendState = EPhysicsBlendState::Disabled;
}

void USkeletalMeshComponent::OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, FHitResult HitResult)
{
    if (bIsHit) { return; }
    if (OtherComp->GetOwner() == GetOwner()) { return; }
    if (AVehicleActor* Car = Cast<AVehicleActor>(OtherComp->GetOwner()))
    {
        UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(MyComp->GetOwner()->GetComponent(UCapsuleComponent::StaticClass()));
        if (Capsule)
        {
            Capsule->BodyInstance.SetCollisionEnabled(false);
        }
        
        bIsHit = true;
        float Speed = Car->GetVehicleComponent()->GetPhysXVehicle()->mDriveDynData.mEnginespeed;
        SetSimulatePhysics(true);
        AddImpulse(HitResult.ImpactNormal * Speed * 2);
    }
}

void USkeletalMeshComponent::OnPropertyChanged(const FProperty& Property)
{
    Super::OnPropertyChanged(Property);

    // PhysicsAsset 프로퍼티가 변경되면 다시 생성
    if (strcmp(Property.Name, "PhysicsAsset") == 0)
    {
        InitPhysics();
    }
}

void USkeletalMeshComponent::SetAnimInstance(UAnimInstance* InInstance)
{
    AnimInstance = InInstance;
    if (AnimInstance)
    {
        AnimInstance->InitializeAnimation(this);
    }
}

void USkeletalMeshComponent::PlayAnimation(UAnimationAsset* Asset, bool bLooping, float InPlayRate)
{
    UAnimSingleNodeInstance* Single = nullptr;
    if (!AnimInstance)
    {
        Single = NewObject<UAnimSingleNodeInstance>();
        SetAnimInstance(Single);
    }
    else
    {
        Single = Cast<UAnimSingleNodeInstance>(AnimInstance);
        if (!Single)
        {
            // Replace with a single-node instance for simple playback
            // 기존 AnimInstance를 먼저 삭제
            ObjectFactory::DeleteObject(AnimInstance);
            AnimInstance = nullptr;

            Single = NewObject<UAnimSingleNodeInstance>();
            SetAnimInstance(Single);
        }
    }

    if (Single)
    {
        Single->SetAnimationAsset(Asset, bLooping);
        Single->SetPlayRate(InPlayRate);
        Single->Play(true);
    }
}

// ==== Lua-friendly State Machine helper: switch this component to a state machine anim instance ====
void USkeletalMeshComponent::UseStateMachine()
{
    UAnimStateMachineInstance* StateMachine = Cast<UAnimStateMachineInstance>(AnimInstance);
    if (!StateMachine)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimStateMachineInstance\n");
        StateMachine = NewObject<UAnimStateMachineInstance>();
        SetAnimInstance(StateMachine);
    }
    else
    {
        UE_LOG("[SkeletalMeshComponent] AnimStateMachineInstance already exists\n");
    }
}

UAnimStateMachineInstance* USkeletalMeshComponent::GetOrCreateStateMachine()
{
    UAnimStateMachineInstance* StateMachine = Cast<UAnimStateMachineInstance>(AnimInstance);
    if (!StateMachine)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimStateMachineInstance\n");
        StateMachine = NewObject<UAnimStateMachineInstance>();
        SetAnimInstance(StateMachine);
    }
    return StateMachine;
}

// ==== Lua-friendly Blend Space helper: switch this component to a blend space 2D anim instance ====
void USkeletalMeshComponent::UseBlendSpace2D()
{
    UAnimBlendSpaceInstance* BS = Cast<UAnimBlendSpaceInstance>(AnimInstance);
    if (!BS)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimBlendSpaceInstance\n");
        BS = NewObject<UAnimBlendSpaceInstance>();
        SetAnimInstance(BS);
    }
    else
    {
        UE_LOG("[SkeletalMeshComponent] AnimBlendSpaceInstance already exists\n");
    }
}

UAnimBlendSpaceInstance* USkeletalMeshComponent::GetOrCreateBlendSpace2D()
{
    UAnimBlendSpaceInstance* BS = Cast<UAnimBlendSpaceInstance>(AnimInstance);
    if (!BS)
    {
        UE_LOG("[SkeletalMeshComponent] Creating new AnimBlendSpaceInstance\n");
        BS = NewObject<UAnimBlendSpaceInstance>();
        SetAnimInstance(BS);
    }
    return BS;
}

void USkeletalMeshComponent::StopAnimation()
{
    if (UAnimSingleNodeInstance* Single = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        Single->Stop();
    }
}

void USkeletalMeshComponent::SetAnimationPosition(float InSeconds)
{
    if (UAnimSingleNodeInstance* Single = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        Single->SetPosition(InSeconds, false);
    }
}

bool USkeletalMeshComponent::IsPlayingAnimation() const
{
    return AnimInstance ? AnimInstance->IsPlaying() : false;
}

float USkeletalMeshComponent::GetAnimationPosition()
{
    if (UAnimSingleNodeInstance* Single = Cast<UAnimSingleNodeInstance>(AnimInstance))
    {
        return Single->GetPosition();
    }
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        CurrentLocalSpacePose[BoneIndex] = NewLocalTransform;
        ForceRecomputePose();
    }
}

void USkeletalMeshComponent::SetBoneWorldTransform(int32 BoneIndex, const FTransform& NewWorldTransform)
{
    if (BoneIndex < 0 || BoneIndex >= CurrentLocalSpacePose.Num())
        return;

    const int32 ParentIndex = SkeletalMesh->GetSkeleton()->Bones[BoneIndex].ParentIndex;

    const FTransform& ParentWorldTransform = GetBoneWorldTransform(ParentIndex);
    FTransform DesiredLocal = ParentWorldTransform.GetRelativeTransform(NewWorldTransform);

    SetBoneLocalTransform(BoneIndex, DesiredLocal);
}


FTransform USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
    if (CurrentLocalSpacePose.Num() > BoneIndex)
    {
        return CurrentLocalSpacePose[BoneIndex];
    }
    return FTransform();
}

FTransform USkeletalMeshComponent::GetBoneWorldTransform(int32 BoneIndex) const
{
    if (CurrentLocalSpacePose.Num() > BoneIndex && BoneIndex >= 0)
    {
        // 뼈의 컴포넌트 공간 트랜스폼 * 컴포넌트의 월드 트랜스폼
        return GetWorldTransform().GetWorldTransform(CurrentComponentSpacePose[BoneIndex]);
    }
    return GetWorldTransform(); // 실패 시 컴포넌트 위치 반환
}

void USkeletalMeshComponent::ForceRecomputePose()
{
    if (!SkeletalMesh) { return; } 

    // LocalSpace -> ComponentSpace 계산
    UpdateComponentSpaceTransforms();

    // ComponentSpace -> Final Skinning Matrices 계산
    // UpdateFinalSkinningMatrices()에서 UpdateSkinningMatrices()를 호출하여 본 행렬 계산 시간과 함께 전달
    UpdateFinalSkinningMatrices();
}

void USkeletalMeshComponent::UpdateComponentSpaceTransforms()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FTransform& LocalTransform = CurrentLocalSpacePose[BoneIndex];
        const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

        if (ParentIndex == -1) // 루트 본
        {
            CurrentComponentSpacePose[BoneIndex] = LocalTransform;
        }
        else // 자식 본
        {
            const FTransform& ParentComponentTransform = CurrentComponentSpacePose[ParentIndex];
            CurrentComponentSpacePose[BoneIndex] = ParentComponentTransform.GetWorldTransform(LocalTransform);
        }
    }
}

void USkeletalMeshComponent::UpdateFinalSkinningMatrices()
{
    const FSkeleton& Skeleton = SkeletalMesh->GetSkeletalMeshData()->Skeleton;
    const int32 NumBones = Skeleton.Bones.Num();

    // 본 행렬 계산 시간 측정 시작
    uint64 BoneMatrixCalcStart = FWindowsPlatformTime::Cycles64();

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        const FMatrix& InvBindPose = Skeleton.Bones[BoneIndex].InverseBindPose;
        const FMatrix ComponentPoseMatrix = CurrentComponentSpacePose[BoneIndex].ToMatrix();

        TempFinalSkinningMatrices[BoneIndex] = InvBindPose * ComponentPoseMatrix;
    }

    // 본 행렬 계산 시간 측정 종료
    uint64 BoneMatrixCalcEnd = FWindowsPlatformTime::Cycles64();
    double BoneMatrixCalcTimeMS = FWindowsPlatformTime::ToMilliseconds(BoneMatrixCalcEnd - BoneMatrixCalcStart);

    // 본 행렬 계산 시간을 부모 USkinnedMeshComponent로 전달
    // 부모에서 실제 스키닝 모드(CPU/GPU)에 따라 통계에 추가됨
    UpdateSkinningMatrices(TempFinalSkinningMatrices, BoneMatrixCalcTimeMS);
}

void USkeletalMeshComponent::ApplyAdditiveTransforms(const TMap<int32, FTransform>& AdditiveTransforms)
{
    if (AdditiveTransforms.IsEmpty()) return;

    // CurrentLocalSpacePose는 TickComponent에서 이미 기본 애니메이션 포즈를 포함하고 있어야 함
    for (auto const& [BoneIndex, AdditiveTransform] : AdditiveTransforms)
    {
        if (BoneIndex >= 0 && BoneIndex < CurrentLocalSpacePose.Num())
        {
            // 회전이 위치에 영향을 주지 않도록 각 성분을 개별적으로 적용
            FTransform& BasePose = CurrentLocalSpacePose[BoneIndex];

            // 위치: 단순 덧셈
            FVector FinalLocation = BasePose.Translation + AdditiveTransform.Translation;

            // 회전: 쿼터니언 곱셈
            FQuat FinalRotation = AdditiveTransform.Rotation * BasePose.Rotation;

            // 스케일: 성분별 곱셈
            FVector FinalScale = FVector(
                BasePose.Scale3D.X * AdditiveTransform.Scale3D.X,
                BasePose.Scale3D.Y * AdditiveTransform.Scale3D.Y,
                BasePose.Scale3D.Z * AdditiveTransform.Scale3D.Z
            );

            CurrentLocalSpacePose[BoneIndex] = FTransform(FinalLocation, FinalRotation, FinalScale);
        }
    }

    // 모든 additive 적용 후 최종 포즈 재계산
    ForceRecomputePose();
}

void USkeletalMeshComponent::ResetToRefPose()
{
    CurrentLocalSpacePose = RefPose;
    ForceRecomputePose();
}

void USkeletalMeshComponent::ResetToAnimationPose()
{
    // BaseAnimationPose가 비어있으면 RefPose 사용
    if (BaseAnimationPose.IsEmpty())
    {
        CurrentLocalSpacePose = RefPose;
    }
    else
    {
        CurrentLocalSpacePose = BaseAnimationPose;
    }
    ForceRecomputePose();
}

void USkeletalMeshComponent::TriggerAnimNotify(const FAnimNotifyEvent& NotifyEvent)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Owner->HandleAnimNotify(NotifyEvent);
    }
}

void USkeletalMeshComponent::SyncByPhysics(const FTransform& NewTransform)
{
    FTransform FinalTransform = NewTransform;
    FinalTransform.Scale3D = GetWorldScale();
    Super::SyncByPhysics(FinalTransform);
}

void USkeletalMeshComponent::InitPhysics()
{
    TermPhysics();
    if (!SkeletalMesh)
    {
        return; 
    }
    ForceRecomputePose();
    UPhysicsAsset* TargetPhysAsset = PhysicsAsset; // 컴포넌트의 프로퍼티

    if (!TargetPhysAsset || !TargetPhysAsset->IsValid())
    {
        // 에셋이 아예 없으면 실패
        UE_LOG("[Physics] InitPhysics failed: No valid PhysicsAsset found.");
        return;
    }

    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    if (!Skeleton)
    {
        UE_LOG("[Physics] InitPhysics failed: Skeleton is null");
        return;
    }

    UWorld* World = GetWorld();
    if (!World || !World->GetPhysicsScene())
    {
        UE_LOG("[Physics] InitPhysics failed: World or PhysicsScene is invalid");
        return;
    }

    FPhysicsScene* PhysScene = World->GetPhysicsScene();

    // Body 생성
    const int32 NumBodies = TargetPhysAsset->GetBodySetupCount();
    Bodies.Reserve(NumBodies);

    for (int32 i = 0; i < NumBodies; ++i)
    {
        USkeletalBodySetup* BodySetup = TargetPhysAsset->GetBodySetup(i);
        if (!BodySetup) continue;

        // 본 이름으로 인덱스 찾기
        const FString BoneNameStr = BodySetup->BoneName.ToString();
        const int32* BoneIndexPtr = Skeleton->BoneNameToIndex.Find(BoneNameStr);
        
        if (!BoneIndexPtr)
        {
            UE_LOG("[Physics] Warning: Bone '%s' not found in Skeleton.", BoneNameStr.c_str());
            continue;
        }
        int32 BoneIndex = *BoneIndexPtr;

        FBodyInstance* Body = new FBodyInstance();
        Body->BoneName = BodySetup->BoneName;
        Body->BoneIndex = BoneIndex;

        FTransform BoneWorldTM = GetBoneWorldTransform(BoneIndex);
        BoneWorldTM.Rotation.Normalize();
        Body->InitBody(BodySetup, BoneWorldTM, (i == 0) ? this : nullptr, PhysScene);

        // 컴포넌트의 BodyInstance 설정을 개별 Body에 전파
        Body->SetEnableGravity(BodyInstance.IsEnabledGravity());

        // DOF(Degree Of Freedom) Lock 설정 복사
        bool bLockLinearX, bLockLinearY, bLockLinearZ;
        bool bLockAngularX, bLockAngularY, bLockAngularZ;
        
        BodyInstance.GetLinearLock(bLockLinearX, bLockLinearY, bLockLinearZ);
        BodyInstance.GetAngularLock(bLockAngularX, bLockAngularY, bLockAngularZ);
        
        Body->SetLinearLock(bLockLinearX, bLockLinearY, bLockLinearZ);
        Body->SetAngularLock(bLockAngularX, bLockAngularY, bLockAngularZ);

        // 초기 상태는 Kinematic
        Body->SetSimulatePhysics(false);

        // 관리 리스트에 추가
        int32 BodyIndex = Bodies.Add(Body);
        BodyIndexMap.Add(BodySetup->BoneName, BodyIndex);
    }

    // 관절(Constraint) 생성
    CreatePhysicsConstraints();

    const int32 NumBones = Skeleton->Bones.Num();
    PhysicsBlendPoseSaved.SetNum(NumBones);
    PhysicsResultPose.SetNum(NumBones); 

    const int32 TotalBones = Skeleton->Bones.Num();
    BoneIndexToBodyCache.assign(TotalBones, nullptr);

    for (FBodyInstance* Body : Bodies)
    {
        if (Body && Body->BoneIndex >= 0 && Body->BoneIndex < TotalBones)
        {
            BoneIndexToBodyCache[Body->BoneIndex] = Body;
        }
    }
    
    UE_LOG("[Physics] InitPhysics completed: %d bodies, %d constraints using asset '%s'",
        Bodies.Num(), Constraints.Num(), TargetPhysAsset->GetName().c_str());
}

void USkeletalMeshComponent::TermPhysics()
{
    // 상태 초기화
    PhysicsBlendState = EPhysicsBlendState::Disabled;
    PhysicsBlendWeight = 0.0f;

    // Constraints 해제 (Joint 먼저 해제해야 Body 해제 시 오류 방지)
    for (FConstraintInstance* Constraint : Constraints)
    {
        if (Constraint)
        {
            Constraint->TermConstraint();
            delete Constraint;
        }
    }
    Constraints.Empty();

    // Bodies 해제
    for (FBodyInstance* Body : Bodies)
    {
        if (Body)
        {
            Body->TermBody();
            delete Body;
        }
    }
    Bodies.Empty();

    // 인덱스 맵 초기화
    BodyIndexMap.Empty();

    // 블렌딩 포즈 초기화
    PhysicsBlendPoseSaved.Empty();
    PhysicsResultPose.Empty();

    // 애니메이션 재활성화
    bUseAnimation = true;

    UE_LOG("[Physics] TermPhysics: All bodies and constraints released");
}

void USkeletalMeshComponent::CreatePhysicsConstraints()
{
    if (!SkeletalMesh)
    {
        return;
    }

    UPhysicsAsset* PhysAsset = PhysicsAsset;
    if (!PhysAsset)
    {
        return;
    }

    const int32 NumConstraints = PhysAsset->GetConstraintCount();
    Constraints.Reserve(NumConstraints);

    for (int32 i = 0; i < NumConstraints; ++i)
    {
        UPhysicsConstraintTemplate* Template = PhysAsset->ConstraintSetup[i];
        if (!Template)
        {
            continue;
        }

        // 연결된 두 Body 찾기
        const int32* BodyIdx1Ptr = BodyIndexMap.Find(Template->GetBone1Name());
        const int32* BodyIdx2Ptr = BodyIndexMap.Find(Template->GetBone2Name());

        if (!BodyIdx1Ptr || !BodyIdx2Ptr)
        {
            UE_LOG("[Ragdoll] Constraint bones not found: %s <-> %s",
                Template->GetBone1Name().ToString().c_str(), Template->GetBone2Name().ToString().c_str());
            continue;
        }

        FBodyInstance* Body1 = Bodies[*BodyIdx1Ptr];
        FBodyInstance* Body2 = Bodies[*BodyIdx2Ptr];

        if (!Body1 || !Body2)
        {
            continue;
        }

        // Constraint Instance 생성 (템플릿 설정 복사)
        FConstraintInstance* Constraint = new FConstraintInstance(Template->DefaultInstance);

        // Joint 초기화 (동적 Frame 계산 사용)
        // Twist 축(X축)이 부모→자식 뼈 방향으로 자동 정렬됨
        Constraint->InitConstraintWithFrames(Body1, Body2, this);
        Constraints.Add(Constraint);
    }

    UE_LOG("[Ragdoll] CreateRagdollConstraints: %d constraints created", Constraints.Num());
}

void USkeletalMeshComponent::SetSimulatePhysics(bool bSimulate, bool bBlend)
{    
    if (Bodies.IsEmpty())
    {
        InitPhysics();
        if (Bodies.IsEmpty())
        {
            UE_LOG("[Physics] SetSimulatePhysics failed: Initialization failed.");
            return;
        }
    }

    if (bSimulate)
    {
        if (PhysicsBlendState == EPhysicsBlendState::Active) { return; }
        PhysicsBlendPoseSaved = CurrentLocalSpacePose;
        UpdateKinematicBonesToAnim();
        ForceRecomputePose();

        FVector ComponentVel = BodyInstance.GetLinearVelocity();
                
        ACharacter* Character = Cast<ACharacter>(GetOwner());
        if (Character)
        {
            ComponentVel = Character->GetVelocity();
            Character->GetCapsuleComponent()->BodyInstance.SetCollisionEnabled(false);
        }
        // Kinematic -> Dynamic 전환
        for (FBodyInstance* Body : Bodies)
        {
            if (Body)
            {
                Body->SetSimulatePhysics(true);
                Body->SetLinearVelocity(ComponentVel); // 관성 유지
                Body->WakeUp();
            }
        }

        if (bBlend)
        {
            PhysicsBlendState = EPhysicsBlendState::BlendingIn;
            PhysicsBlendWeight = 0.0f;
        }
        else
        {
            PhysicsBlendState = EPhysicsBlendState::Active;
            PhysicsBlendWeight = 1.0f;
        }

        bUseAnimation = false;
    }
    else
    {
        if (PhysicsBlendState == EPhysicsBlendState::Disabled) { return; }
        // Dynamic -> Kinematic 전환
        for (FBodyInstance* Body : Bodies)
        {
            if (Body)
            {
                Body->SetSimulatePhysics(false);
                Body->SetLinearVelocity(FVector::Zero()); // 끄는 순간 속도 제거
            }
        }

        if (bBlend)
        {
            PhysicsBlendState = EPhysicsBlendState::BlendingOut;
            PhysicsBlendWeight = 1.0f; // 래그돌 100% 상태에서 시작
        }
        else
        {
            PhysicsBlendState = EPhysicsBlendState::Disabled;
            PhysicsBlendWeight = 0.0f;
            UpdateKinematicBonesToAnim();
        }

        bUseAnimation = true;
    }

    UE_LOG("[Physics] SetSimulatePhysics: %s (Blend: %s)", 
        bSimulate ? "Enabled" : "Disabled", bBlend ? "True" : "False");
}

void USkeletalMeshComponent::AddImpulse(const FVector& Impulse, FName BoneName, bool bVelChange)
{
    if (Bodies.IsEmpty()) { return; }

    FBodyInstance* TargetBody = nullptr;

    if (!BoneName.IsValid())
    {
        // 루트 바디
        TargetBody = Bodies[0];
    }
    else
    {
        // 특정 뼈 찾기
        if (int32* Index = BodyIndexMap.Find(BoneName))
        {
            TargetBody = Bodies[*Index];
        }
    }

    if (TargetBody)
    {
        TargetBody->AddImpulse(Impulse, bVelChange);
    }
}

void USkeletalMeshComponent::AddForce(const FVector& Force, FName BoneName, bool bAccelChange)
{
    if (Bodies.IsEmpty()) { return; }

    FBodyInstance* TargetBody = nullptr;

    if (!BoneName.IsValid())
    {
        // 루트 바디
        TargetBody = Bodies[0];
    }
    else
    {
        // 특정 뼈 찾기
        if (int32* Index = BodyIndexMap.Find(BoneName))
        {
            TargetBody = Bodies[*Index];
        }
    }

    if (TargetBody)
    {
        TargetBody->AddForce(Force, bAccelChange);
    }
}

void USkeletalMeshComponent::RenderPhysicsAssetDebug(URenderer* Renderer) const
{
    if (!Renderer || !PhysicsAsset || !SkeletalMesh) return;

    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    if (!Skeleton) return;

    const float CmToM = 0.01f;
    const float Default = 1.0f;
    const FVector4 LineColor(0.0f, 1.0f, 0.5f, 1.0f);  // 청록색
    const int32 NumSegments = 16;

    // 모든 BodySetup 순회
    int32 BodyCount = PhysicsAsset->GetBodySetupCount();
    for (int32 BodyIdx = 0; BodyIdx < BodyCount; ++BodyIdx)
    {
        USkeletalBodySetup* Body = PhysicsAsset->GetBodySetup(BodyIdx);
        if (!Body) continue;

        // 본 인덱스 찾기
        auto it = Skeleton->BoneNameToIndex.find(Body->BoneName.ToString());
        if (it == Skeleton->BoneNameToIndex.end()) continue;

        int32 BoneIndex = it->second;
        FTransform BoneTM = GetBoneWorldTransform(BoneIndex);

        // Sphere Elements
        for (const FKSphereElem& Elem : Body->AggGeom.SphereElems)
        {
            FVector WorldCenter = BoneTM.TransformPosition(Elem.Center);
            float ScaledRadius = Elem.Radius * CmToM;

            // XY 평면 원
            for (int32 i = 0; i < NumSegments; ++i)
            {
                float Angle1 = (float(i) / NumSegments) * 2.0f * PI;
                float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;
                FVector P1 = WorldCenter + FVector(cos(Angle1) * ScaledRadius, sin(Angle1) * ScaledRadius, 0.0f);
                FVector P2 = WorldCenter + FVector(cos(Angle2) * ScaledRadius, sin(Angle2) * ScaledRadius, 0.0f);
                Renderer->AddLine(P1, P2, LineColor);
            }
            // XZ 평면 원
            for (int32 i = 0; i < NumSegments; ++i)
            {
                float Angle1 = (float(i) / NumSegments) * 2.0f * PI;
                float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;
                FVector P1 = WorldCenter + FVector(cos(Angle1) * ScaledRadius, 0.0f, sin(Angle1) * ScaledRadius);
                FVector P2 = WorldCenter + FVector(cos(Angle2) * ScaledRadius, 0.0f, sin(Angle2) * ScaledRadius);
                Renderer->AddLine(P1, P2, LineColor);
            }
            // YZ 평면 원
            for (int32 i = 0; i < NumSegments; ++i)
            {
                float Angle1 = (float(i) / NumSegments) * 2.0f * PI;
                float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;
                FVector P1 = WorldCenter + FVector(0.0f, cos(Angle1) * ScaledRadius, sin(Angle1) * ScaledRadius);
                FVector P2 = WorldCenter + FVector(0.0f, cos(Angle2) * ScaledRadius, sin(Angle2) * ScaledRadius);
                Renderer->AddLine(P1, P2, LineColor);
            }
        }

        // Box Elements (Scale = 1.0f, 이미 미터 단위)
        for (const FKBoxElem& Elem : Body->AggGeom.BoxElems)
        {
            // Box의 World Transform (BoneTM 기준)
            FVector WorldCenter = BoneTM.TransformPosition(Elem.Center);
            FQuat WorldRotation = BoneTM.Rotation * Elem.Rotation;

            // Box Half Extent (X, Y, Z는 cm 단위 전체 크기이므로 절반 + 미터 변환)
            FVector Extent(Elem.X * CmToM, Elem.Y * CmToM, Elem.Z * CmToM);

            // 로컬 꼭지점
            FVector LocalV[8] = {
                FVector(-Extent.X, -Extent.Y, -Extent.Z),
                FVector(+Extent.X, -Extent.Y, -Extent.Z),
                FVector(+Extent.X, +Extent.Y, -Extent.Z),
                FVector(-Extent.X, +Extent.Y, -Extent.Z),
                FVector(-Extent.X, -Extent.Y, +Extent.Z),
                FVector(+Extent.X, -Extent.Y, +Extent.Z),
                FVector(+Extent.X, +Extent.Y, +Extent.Z),
                FVector(-Extent.X, +Extent.Y, +Extent.Z),
            };

            // 월드 변환
            FVector Corners[8];
            for (int32 i = 0; i < 8; ++i)
            {
                Corners[i] = WorldCenter + WorldRotation.RotateVector(LocalV[i]);
            }

            // 12개 에지
            Renderer->AddLine(Corners[0], Corners[1], LineColor);
            Renderer->AddLine(Corners[1], Corners[2], LineColor);
            Renderer->AddLine(Corners[2], Corners[3], LineColor);
            Renderer->AddLine(Corners[3], Corners[0], LineColor);
            Renderer->AddLine(Corners[4], Corners[5], LineColor);
            Renderer->AddLine(Corners[5], Corners[6], LineColor);
            Renderer->AddLine(Corners[6], Corners[7], LineColor);
            Renderer->AddLine(Corners[7], Corners[4], LineColor);
            Renderer->AddLine(Corners[0], Corners[4], LineColor);
            Renderer->AddLine(Corners[1], Corners[5], LineColor);
            Renderer->AddLine(Corners[2], Corners[6], LineColor);
            Renderer->AddLine(Corners[3], Corners[7], LineColor);
        }

        // Capsule (Sphyl) Elements
        for (const FKSphylElem& Elem : Body->AggGeom.SphylElems)
        {
            // Capsule의 World Transform (BoneTM 기준)
            FVector WorldCenter = BoneTM.TransformPosition(Elem.Center);
            FQuat WorldRotation = BoneTM.Rotation * Elem.Rotation;

            float Radius = Elem.Radius * CmToM;
            float HalfHeight = Elem.Length * 0.5f * CmToM;

            // 캡슐 축
            FVector XAxis = WorldRotation.RotateVector(FVector(1, 0, 0));
            FVector YAxis = WorldRotation.RotateVector(FVector(0, 1, 0));
            FVector ZAxis = WorldRotation.RotateVector(FVector(0, 0, 1));

            FVector TopCenter = WorldCenter + ZAxis * HalfHeight;
            FVector BottomCenter = WorldCenter - ZAxis * HalfHeight;

            // 상단/하단 원
            for (int32 i = 0; i < NumSegments; ++i)
            {
                float Angle1 = (float(i) / NumSegments) * 2.0f * PI;
                float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI;
                FVector Offset1 = XAxis * cos(Angle1) * Radius + YAxis * sin(Angle1) * Radius;
                FVector Offset2 = XAxis * cos(Angle2) * Radius + YAxis * sin(Angle2) * Radius;
                Renderer->AddLine(TopCenter + Offset1, TopCenter + Offset2, LineColor);
                Renderer->AddLine(BottomCenter + Offset1, BottomCenter + Offset2, LineColor);
            }

            // 4개 세로선
            Renderer->AddLine(TopCenter + XAxis * Radius, BottomCenter + XAxis * Radius, LineColor);
            Renderer->AddLine(TopCenter - XAxis * Radius, BottomCenter - XAxis * Radius, LineColor);
            Renderer->AddLine(TopCenter + YAxis * Radius, BottomCenter + YAxis * Radius, LineColor);
            Renderer->AddLine(TopCenter - YAxis * Radius, BottomCenter - YAxis * Radius, LineColor);

            // 상단 반구 (XZ, YZ 평면 반원)
            int32 HalfSegments = NumSegments / 2;
            float AngleStep = PI / static_cast<float>(HalfSegments);

            // 상단 반구 - XZ 평면 반원
            FVector PrevTop = TopCenter + XAxis * Radius;
            for (int32 i = 1; i <= HalfSegments; ++i)
            {
                float Angle = AngleStep * i;
                FVector CurrTop = TopCenter + XAxis * (Radius * std::cos(Angle)) + ZAxis * (Radius * std::sin(Angle));
                Renderer->AddLine(PrevTop, CurrTop, LineColor);
                PrevTop = CurrTop;
            }

            // 상단 반구 - YZ 평면 반원
            PrevTop = TopCenter + YAxis * Radius;
            for (int32 i = 1; i <= HalfSegments; ++i)
            {
                float Angle = AngleStep * i;
                FVector CurrTop = TopCenter + YAxis * (Radius * std::cos(Angle)) + ZAxis * (Radius * std::sin(Angle));
                Renderer->AddLine(PrevTop, CurrTop, LineColor);
                PrevTop = CurrTop;
            }

            // 하단 반구 - XZ 평면 반원
            FVector PrevBottom = BottomCenter + XAxis * Radius;
            for (int32 i = 1; i <= HalfSegments; ++i)
            {
                float Angle = AngleStep * i;
                FVector CurrBottom = BottomCenter + XAxis * (Radius * std::cos(Angle)) - ZAxis * (Radius * std::sin(Angle));
                Renderer->AddLine(PrevBottom, CurrBottom, LineColor);
                PrevBottom = CurrBottom;
            }

            // 하단 반구 - YZ 평면 반원
            PrevBottom = BottomCenter + YAxis * Radius;
            for (int32 i = 1; i <= HalfSegments; ++i)
            {
                float Angle = AngleStep * i;
                FVector CurrBottom = BottomCenter + YAxis * (Radius * std::cos(Angle)) - ZAxis * (Radius * std::sin(Angle));
                Renderer->AddLine(PrevBottom, CurrBottom, LineColor);
                PrevBottom = CurrBottom;
            }
        }
    }
}

void USkeletalMeshComponent::UpdateKinematicBonesToAnim()
{
    // 애니메이션 포즈를 물리 Body에 텔레포트
    for (FBodyInstance* Body : Bodies)
    {
        if (!Body || Body->BoneIndex < 0)
        {
            continue;
        }
        
        FTransform BoneWorldTM = GetBoneWorldTransform(Body->BoneIndex);
        BoneWorldTM.Rotation.Normalize();

        // 물리 바디 이동
        Body->SetWorldTransform(BoneWorldTM, true);
    }
}

void USkeletalMeshComponent::BlendPhysicsBones()
{
    if (!SkeletalMesh || BoneIndexToBodyCache.IsEmpty()) return; // 방어 코드 간소화

    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    const FTransform ComponentWorldTM = GetWorldTransform();
    FTransform WorldToComponent = ComponentWorldTM.Inverse();

    // 루트부터 순회하며 ComponentSpace 트랜스폼 계산
    // (Body가 있는 본은 물리에서, 없는 본은 부모 기준 로컬 포즈 유지)
    for (int32 BoneIdx = 0; BoneIdx < Skeleton->Bones.Num(); ++BoneIdx)
    {
        FBodyInstance* Body = BoneIndexToBodyCache[BoneIdx];
        if (Body)
        {
            // Body가 있는 본: 물리 시뮬레이션에서 트랜스폼 획득
            FTransform BodyWorldTM = Body->GetWorldTransform();
            FTransform BodyComponentTM = WorldToComponent * BodyWorldTM;

            // ComponentSpace 직접 설정
            CurrentComponentSpacePose[BoneIdx] = BodyComponentTM;

            // LocalSpace 역계산 (부모 기준)
            int32 ParentIndex = Skeleton->Bones[BoneIdx].ParentIndex;
            if (ParentIndex >= 0 && ParentIndex < CurrentComponentSpacePose.Num())
            {
                const FTransform& ParentComponentTM = CurrentComponentSpacePose[ParentIndex];
                CurrentLocalSpacePose[BoneIdx] = ParentComponentTM.GetRelativeTransform(BodyComponentTM);
            }
            else
            {
                // 루트 본인 경우
                CurrentLocalSpacePose[BoneIdx] = BodyComponentTM;
            }

            // RagdollPose에도 저장 (블렌딩용)
            if (BoneIdx < PhysicsResultPose.Num())
            {
                PhysicsResultPose[BoneIdx] = CurrentLocalSpacePose[BoneIdx];
            }
        }
        else
        {
            // Body가 없는 본: 원래 애니메이션 로컬 포즈 유지, ComponentSpace만 재계산
            int32 ParentIndex = Skeleton->Bones[BoneIdx].ParentIndex;
            if (ParentIndex >= 0 && ParentIndex < CurrentComponentSpacePose.Num())
            {
                // 부모의 ComponentSpace에 로컬 트랜스폼 적용
                CurrentComponentSpacePose[BoneIdx] = CurrentComponentSpacePose[ParentIndex].GetWorldTransform(CurrentLocalSpacePose[BoneIdx]);
            }
            else
            {
                // 루트 본인 경우
                CurrentComponentSpacePose[BoneIdx] = CurrentLocalSpacePose[BoneIdx];
            }

            // RagdollPose에도 저장 (블렌딩용)
            if (BoneIdx < PhysicsResultPose.Num())
            {
                PhysicsResultPose[BoneIdx] = CurrentLocalSpacePose[BoneIdx];
            }
        }
    }

    // 최종 스키닝 행렬 업데이트
    UpdateFinalSkinningMatrices();
}

void USkeletalMeshComponent::UpdatePhysicsBlend(float DeltaTime)
{
    switch (PhysicsBlendState)
    {
    case EPhysicsBlendState::BlendingIn:
        // 블렌드 알파 증가
        PhysicsBlendWeight += DeltaTime / PhysicsBlendDuration;
        if (PhysicsBlendWeight >= 1.0f)
        {
            PhysicsBlendWeight = 1.0f;
            PhysicsBlendState = EPhysicsBlendState::Active;
        }
        // 물리 → 본 동기화 후 블렌딩
        BlendPhysicsBones();
        BlendPhysicsPoses(PhysicsBlendWeight);
        break;

    case EPhysicsBlendState::BlendingOut:
        // 블렌드 알파 감소
        PhysicsBlendWeight -= DeltaTime / PhysicsBlendDuration;
        if (PhysicsBlendWeight <= 0.0f)
        {
            PhysicsBlendWeight = 0.0f;
            PhysicsBlendState = EPhysicsBlendState::Disabled;
        }
        BlendPhysicsPoses(PhysicsBlendWeight);
        break;

    case EPhysicsBlendState::Active:
        // 물리 → 본 동기화만 수행
        BlendPhysicsBones();
        break;

    case EPhysicsBlendState::Disabled:
        // 애니메이션 시스템이 처리 (TickComponent에서)
        break;
    }
}

void USkeletalMeshComponent::BlendPhysicsPoses(float Alpha)
{
    if (PhysicsBlendPoseSaved.Num() != CurrentLocalSpacePose.Num())
    {
        return;
    }

    for (int32 i = 0; i < CurrentLocalSpacePose.Num(); ++i)
    {
        FTransform& Current = CurrentLocalSpacePose[i];
        const FTransform& AnimPose = PhysicsBlendPoseSaved[i];

        // RagdollPose가 유효한 경우에만 블렌딩
        if (i < PhysicsResultPose.Num())
        {
            const FTransform& PhysPose = PhysicsResultPose[i];

            // 위치 선형 보간 (Lerp)
            Current.Translation = FVector::Lerp(AnimPose.Translation, PhysPose.Translation, Alpha);

            // 회전 구면 선형 보간 (Slerp)
            Current.Rotation = FQuat::Slerp(AnimPose.Rotation, PhysPose.Rotation, Alpha);

            // 스케일은 애니메이션 값 유지
            Current.Scale3D = AnimPose.Scale3D;
        }
    }
}
