#include "pch.h"
#include "SkinnedMeshComponent.h"
#include "MeshBatchElement.h"
#include "SceneView.h"
#include "SkinningStats.h"
#include "PlatformTime.h"

USkinnedMeshComponent::USkinnedMeshComponent() : SkeletalMesh(nullptr)
{
   bCanEverTick = true;
}

USkinnedMeshComponent::~USkinnedMeshComponent()
{
   if (VertexBuffer)
   {
      VertexBuffer->Release();
      VertexBuffer = nullptr;
   }

   // GPU 스키닝 버퍼 해제
   if (GPUSkinnedVertexBuffer)
   {
      GPUSkinnedVertexBuffer->Release();
      GPUSkinnedVertexBuffer = nullptr;
   }
   if (BoneMatricesBuffer)
   {
      BoneMatricesBuffer->Release();
      BoneMatricesBuffer = nullptr;
   }
}

void USkinnedMeshComponent::BeginPlay()
{
   Super::BeginPlay();
}

void USkinnedMeshComponent::TickComponent(float DeltaTime)
{
   UMeshComponent::TickComponent(DeltaTime);
}

void USkinnedMeshComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
   Super::Serialize(bInIsLoading, InOutHandle);

   if (bInIsLoading)
   {
      SetSkeletalMesh(SkeletalMesh->GetPathFileName());
   }
   // @TODO - UStaticMeshComponent처럼 프로퍼티 기반 직렬화 로직 추가
}

void USkinnedMeshComponent::DuplicateSubObjects()
{
   Super::DuplicateSubObjects();

   // 버퍼는 필요할 때 PerformSkinning()에서 생성되므로 여기서는 리셋만
   VertexBuffer = nullptr;
   GPUSkinnedVertexBuffer = nullptr;
   BoneMatricesBuffer = nullptr;
   CurrentBoneBufferSize = 0;

   // PIE 종료 후 스키닝 데이터를 다시 계산하도록 플래그 설정
   bSkinningMatricesDirty = true;
}

void USkinnedMeshComponent::CollectMeshBatches(TArray<FMeshBatchElement>& OutMeshBatchElements, const FSceneView* View)
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshData()) { return; }

   // 전역 스키닝 모드 체크 (언리얼 엔진 방식)
   ESkinningMode GlobalMode = View->RenderSettings->GetGlobalSkinningMode();
   const bool bUseGPU = (GlobalMode == ESkinningMode::ForceGPU);

   // 스키닝 모드가 바뀌었으면 기존 버퍼 정리
   if (bUseGPU != bLastFrameUsedGPU)
   {
      if (bUseGPU)
      {
         // CPU -> GPU 전환: CPU 버퍼 해제
         if (VertexBuffer)
         {
            VertexBuffer->Release();
            VertexBuffer = nullptr;
         }
      }
      else
      {
         // GPU -> CPU 전환: GPU 버퍼 해제
         if (GPUSkinnedVertexBuffer)
         {
            GPUSkinnedVertexBuffer->Release();
            GPUSkinnedVertexBuffer = nullptr;
         }
         if (BoneMatricesBuffer)
         {
            BoneMatricesBuffer->Release();
            BoneMatricesBuffer = nullptr;
         }
      }
      bSkinningMatricesDirty = true;  // 모드 변경 시 강제 업데이트
      bLastFrameUsedGPU = bUseGPU;
   }

   // 스키닝 데이터가 변경되었으면 업데이트
   if (bSkinningMatricesDirty)
   {
      PerformSkinning(bUseGPU);  // 전역 모드 적용하여 GPU/CPU 처리
   }

    const TArray<FGroupInfo>& MeshGroupInfos = SkeletalMesh->GetMeshGroupInfo();
    auto DetermineMaterialAndShader = [&](uint32 SectionIndex) -> TPair<UMaterialInterface*, UShader*>
    {
       UMaterialInterface* Material = GetMaterial(SectionIndex);
       UShader* Shader = nullptr;

       if (Material && Material->GetShader())
       {
          Shader = Material->GetShader();
       }
       else
       {
          // UE_LOG("USkinnedMeshComponent: 머티리얼이 없거나 셰이더가 없어서 기본 머티리얼 사용 section %u.", SectionIndex);
          Material = UResourceManager::GetInstance().GetDefaultMaterial();
          if (Material)
          {
             Shader = Material->GetShader();
          }
          if (!Material || !Shader)
          {
             UE_LOG("USkinnedMeshComponent: 기본 머티리얼이 없습니다.");
             return { nullptr, nullptr };
          }
       }
       return { Material, Shader };
    };

    const bool bHasSections = !MeshGroupInfos.IsEmpty();
    const uint32 NumSectionsToProcess = bHasSections ? static_cast<uint32>(MeshGroupInfos.size()) : 1;

    for (uint32 SectionIndex = 0; SectionIndex < NumSectionsToProcess; ++SectionIndex)
    {
       uint32 IndexCount = 0;
       uint32 StartIndex = 0;

       if (bHasSections)
       {
          const FGroupInfo& Group = MeshGroupInfos[SectionIndex];
          IndexCount = Group.IndexCount;
          StartIndex = Group.StartIndex;
       }
       else
       {
          IndexCount = SkeletalMesh->GetIndexCount();
          StartIndex = 0;
       }

       if (IndexCount == 0)
       {
          continue;
       }

       auto [MaterialToUse, ShaderToUse] = DetermineMaterialAndShader(SectionIndex);
       if (!MaterialToUse || !ShaderToUse)
       {
          continue;
       }

       FMeshBatchElement BatchElement;
       TArray<FShaderMacro> ShaderMacros = View->ViewShaderMacros;
       if (0 < MaterialToUse->GetShaderMacros().Num())
       {
          ShaderMacros.Append(MaterialToUse->GetShaderMacros());
       }

       // GPU 스키닝 매크로 추가 (전역 설정 적용)
       if (bUseGPU)
       {
          FShaderMacro GPUSkinningMacro;
          GPUSkinningMacro.Name = FName("GPU_SKINNING");
          GPUSkinningMacro.Definition = FName("1");
          ShaderMacros.Add(GPUSkinningMacro);
       }

       FShaderVariant* ShaderVariant = ShaderToUse->GetOrCompileShaderVariant(ShaderMacros);

       if (ShaderVariant)
       {
          BatchElement.VertexShader = ShaderVariant->VertexShader;
          BatchElement.PixelShader = ShaderVariant->PixelShader;
          BatchElement.InputLayout = ShaderVariant->InputLayout;
       }

       BatchElement.Material = MaterialToUse;

       // GPU/CPU 모드에 따라 적절한 버텍스 버퍼 및 스트라이드 설정 (전역 설정 적용)
       if (bUseGPU)
       {
          BatchElement.VertexBuffer = GPUSkinnedVertexBuffer;
          BatchElement.VertexStride = sizeof(FSkinnedVertex);
       }
       else
       {
          BatchElement.VertexBuffer = VertexBuffer;
          BatchElement.VertexStride = SkeletalMesh->GetVertexStride();
       }

       BatchElement.IndexBuffer = SkeletalMesh->GetIndexBuffer();

       BatchElement.IndexCount = IndexCount;
       BatchElement.StartIndex = StartIndex;
       BatchElement.BaseVertexIndex = 0;
       BatchElement.WorldMatrix = GetWorldMatrix();
       BatchElement.ObjectID = InternalIndex;
       BatchElement.PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

       // GPU 스키닝 모드일 때 본 버퍼 설정 (전역 설정 적용)
       if (bUseGPU && BoneMatricesBuffer)
       {
          // AddRef to keep buffer alive even if component is destroyed
          BoneMatricesBuffer->AddRef();
          BatchElement.BoneMatricesBuffer = BoneMatricesBuffer;
       }
       else
       {
          BatchElement.BoneMatricesBuffer = nullptr;
       }

       OutMeshBatchElements.Add(BatchElement);
    }
}

FAABB USkinnedMeshComponent::GetWorldAABB() const
{
   return {};
   // const FTransform WorldTransform = GetWorldTransform();
   // const FMatrix WorldMatrix = GetWorldMatrix();
   //
   // if (!SkeletalMesh)
   // {
   //    const FVector Origin = WorldTransform.TransformPosition(FVector());
   //    return FAABB(Origin, Origin);
   // }
   //
   // const FAABB LocalBound = SkeletalMesh->GetLocalBound(); // <-- 이 함수 구현 필요
   // const FVector LocalMin = LocalBound.Min;
   // const FVector LocalMax = LocalBound.Max;
   //
   // // ... (이하 AABB 계산 로직은 UStaticMeshComponent와 동일) ...
   // const FVector LocalCorners[8] = {
   //    FVector(LocalMin.X, LocalMin.Y, LocalMin.Z),
   //    FVector(LocalMax.X, LocalMin.Y, LocalMin.Z),
   //    // ... (나머지 6개 코너) ...
   //    FVector(LocalMax.X, LocalMax.Y, LocalMax.Z)
   // };
   //
   // FVector4 WorldMin4 = FVector4(LocalCorners[0].X, LocalCorners[0].Y, LocalCorners[0].Z, 1.0f) * WorldMatrix;
   // FVector4 WorldMax4 = WorldMin4;
   //
   // for (int32 CornerIndex = 1; CornerIndex < 8; ++CornerIndex)
   // {
   //    const FVector4 WorldPos = FVector4(LocalCorners[CornerIndex].X
   //       , LocalCorners[CornerIndex].Y
   //       , LocalCorners[CornerIndex].Z
   //       , 1.0f)
   //       * WorldMatrix;
   //    WorldMin4 = WorldMin4.ComponentMin(WorldPos);
   //    WorldMax4 = WorldMax4.ComponentMax(WorldPos);
   // }
   //
   // FVector WorldMin = FVector(WorldMin4.X, WorldMin4.Y, WorldMin4.Z);
   // FVector WorldMax = FVector(WorldMax4.X, WorldMax4.Y, WorldMax4.Z);
   // return FAABB(WorldMin, WorldMax);
}

void USkinnedMeshComponent::OnTransformUpdated()
{
   Super::OnTransformUpdated();
   MarkWorldPartitionDirty();
}

void USkinnedMeshComponent::SetSkeletalMesh(const FString& PathFileName)
{
   ClearDynamicMaterials();

   SkeletalMesh = UResourceManager::GetInstance().Load<USkeletalMesh>(PathFileName);

   if (VertexBuffer)
   {
      VertexBuffer->Release();
      VertexBuffer = nullptr;
   }
   if (GPUSkinnedVertexBuffer)
   {
      GPUSkinnedVertexBuffer->Release();
      GPUSkinnedVertexBuffer = nullptr;
   }

   if (SkeletalMesh && SkeletalMesh->GetSkeletalMeshData())
   {
      // 버퍼는 PerformSkinning()에서 필요할 때 생성됨
      const TArray<FMatrix> IdentityMatrices(SkeletalMesh->GetBoneCount(), FMatrix::Identity());
      UpdateSkinningMatrices(IdentityMatrices);
      // 첫 스키닝은 기본값 GPU로 수행 (나중에 전역 모드에 따라 변경됨)
      PerformSkinning(true);
      
      const TArray<FGroupInfo>& GroupInfos = SkeletalMesh->GetMeshGroupInfo();
       MaterialSlots.resize(GroupInfos.size());
       for (int i = 0; i < GroupInfos.size(); ++i)
      {
         // FGroupInfo에 InitialMaterialName이 있다고 가정
         SetMaterialByName(i, GroupInfos[i].InitialMaterialName);
      }
      MarkWorldPartitionDirty();
   }
   else
   {
      SkeletalMesh = nullptr;
      UpdateSkinningMatrices(TArray<FMatrix>());
   }
}

void USkinnedMeshComponent::PerformSkinning(bool bUseGPU)
{
   if (!SkeletalMesh || FinalSkinningMatrices.IsEmpty()) { return; }

   FSkinningStatManager& StatManager = FSkinningStatManager::GetInstance();
   const TArray<FSkinnedVertex>& SrcVertices = SkeletalMesh->GetSkeletalMeshData()->Vertices;
   const int32 NumVertices = SrcVertices.Num();
   const int32 NumBones = FinalSkinningMatrices.Num();

   if (bUseGPU)
   {
      // === GPU 스키닝 ===

      // 전역 모드로 GPU 강제될 때 버퍼가 없으면 생성
      if (!GPUSkinnedVertexBuffer)
      {
         SkeletalMesh->CreateGPUSkinnedVertexBuffer(&GPUSkinnedVertexBuffer);
      }

      // 본 버퍼 업로드 시간 측정
      uint64 BoneUploadStart = FWindowsPlatformTime::Cycles64();
      UpdateBoneMatrixBuffer();
      uint64 BoneUploadEnd = FWindowsPlatformTime::Cycles64();
      double BoneUploadTimeMS = FWindowsPlatformTime::ToMilliseconds(BoneUploadEnd - BoneUploadStart);

      // NOTE: GPU 셰이더 실행 시간은 현재 구조상 측정 불가
      // (Begin/End 사이에 실제 Draw call이 없음, 별도의 렌더링 파이프라인 통합 필요)

      // 통계에 추가 (실제 할당된 버퍼 크기 사용)
      const uint64 BoneBufferSize = CurrentBoneBufferSize;
      StatManager.AddGPUMesh(NumVertices, NumBones, BoneBufferSize);

      // TimeProfile 시스템에 GPU 스키닝 시간 추가
      FScopeCycleCounter::AddTimeProfile(TStatId("GPU_BoneCalc"), LastBoneMatrixCalcTimeMS);
      FScopeCycleCounter::AddTimeProfile(TStatId("GPU_BoneUpload"), BoneUploadTimeMS);

      StatManager.AddGPUBoneMatrixCalcTime(LastBoneMatrixCalcTimeMS); // 본 행렬 계산 시간 추가
      StatManager.AddGPUBoneBufferUploadTime(BoneUploadTimeMS);

      bSkinningMatricesDirty = false;
   }
   else
   {
      // === CPU 스키닝 ===

      // 전역 모드로 CPU 강제될 때 버퍼가 없으면 생성
      if (!VertexBuffer)
      {
         SkeletalMesh->CreateVertexBuffer(&VertexBuffer);
      }

      SkinnedVertices.SetNum(NumVertices);

      // CPU 버텍스 스키닝 계산 시간 측정
      uint64 VertexSkinningStart = FWindowsPlatformTime::Cycles64();

      for (int32 Idx = 0; Idx < NumVertices; ++Idx)
      {
         const FSkinnedVertex& SrcVert = SrcVertices[Idx];
         FNormalVertex& DstVert = SkinnedVertices[Idx];

         DstVert.pos = SkinVertexPosition(SrcVert);
         DstVert.normal = SkinVertexNormal(SrcVert);
         DstVert.Tangent = SkinVertexTangent(SrcVert);
         DstVert.tex = SrcVert.UV;
      }

      uint64 VertexSkinningEnd = FWindowsPlatformTime::Cycles64();
      double VertexSkinningTimeMS = FWindowsPlatformTime::ToMilliseconds(VertexSkinningEnd - VertexSkinningStart);

      // 버텍스 버퍼 업로드 시간 측정
      uint64 BufferUploadStart = FWindowsPlatformTime::Cycles64();

      if (VertexBuffer)
      {
         SkeletalMesh->UpdateVertexBuffer(SkinnedVertices, VertexBuffer);
      }

      uint64 BufferUploadEnd = FWindowsPlatformTime::Cycles64();
      double BufferUploadTimeMS = FWindowsPlatformTime::ToMilliseconds(BufferUploadEnd - BufferUploadStart);

      // 통계에 추가
      const uint64 VertexBufferSize = sizeof(FNormalVertex) * NumVertices;
      StatManager.AddCPUMesh(NumVertices, NumBones, VertexBufferSize);

      // TimeProfile 시스템에 CPU 스키닝 시간 추가
      FScopeCycleCounter::AddTimeProfile(TStatId("CPU_BoneCalc"), LastBoneMatrixCalcTimeMS);
      FScopeCycleCounter::AddTimeProfile(TStatId("CPU_VertexSkinning"), VertexSkinningTimeMS);
      FScopeCycleCounter::AddTimeProfile(TStatId("CPU_BufferUpload"), BufferUploadTimeMS);

      StatManager.AddCPUBoneMatrixCalcTime(LastBoneMatrixCalcTimeMS); // 본 행렬 계산 시간 추가
      StatManager.AddCPUVertexSkinningTime(VertexSkinningTimeMS);
      StatManager.AddCPUBufferUploadTime(BufferUploadTimeMS);

      bSkinningMatricesDirty = false;
   }
}

void USkinnedMeshComponent::UpdateSkinningMatrices(const TArray<FMatrix>& InSkinningMatrices, double BoneMatrixCalcTimeMS)
{
   FinalSkinningMatrices = InSkinningMatrices;
   LastBoneMatrixCalcTimeMS = BoneMatrixCalcTimeMS;
   bSkinningMatricesDirty = true;
}

FVector USkinnedMeshComponent::SkinVertexPosition(const FSkinnedVertex& InVertex) const
{
   FVector BlendedPosition(0.f, 0.f, 0.f);

   for (int32 Idx = 0; Idx < 4; ++Idx)
   {
      const uint32 BoneIndex = InVertex.BoneIndices[Idx];
      const float Weight = InVertex.BoneWeights[Idx];

      if (Weight > 0.f)
      {
         const FMatrix& SkinMatrix = FinalSkinningMatrices[BoneIndex];
         FVector TransformedPosition = SkinMatrix.TransformPosition(InVertex.Position);
         BlendedPosition += TransformedPosition * Weight;
      }
   }

   return BlendedPosition;
}

FVector USkinnedMeshComponent::SkinVertexNormal(const FSkinnedVertex& InVertex) const
{
   FVector BlendedNormal(0.f, 0.f, 0.f);

   for (int32 Idx = 0; Idx < 4; ++Idx)
   {
      const uint32 BoneIndex = InVertex.BoneIndices[Idx];
      const float Weight = InVertex.BoneWeights[Idx];

      if (Weight > 0.f)
      {
         // 노멀도 일반 스키닝 행렬로 변환 (비균등 스케일 무시)
         const FMatrix& SkinMatrix = FinalSkinningMatrices[BoneIndex];
         FVector TransformedNormal = SkinMatrix.TransformVector(InVertex.Normal);
         BlendedNormal += TransformedNormal * Weight;
      }
   }

   return BlendedNormal.GetSafeNormal();
}

FVector4 USkinnedMeshComponent::SkinVertexTangent(const FSkinnedVertex& InVertex) const
{
   const FVector OriginalTangentDir(InVertex.Tangent.X, InVertex.Tangent.Y, InVertex.Tangent.Z);
   const float OriginalSignW = InVertex.Tangent.W;

   FVector BlendedTangentDir(0.f, 0.f, 0.f);

   for (int32 Idx = 0; Idx < 4; ++Idx)
   {
      const uint32 BoneIndex = InVertex.BoneIndices[Idx];
      const float Weight = InVertex.BoneWeights[Idx];

      if (Weight > 0.f)
      {
         // 탄젠트도 일반 스키닝 행렬로 변환 (비균등 스케일 무시)
         const FMatrix& SkinMatrix = FinalSkinningMatrices[BoneIndex];
         FVector TransformedTangentDir = SkinMatrix.TransformVector(OriginalTangentDir);
         BlendedTangentDir += TransformedTangentDir * Weight;
      }
   }

   const FVector FinalTangentDir = BlendedTangentDir.GetSafeNormal();
   return { FinalTangentDir.X, FinalTangentDir.Y, FinalTangentDir.Z, OriginalSignW };
}

void USkinnedMeshComponent::UpdateBoneMatrixBuffer()
{
   // 실제 본 개수 계산
   const int32 NumBones = FinalSkinningMatrices.Num();
   if (NumBones == 0) return;

   // 버퍼 크기 계산 (실제 본 개수만큼)
   const int32 RawBufferSize = NumBones * sizeof(FMatrix);

   // D3D11 상수 버퍼는 16바이트 정렬 필수
   const int32 AlignedBufferSize = ((RawBufferSize + 15) / 16) * 16;

   // 동적 배열로 본 행렬 데이터 준비
   TArray<FMatrix> BufferData;
   BufferData.SetNum(NumBones);
   for (int32 i = 0; i < NumBones; ++i)
   {
      BufferData[i] = FinalSkinningMatrices[i];
   }

   ID3D11Device* Device = GEngine.GetRHIDevice()->GetDevice();
   ID3D11DeviceContext* Context = GEngine.GetRHIDevice()->GetDeviceContext();

   // 버퍼 크기가 변경되었거나 버퍼가 없으면 재생성
   if (!BoneMatricesBuffer || CurrentBoneBufferSize != AlignedBufferSize)
   {
      // 기존 버퍼 해제
      if (BoneMatricesBuffer)
      {
         BoneMatricesBuffer->Release();
         BoneMatricesBuffer = nullptr;
      }

      // 새 버퍼 생성
      D3D11_BUFFER_DESC BufferDesc;
      ZeroMemory(&BufferDesc, sizeof(BufferDesc));
      BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
      BufferDesc.ByteWidth = AlignedBufferSize;
      BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
      BufferDesc.MiscFlags = 0;
      BufferDesc.StructureByteStride = 0;

      D3D11_SUBRESOURCE_DATA InitData;
      ZeroMemory(&InitData, sizeof(InitData));
      InitData.pSysMem = BufferData.GetData();

      HRESULT hr = Device->CreateBuffer(&BufferDesc, &InitData, &BoneMatricesBuffer);
      if (FAILED(hr))
      {
         UE_LOG("Failed to create bone matrices buffer (Size: %d bytes, NumBones: %d)", AlignedBufferSize, NumBones);
         CurrentBoneBufferSize = 0;
         return;
      }

      CurrentBoneBufferSize = AlignedBufferSize;
   }
   else
   {
      // 버퍼 크기가 동일하면 데이터만 업데이트
      D3D11_MAPPED_SUBRESOURCE MappedResource;
      HRESULT hr = Context->Map(BoneMatricesBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
      if (SUCCEEDED(hr))
      {
         // 실제 데이터 크기만큼만 복사
         memcpy(MappedResource.pData, BufferData.GetData(), RawBufferSize);
         Context->Unmap(BoneMatricesBuffer, 0);
      }
      else
      {
         UE_LOG("Failed to map bone matrices buffer");
      }
   }
}
