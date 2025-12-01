#pragma once

#include "PhysXSupport.h"

/** 물리 재질 파일을 스캔하고 미리 로드하는 로더 클래스 */
class FPhysicalMaterialLoader
{
public:
    /**
     * 지정된 디렉토리 내의 모든 물리 재질 파일을 로드하여 리소스 매니저에 등록한다.
     * @param RelativePath : 데이터 디렉토리 내 상대 경로 (기본값: "PhysicalMaterials")
     */
    static void Preload(const FString& RelativePath = "PhysicalMaterials");
};