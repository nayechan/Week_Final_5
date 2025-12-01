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
#include "ConstraintInstance.h"
#include "PhysicsAsset.h"
#include "PhysicsScene.h"
#include "SkeletalBodySetup.h"
#include "PhysicsConstraintTemplate.h"
#include "World.h"
#include "InputManager.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
    // Keep constructor lightweight for editor/viewer usage.
    // Load a simple default test mesh if available; viewer UI can override.
    SetSkeletalMesh(GDataDir + "/DancingRacer.fbx");
    // TODO - 애니메이션 나중에 써먹으세요
    
	UAnimationAsset* AnimationAsset = UResourceManager::GetInstance().Get<UAnimSequence>("Data/DancingRacer_mixamo.com");
    PlayAnimation(AnimationAsset, true, 1.f);
    
}


void USkeletalMeshComponent::BeginPlay()
{
    Super::BeginPlay();
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
    Super::TickComponent(DeltaTime);

    if (!SkeletalMesh) { return; }

    // Ragdoll 테스트 모드: R키로 토글
    if (bRagdollTestMode)
    {
        UInputManager& Input = UInputManager::GetInstance();
        if (Input.IsKeyPressed('R'))  // R키
        {
            if (RagdollState == ERagdollState::Disabled)
            {
                UE_LOG("[Ragdoll Test] R key pressed - Starting Ragdoll");
                StartRagdoll(false);  // 즉시 전환
            }
            else
            {
                UE_LOG("[Ragdoll Test] R key pressed - Stopping Ragdoll");
                StopRagdoll(false);  // 즉시 전환
            }
        }

        // Space키로 위쪽 임펄스 테스트
        if (Input.IsKeyPressed(VK_SPACE) && IsRagdollActive())
        {
            UE_LOG("[Ragdoll Test] Space key pressed - Adding upward impulse");
            for (FBodyInstance* Body : RagdollBodies)
            {
                if (Body)
                {
                    Body->AddImpulse(FVector(0, 0, 5.0f));  // 위쪽 임펄스
                }
            }
        }
    }

    // Ragdoll이 활성화된 경우 물리 시뮬레이션 처리
    if (RagdollState != ERagdollState::Disabled)
    {
        UpdateRagdollState(DeltaTime);
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
        return; // skip test code when animation is active
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

        // 메시의 PhysicsAsset 동기화
        SyncPhysicsAssetFromMesh();
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

void USkeletalMeshComponent::SyncPhysicsAssetFromMesh()
{
    if (SkeletalMesh)
    {
        PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
    }
    else
    {
        PhysicsAsset = nullptr;
    }
}

void USkeletalMeshComponent::OnPropertyChanged(const FProperty& Property)
{
    Super::OnPropertyChanged(Property);

    // PhysicsAsset 프로퍼티가 변경되면 메시에도 반영
    if (strcmp(Property.Name, "PhysicsAsset") == 0)
    {
        if (SkeletalMesh)
        {
            SkeletalMesh->SetPhysicsAsset(PhysicsAsset);
        }
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

FTransform USkeletalMeshComponent::GetBoneWorldTransform(int32 BoneIndex)
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
    // PerformSkinning은 CollectMeshBatches에서 전역 모드에 따라 수행됨
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

// ===========================
// Ragdoll Physics Implementation
// ===========================

void USkeletalMeshComponent::InitRagdoll()
{
    // 기존 Ragdoll 정리
    TermRagdoll();

    if (!SkeletalMesh)
    {
        UE_LOG("[Ragdoll] InitRagdoll failed: SkeletalMesh is null");
        return;
    }

    UPhysicsAsset* PhysAsset = SkeletalMesh->GetPhysicsAsset();
    if (!PhysAsset || !PhysAsset->IsValid())
    {
        UE_LOG("[Ragdoll] InitRagdoll failed: PhysicsAsset is null or invalid");
        return;
    }

    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    if (!Skeleton)
    {
        UE_LOG("[Ragdoll] InitRagdoll failed: Skeleton is null");
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG("[Ragdoll] InitRagdoll failed: World is null");
        return;
    }

    FPhysicsScene* PhysScene = World->GetPhysicsScene();
    if (!PhysScene)
    {
        UE_LOG("[Ragdoll] InitRagdoll failed: PhysicsScene is null");
        return;
    }

    // Body 생성
    const int32 NumBodies = PhysAsset->GetBodySetupCount();
    RagdollBodies.Reserve(NumBodies);

    for (int32 i = 0; i < NumBodies; ++i)
    {
        USkeletalBodySetup* BodySetup = PhysAsset->GetBodySetup(i);
        if (!BodySetup)
        {
            continue;
        }

        // 본 인덱스 찾기
        const FString BoneNameStr = BodySetup->BoneName.ToString();
        const int32* BoneIndexPtr = Skeleton->BoneNameToIndex.Find(BoneNameStr);
        if (!BoneIndexPtr)
        {
            UE_LOG("[Ragdoll] Bone not found: %s", BoneNameStr.c_str());
            continue;
        }
        int32 BoneIndex = *BoneIndexPtr;

        // Body Instance 생성
        FBodyInstance* Body = new FBodyInstance();
        Body->BoneName = BodySetup->BoneName;
        Body->BoneIndex = BoneIndex;

        // 본의 현재 월드 트랜스폼으로 초기화
        FTransform BoneWorldTM = GetBoneWorldTransform(BoneIndex);
        Body->InitBody(BodySetup, BoneWorldTM, this, PhysScene);

        // 컴포넌트의 BodyInstance에서 전역 설정 적용
        Body->SetEnableGravity(BodyInstance.IsEnabledGravity());

        // DOF Lock 설정 복사
        bool bLockLinearX, bLockLinearY, bLockLinearZ;
        bool bLockAngularX, bLockAngularY, bLockAngularZ;
        BodyInstance.GetLinearLock(bLockLinearX, bLockLinearY, bLockLinearZ);
        BodyInstance.GetAngularLock(bLockAngularX, bLockAngularY, bLockAngularZ);
        Body->SetLinearLock(bLockLinearX, bLockLinearY, bLockLinearZ);
        Body->SetAngularLock(bLockAngularX, bLockAngularY, bLockAngularZ);

        // 초기 상태는 Kinematic (비활성)
        Body->SetSimulatePhysics(false);

        int32 BodyIndex = RagdollBodies.Add(Body);
        BoneNameToBodyIndex.Add(BodySetup->BoneName, BodyIndex);
    }

    // Constraint 생성
    CreateRagdollConstraints();

    // 블렌딩용 포즈 배열 초기화
    const int32 NumBones = Skeleton->Bones.Num();
    PreRagdollPose.SetNum(NumBones);
    RagdollPose.SetNum(NumBones);

    UE_LOG("[Ragdoll] InitRagdoll completed: %d bodies, %d constraints",
        RagdollBodies.Num(), RagdollConstraints.Num());
}

void USkeletalMeshComponent::TermRagdoll()
{
    // 상태 초기화
    RagdollState = ERagdollState::Disabled;
    RagdollBlendAlpha = 0.0f;

    // Constraints 해제 (Joint 먼저 해제해야 Body 해제 시 오류 방지)
    for (FConstraintInstance* Constraint : RagdollConstraints)
    {
        if (Constraint)
        {
            Constraint->TermConstraint();
            delete Constraint;
        }
    }
    RagdollConstraints.Empty();

    // Bodies 해제
    for (FBodyInstance* Body : RagdollBodies)
    {
        if (Body)
        {
            Body->TermBody();
            delete Body;
        }
    }
    RagdollBodies.Empty();

    // 인덱스 맵 초기화
    BoneNameToBodyIndex.Empty();

    // 블렌딩 포즈 초기화
    PreRagdollPose.Empty();
    RagdollPose.Empty();

    // 애니메이션 재활성화
    bUseAnimation = true;

    UE_LOG("[Ragdoll] TermRagdoll: All bodies and constraints released");
}

void USkeletalMeshComponent::CreateRagdollConstraints()
{
    if (!SkeletalMesh)
    {
        return;
    }

    UPhysicsAsset* PhysAsset = SkeletalMesh->GetPhysicsAsset();
    if (!PhysAsset)
    {
        return;
    }

    const int32 NumConstraints = PhysAsset->GetConstraintCount();
    RagdollConstraints.Reserve(NumConstraints);

    for (int32 i = 0; i < NumConstraints; ++i)
    {
        UPhysicsConstraintTemplate* Template = PhysAsset->ConstraintSetup[i];
        if (!Template)
        {
            continue;
        }

        // 연결된 두 Body 찾기
        const int32* BodyIdx1Ptr = BoneNameToBodyIndex.Find(Template->GetBone1Name());
        const int32* BodyIdx2Ptr = BoneNameToBodyIndex.Find(Template->GetBone2Name());

        if (!BodyIdx1Ptr || !BodyIdx2Ptr)
        {
            UE_LOG("[Ragdoll] Constraint bones not found: %s <-> %s",
                Template->GetBone1Name().ToString().c_str(), Template->GetBone2Name().ToString().c_str());
            continue;
        }

        FBodyInstance* Body1 = RagdollBodies[*BodyIdx1Ptr];
        FBodyInstance* Body2 = RagdollBodies[*BodyIdx2Ptr];

        if (!Body1 || !Body2)
        {
            continue;
        }

        // Constraint Instance 생성 (템플릿 설정 복사)
        FConstraintInstance* Constraint = new FConstraintInstance(Template->DefaultInstance);

        // Joint 초기화 (동적 Frame 계산 사용)
        // Twist 축(X축)이 부모→자식 뼈 방향으로 자동 정렬됨
        Constraint->InitConstraint(Body1, Body2, this);
        RagdollConstraints.Add(Constraint);
    }

    UE_LOG("[Ragdoll] CreateRagdollConstraints: %d constraints created", RagdollConstraints.Num());
}

void USkeletalMeshComponent::StartRagdoll(bool bBlend)
{
    // 이미 Active면 무시
    if (RagdollState == ERagdollState::Active)
    {
        return;
    }

    // Bodies가 없으면 초기화
    if (RagdollBodies.IsEmpty())
    {
        // PhysicsAsset이 없으면 자동 생성
        if (!SkeletalMesh || !SkeletalMesh->GetPhysicsAsset() || !SkeletalMesh->GetPhysicsAsset()->IsValid())
        {
            UE_LOG("[Ragdoll] No PhysicsAsset found, creating default...");
            CreateDefaultPhysicsAsset();
        }

        InitRagdoll();
        if (RagdollBodies.IsEmpty())
        {
            UE_LOG("[Ragdoll] StartRagdoll failed: No bodies created");
            return;
        }
    }

    // 현재 애니메이션 포즈 저장 (블렌딩용)
    PreRagdollPose = CurrentLocalSpacePose;

    // Body를 현재 애니메이션 위치로 텔레포트
    SyncRagdollToAnimation();

    // 모든 Body를 Dynamic으로 전환
    for (FBodyInstance* Body : RagdollBodies)
    {
        if (Body)
        {
            Body->SetSimulatePhysics(true);
            Body->WakeUp();
        }
    }

    // 상태 전환
    if (bBlend)
    {
        RagdollState = ERagdollState::BlendingIn;
        RagdollBlendAlpha = 0.0f;
    }
    else
    {
        RagdollState = ERagdollState::Active;
        RagdollBlendAlpha = 1.0f;
    }

    // 애니메이션 비활성화
    bUseAnimation = false;

    UE_LOG("[Ragdoll] StartRagdoll: state=%d, blend=%s",
        static_cast<int>(RagdollState), bBlend ? "true" : "false");
}

void USkeletalMeshComponent::StopRagdoll(bool bBlend)
{
    // 이미 Disabled면 무시
    if (RagdollState == ERagdollState::Disabled)
    {
        return;
    }

    // 모든 Body를 Kinematic으로 전환 (재사용)
    for (FBodyInstance* Body : RagdollBodies)
    {
        if (Body)
        {
            Body->SetSimulatePhysics(false);
        }
    }

    // 상태 전환
    if (bBlend)
    {
        RagdollState = ERagdollState::BlendingOut;
        RagdollBlendAlpha = 1.0f;
    }
    else
    {
        RagdollState = ERagdollState::Disabled;
        RagdollBlendAlpha = 0.0f;
    }

    // 애니메이션 재활성화
    bUseAnimation = true;

    UE_LOG("[Ragdoll] StopRagdoll: state=%d, blend=%s",
        static_cast<int>(RagdollState), bBlend ? "true" : "false");
}

void USkeletalMeshComponent::SyncRagdollToAnimation()
{
    // 애니메이션 포즈를 물리 Body에 텔레포트
    for (FBodyInstance* Body : RagdollBodies)
    {
        if (!Body || Body->BoneIndex < 0)
        {
            continue;
        }

        FTransform BoneWorldTM = GetBoneWorldTransform(Body->BoneIndex);
        Body->SetWorldTransform(BoneWorldTM, true);  // bTeleport = true
    }
}

void USkeletalMeshComponent::SyncAnimationToRagdoll()
{
    // 물리 Body 위치를 본 트랜스폼으로 동기화
    if (!SkeletalMesh)
    {
        return;
    }

    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    if (!Skeleton)
    {
        return;
    }

    // 컴포넌트 월드 트랜스폼의 역행렬 (월드 → 컴포넌트 로컬 변환용)
    const FTransform ComponentWorldTM = GetWorldTransform();
    const FTransform InvComponentTM = ComponentWorldTM.Inverse();

    // 본 인덱스 → Body 인스턴스 매핑 구축
    TMap<int32, FBodyInstance*> BoneToBody;
    for (FBodyInstance* Body : RagdollBodies)
    {
        if (Body && Body->BoneIndex >= 0)
        {
            BoneToBody.Add(Body->BoneIndex, Body);
        }
    }

    // 루트부터 순회하며 ComponentSpace 트랜스폼 계산
    // (Body가 있는 본은 물리에서, 없는 본은 부모 기준 로컬 포즈 유지)
    for (int32 BoneIdx = 0; BoneIdx < Skeleton->Bones.Num(); ++BoneIdx)
    {
        if (BoneIdx >= CurrentLocalSpacePose.Num() || BoneIdx >= CurrentComponentSpacePose.Num())
        {
            continue;
        }

        FBodyInstance** BodyPtr = BoneToBody.Find(BoneIdx);

        if (BodyPtr && *BodyPtr)
        {
            // Body가 있는 본: 물리 시뮬레이션에서 트랜스폼 획득
            FBodyInstance* Body = *BodyPtr;
            FTransform BodyWorldTM = Body->GetWorldTransform();
            FTransform BodyComponentTM = InvComponentTM.GetWorldTransform(BodyWorldTM);

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
            if (BoneIdx < RagdollPose.Num())
            {
                RagdollPose[BoneIdx] = CurrentLocalSpacePose[BoneIdx];
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
            if (BoneIdx < RagdollPose.Num())
            {
                RagdollPose[BoneIdx] = CurrentLocalSpacePose[BoneIdx];
            }
        }
    }

    // 최종 스키닝 행렬 업데이트
    UpdateFinalSkinningMatrices();
}

void USkeletalMeshComponent::UpdateRagdollState(float DeltaTime)
{
    switch (RagdollState)
    {
    case ERagdollState::BlendingIn:
        // 블렌드 알파 증가
        RagdollBlendAlpha += DeltaTime / RagdollBlendDuration;
        if (RagdollBlendAlpha >= 1.0f)
        {
            RagdollBlendAlpha = 1.0f;
            RagdollState = ERagdollState::Active;
        }
        // 물리 → 본 동기화 후 블렌딩
        SyncAnimationToRagdoll();
        BlendPoses(RagdollBlendAlpha);
        break;

    case ERagdollState::BlendingOut:
        // 블렌드 알파 감소
        RagdollBlendAlpha -= DeltaTime / RagdollBlendDuration;
        if (RagdollBlendAlpha <= 0.0f)
        {
            RagdollBlendAlpha = 0.0f;
            RagdollState = ERagdollState::Disabled;
        }
        BlendPoses(RagdollBlendAlpha);
        break;

    case ERagdollState::Active:
        // 물리 → 본 동기화만 수행
        SyncAnimationToRagdoll();
        break;

    case ERagdollState::Disabled:
        // 애니메이션 시스템이 처리 (TickComponent에서)
        break;
    }
}

void USkeletalMeshComponent::BlendPoses(float Alpha)
{
    if (PreRagdollPose.Num() != CurrentLocalSpacePose.Num())
    {
        return;
    }

    for (int32 i = 0; i < CurrentLocalSpacePose.Num(); ++i)
    {
        FTransform& Current = CurrentLocalSpacePose[i];
        const FTransform& AnimPose = PreRagdollPose[i];

        // RagdollPose가 유효한 경우에만 블렌딩
        if (i < RagdollPose.Num())
        {
            const FTransform& PhysPose = RagdollPose[i];

            // 위치 선형 보간 (Lerp)
            Current.Translation = FVector::Lerp(AnimPose.Translation, PhysPose.Translation, Alpha);

            // 회전 구면 선형 보간 (Slerp)
            Current.Rotation = FQuat::Slerp(AnimPose.Rotation, PhysPose.Rotation, Alpha);

            // 스케일은 애니메이션 값 유지
            Current.Scale3D = AnimPose.Scale3D;
        }
    }

    // 포즈 재계산
    ForceRecomputePose();
}

void USkeletalMeshComponent::AddImpulseToBody(const FName& BoneName, const FVector& Impulse)
{
    // 본 이름으로 Body 인덱스 찾기
    const int32* BodyIndexPtr = BoneNameToBodyIndex.Find(BoneName);
    if (!BodyIndexPtr)
    {
        UE_LOG("[Ragdoll] AddImpulseToBody: Body not found for bone %s", BoneName.ToString().c_str());
        return;
    }

    int32 BodyIndex = *BodyIndexPtr;
    if (BodyIndex < 0 || BodyIndex >= RagdollBodies.Num())
    {
        return;
    }

    FBodyInstance* Body = RagdollBodies[BodyIndex];
    if (Body)
    {
        Body->AddImpulse(Impulse);
    }
}

void USkeletalMeshComponent::AddForceToBody(const FName& BoneName, const FVector& Force)
{
    // 본 이름으로 Body 인덱스 찾기
    const int32* BodyIndexPtr = BoneNameToBodyIndex.Find(BoneName);
    if (!BodyIndexPtr)
    {
        UE_LOG("[Ragdoll] AddForceToBody: Body not found for bone %s", BoneName.ToString().c_str());
        return;
    }

    int32 BodyIndex = *BodyIndexPtr;
    if (BodyIndex < 0 || BodyIndex >= RagdollBodies.Num())
    {
        return;
    }

    FBodyInstance* Body = RagdollBodies[BodyIndex];
    if (Body)
    {
        Body->AddForce(Force);
    }
}

// 헬퍼: 래그돌에서 제외할 본인지 확인
static bool ShouldExcludeFromRagdoll(const FString& BoneName)
{
    FString BoneNameLower = BoneName;
    std::transform(BoneNameLower.begin(), BoneNameLower.end(), BoneNameLower.begin(), ::tolower);

    // 손가락 관련 본 제외
    if (BoneNameLower.find("finger") != FString::npos) return true;
    if (BoneNameLower.find("thumb") != FString::npos) return true;
    if (BoneNameLower.find("index") != FString::npos) return true;
    if (BoneNameLower.find("middle") != FString::npos) return true;
    if (BoneNameLower.find("ring") != FString::npos) return true;
    if (BoneNameLower.find("pinky") != FString::npos) return true;

    // 발가락 관련 본 제외
    if (BoneNameLower.find("toe") != FString::npos) return true;

    // 얼굴 본 제외 (눈, 턱 등)
    if (BoneNameLower.find("eye") != FString::npos) return true;
    if (BoneNameLower.find("jaw") != FString::npos) return true;
    if (BoneNameLower.find("tongue") != FString::npos) return true;
    if (BoneNameLower.find("lip") != FString::npos) return true;
    if (BoneNameLower.find("brow") != FString::npos) return true;
    if (BoneNameLower.find("eyelid") != FString::npos) return true;
    if (BoneNameLower.find("cheek") != FString::npos) return true;
    if (BoneNameLower.find("nose") != FString::npos) return true;

    // 기타 작은 본 제외
    if (BoneNameLower.find("twist") != FString::npos) return true;   // 트위스트 본
    if (BoneNameLower.find("roll") != FString::npos) return true;    // 롤 본
    if (BoneNameLower.find("helper") != FString::npos) return true;  // 헬퍼 본
    if (BoneNameLower.find("ik_") != FString::npos) return true;     // IK 본
    if (BoneNameLower.find("_ik") != FString::npos) return true;

    return false;
}

void USkeletalMeshComponent::CreateDefaultPhysicsAsset()
{
    if (!SkeletalMesh)
    {
        UE_LOG("[Ragdoll] CreateDefaultPhysicsAsset failed: SkeletalMesh is null");
        return;
    }

    // 이미 PhysicsAsset이 있으면 건너뛰기
    if (SkeletalMesh->GetPhysicsAsset() && SkeletalMesh->GetPhysicsAsset()->IsValid())
    {
        UE_LOG("[Ragdoll] PhysicsAsset already exists");
        return;
    }

    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    if (!Skeleton || Skeleton->Bones.Num() == 0)
    {
        UE_LOG("[Ragdoll] CreateDefaultPhysicsAsset failed: Skeleton is empty");
        return;
    }

    UE_LOG("[Ragdoll] Creating default PhysicsAsset for %d bones...", Skeleton->Bones.Num());

    // PhysicsAsset 생성
    UPhysicsAsset* PhysAsset = NewObject<UPhysicsAsset>();

    // Body를 생성할 본 인덱스 목록
    TArray<int32> BodyBoneIndices;

    // 각 본에 대해 BodySetup 생성 (주요 본만)
    for (int32 BoneIdx = 0; BoneIdx < Skeleton->Bones.Num(); ++BoneIdx)
    {
        const FBone& Bone = Skeleton->Bones[BoneIdx];

        // 래그돌에서 제외할 본인지 확인
        if (ShouldExcludeFromRagdoll(Bone.Name))
        {
            continue;
        }

        // BodySetup 생성
        USkeletalBodySetup* BodySetup = NewObject<USkeletalBodySetup>();
        BodySetup->BoneName = FName(Bone.Name);

        // 본 길이 추정 (자식 본까지의 거리 기반)
        float BoneLength = 10.0f;  // 기본 길이
        float BoneRadius = 3.0f;   // 기본 반지름

        // 자식 본 찾아서 길이 계산
        for (int32 ChildIdx = 0; ChildIdx < Skeleton->Bones.Num(); ++ChildIdx)
        {
            if (Skeleton->Bones[ChildIdx].ParentIndex == BoneIdx)
            {
                // 자식 본의 로컬 위치로부터 본 길이 추정
                FTransform ChildLocal = FTransform(Skeleton->Bones[ChildIdx].BindPose * Bone.InverseBindPose);
                float Dist = ChildLocal.Translation.Size();
                if (Dist > BoneLength)
                {
                    BoneLength = Dist;
                }
            }
        }

        // 본 이름에 따라 Shape 크기 조절
        FString BoneNameLower = Bone.Name;
        std::transform(BoneNameLower.begin(), BoneNameLower.end(), BoneNameLower.begin(), ::tolower);

        if (BoneNameLower.find("head") != FString::npos || BoneNameLower.find("skull") != FString::npos)
        {
            // 머리: 구 사용
            BodySetup->AddSphereElem(8.0f);
        }
        else if (BoneNameLower.find("hand") != FString::npos || BoneNameLower.find("foot") != FString::npos)
        {
            // 손/발: 작은 구
            BodySetup->AddSphereElem(3.0f);
        }
        else if (BoneNameLower.find("spine") != FString::npos || BoneNameLower.find("pelvis") != FString::npos || BoneNameLower.find("hip") != FString::npos)
        {
            // 척추/골반: 큰 캡슐
            BodySetup->AddCapsuleElem(6.0f, BoneLength * 0.4f);
        }
        else if (BoneNameLower.find("arm") != FString::npos || BoneNameLower.find("leg") != FString::npos ||
                 BoneNameLower.find("thigh") != FString::npos || BoneNameLower.find("calf") != FString::npos ||
                 BoneNameLower.find("forearm") != FString::npos || BoneNameLower.find("upperarm") != FString::npos ||
                 BoneNameLower.find("shin") != FString::npos || BoneNameLower.find("clavicle") != FString::npos ||
                 BoneNameLower.find("shoulder") != FString::npos)
        {
            // 팔/다리: 중간 캡슐
            BodySetup->AddCapsuleElem(4.0f, BoneLength * 0.4f);
        }
        else if (BoneNameLower.find("neck") != FString::npos)
        {
            // 목: 작은 캡슐
            BodySetup->AddCapsuleElem(3.0f, BoneLength * 0.3f);
        }
        else
        {
            // 기타: 기본 캡슐
            BodySetup->AddCapsuleElem(BoneRadius, FMath::Max(5.0f, BoneLength * 0.3f));
        }

        // PhysicsType을 Kinematic으로 설정 (Ragdoll 시작 전까지)
        BodySetup->PhysicsType = EPhysicsType::Kinematic;

        PhysAsset->AddBodySetup(BodySetup);
        BodyBoneIndices.Add(BoneIdx);
    }

    // Body가 있는 본들 사이에만 Constraint 생성
    for (int32 i = 0; i < BodyBoneIndices.Num(); ++i)
    {
        int32 BoneIdx = BodyBoneIndices[i];
        const FBone& Bone = Skeleton->Bones[BoneIdx];

        // 부모 Body 찾기 (직접 부모가 아닐 수 있음 - 부모가 제외된 경우)
        int32 ParentBoneIdx = Bone.ParentIndex;
        FName ParentBodyName = "None";

        // 부모 체인을 따라가며 Body가 있는 본 찾기
        while (ParentBoneIdx >= 0)
        {
            const FBone& ParentBone = Skeleton->Bones[ParentBoneIdx];

            // 이 본에 Body가 있는지 확인
            if (PhysAsset->FindBodySetupIndex(FName(ParentBone.Name)) >= 0)
            {
                ParentBodyName = FName(ParentBone.Name);
                break;
            }

            // 부모의 부모로 이동
            ParentBoneIdx = ParentBone.ParentIndex;
        }

        // 부모 Body가 없으면 Constraint 생성 안함 (루트)
        if (ParentBodyName == "None")
        {
            continue;
        }

        // Constraint 템플릿 생성
        UPhysicsConstraintTemplate* ConstraintTemplate = NewObject<UPhysicsConstraintTemplate>();

        // 조인트 설정
        FConstraintInstance& CI = ConstraintTemplate->DefaultInstance;
        CI.JointName = FName(Bone.Name + "_Joint");
        CI.ConstraintBone1 = ParentBodyName;      // 부모 Body 본
        CI.ConstraintBone2 = FName(Bone.Name);    // 자식 본

        // 선형 제한: 모두 잠금 (관절은 늘어나지 않음)
        CI.LinearXMotion = ELinearConstraintMotion::Locked;
        CI.LinearYMotion = ELinearConstraintMotion::Locked;
        CI.LinearZMotion = ELinearConstraintMotion::Locked;

        // 각도 제한: 제한된 회전 허용
        CI.AngularTwistMotion = EAngularConstraintMotion::Limited;
        CI.AngularSwing1Motion = EAngularConstraintMotion::Limited;
        CI.AngularSwing2Motion = EAngularConstraintMotion::Limited;

        // 기본 각도 제한값 (본 이름에 따라 조절 가능)
        CI.TwistLimitAngle = 30.0f;
        CI.Swing1LimitAngle = 45.0f;
        CI.Swing2LimitAngle = 45.0f;

        // 인접 본 간 충돌 비활성화 (래그돌 기본값)
        CI.bDisableCollision = true;

        PhysAsset->AddConstraint(ConstraintTemplate);

        // 인접 바디 충돌 비활성화
        int32 ParentBodyIdx = PhysAsset->FindBodySetupIndex(ParentBodyName);
        int32 ChildBodyIdx = PhysAsset->FindBodySetupIndex(FName(Bone.Name));
        if (ParentBodyIdx >= 0 && ChildBodyIdx >= 0)
        {
            PhysAsset->DisableCollision(ParentBodyIdx, ChildBodyIdx);
        }
    }

    // SkeletalMesh에 PhysicsAsset 설정
    SkeletalMesh->SetPhysicsAsset(PhysAsset);

    UE_LOG("[Ragdoll] Default PhysicsAsset created: %d bodies, %d constraints (excluded %d bones)",
        PhysAsset->GetBodySetupCount(), PhysAsset->GetConstraintCount(),
        Skeleton->Bones.Num() - BodyBoneIndices.Num());
}