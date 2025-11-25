#pragma once

#include <wrl/client.h>
#include "PrimitiveComponent.h"
#include "Source/Runtime/Engine/Particles/ParticleSystem.h"
#include "Source/Runtime/Engine/Particles/ParticleEmitterInstance.h"
#include "UParticleSystemComponent.generated.h"

struct FMeshBatchElement;
struct FSceneView;

UCLASS(DisplayName="파티클 시스템 컴포넌트", Description="파티클 시스템을 씬에 배치하는 컴포넌트입니다")
class UParticleSystemComponent : public UPrimitiveComponent
{
public:
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Particle System")
	UParticleSystem* Template = nullptr;

	UPROPERTY(EditAnywhere, Category="Particle System")
	bool bAutoActivate = true;

	// 이미터 인스턴스 (런타임)
	TArray<FParticleEmitterInstance*> EmitterInstances;

	// 렌더 데이터 (렌더링 스레드용)
	TArray<FDynamicEmitterDataBase*> EmitterRenderData;

	// 언리얼 엔진 호환: 인스턴스 파라미터 시스템
	// 게임플레이에서 파티클 속성을 동적으로 제어 가능
	struct FParticleParameter
	{
		FString Name;
		float FloatValue = 0.0f;
		FVector VectorValue = FVector(0.0f, 0.0f, 0.0f);
		FLinearColor ColorValue = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

		FParticleParameter() = default;
		explicit FParticleParameter(const FString& InName)
			: Name(InName)
		{
		}
	};

	TArray<FParticleParameter> InstanceParameters;

	// Dynamic Instance Buffer (메시 파티클 인스턴싱용)
	ID3D11Buffer* MeshInstanceBuffer = nullptr;
	uint32 AllocatedMeshInstanceCount = 0;

	// Dynamic Instance Buffer (스프라이트 파티클 인스턴싱용)
	ID3D11Buffer* SpriteInstanceBuffer = nullptr;
	uint32 AllocatedSpriteInstanceCount = 0;

	// Dynamic Vertex / Index Buffer (빔 파티클용)
	ID3D11Buffer* BeamVertexBuffer = nullptr;
	ID3D11Buffer* BeamIndexBuffer = nullptr;
	uint32 AllocatedBeamVertexCount = 0;
	uint32 AllocatedBeamIndexCount = 0;

	// Shared Quad Mesh (스프라이트 인스턴싱용)
	// ComPtr + static inline: 프로그램 종료 시 자동 해제, cpp 정의 불필요
	static inline Microsoft::WRL::ComPtr<ID3D11Buffer> SpriteQuadVertexBuffer;
	static inline Microsoft::WRL::ComPtr<ID3D11Buffer> SpriteQuadIndexBuffer;
	static inline bool bQuadBuffersInitialized = false;
	static void InitializeQuadBuffers();

	UParticleSystemComponent();
	virtual ~UParticleSystemComponent();

	// 초기화/정리
	virtual void OnRegister(UWorld* InWorld) override;    // 에디터/PIE 모두에서 호출
	virtual void OnUnregister() override;                 // 에디터/PIE 모두에서 호출

	// 틱
	virtual void TickComponent(float DeltaTime) override;

	// 활성화/비활성화
	void ActivateSystem();
	void DeactivateSystem();
	void ResetParticles();

	// 템플릿 설정
	void SetTemplate(UParticleSystem* NewTemplate);

	// 언리얼 엔진 호환: 인스턴스 파라미터 제어
	void SetFloatParameter(const FString& ParameterName, float Value);
	void SetVectorParameter(const FString& ParameterName, const FVector& Value);
	void SetColorParameter(const FString& ParameterName, const FLinearColor& Value);

	float GetFloatParameter(const FString& ParameterName, float DefaultValue = 0.0f) const;
	FVector GetVectorParameter(const FString& ParameterName, const FVector& DefaultValue = FVector(0.0f, 0.0f, 0.0f)) const;
	FLinearColor GetColorParameter(const FString& ParameterName, const FLinearColor& DefaultValue = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)) const;

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// PIE 복사 시 포인터 배열 초기화
	virtual void DuplicateSubObjects() override;

	void CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View) override;

	// 메시 파티클 인스턴싱
	void FillMeshInstanceBuffer(uint32 TotalInstances);
	void CreateMeshParticleBatch(TArray<FMeshBatchElement>& OutMeshBatchElements);

	// 스프라이트 파티클 인스턴싱
	void FillSpriteInstanceBuffer(uint32 TotalInstances);
	void CreateSpriteParticleBatch(TArray<FMeshBatchElement>& OutMeshBatchElements);

	// 빔 파티클 렌더링
	void FillBeamBuffers(const FSceneView* View);
	void CreateBeamParticleBatch(TArray<FMeshBatchElement>& OutMeshBatchElements);

private:
	void InitializeEmitterInstances();
	void ClearEmitterInstances();
	void UpdateRenderData();

	// === 테스트용 리소스 (디버그 함수에서 생성, Component가 소유) ===
	UParticleSystem* TestTemplate = nullptr;
	TArray<UMaterialInterface*> TestMaterials;
	void CleanupTestResources();

	// 테스트용 디버그 파티클 시스템 생성
	void CreateDebugMeshParticleSystem();	// 메시 파티클 테스트
	void CreateDebugSpriteParticleSystem();	// 스프라이트 파티클 테스트
	void CreateDebugBeamParticleSystem();	// 빔 파티클 테스트
};
