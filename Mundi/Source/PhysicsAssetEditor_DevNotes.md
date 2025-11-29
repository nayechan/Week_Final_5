# Physics Asset Editor 개발 노트

## 핵심 개발 지침

### 반드시 지켜야 할 사항
1. **기존 패턴 확인**: 모든 구현 전 기존 코드 패턴 확인 필수
2. **UE 구조 참고**: C:/unrealengine 참고하되 과도한 복잡성 배제
3. **Context Compact 대비**: 중요 내용 md로 캐싱
4. **PhysX 연동 금지**: 에디터 UI만 구현, 실제 물리 연동은 다른 팀
5. **더미 클래스 활용**: 연동 불가 시 더미로 구현, 불가능하면 패스
6. **정적 테스트 수행**: 코드 분석으로 로직 오류/OOP 위반/중복 로직 검출
7. **확장성 설계**: OOP 원칙 준수, 확장에 열린 설계
8. **커스텀 엔진 인지**: UE와 유사해도 다른 엔진임을 인지

---

## 참조할 기존 패턴 파일

### 에디터 윈도우
- `Slate/Windows/SParticleEditorWindow.h` - 가장 복잡한 에디터
- `Slate/Windows/SViewerWindow.h` - 기본 뷰어 클래스
- `Slate/Windows/SAnimationViewerWindow.h` - 애니메이션 뷰어

### Bootstrap
- `Runtime/Engine/Viewer/ParticleEditorBootstrap.h`
- `Runtime/Engine/Viewer/AnimationViewerBootstrap.h`

### ViewerState
- `Runtime/Engine/Viewer/ViewerState.h`

### Shape 컴포넌트
- `Runtime/Engine/Components/CapsuleComponent.h`
- `Runtime/Engine/Components/BoxComponent.h`
- `Runtime/Engine/Components/SphereComponent.h`

### 기타
- `Editor/Gizmo/GizmoActor.h`
- `Runtime/Core/Misc/VertexData.h` (FSkeleton, FBone)
- `Slate/SlateManager.cpp`

---

## Phase 1 구현 체크리스트

- [x] EViewerType::PhysicsAsset 추가 (Enums.h)
- [x] PhysicsTypes.h/cpp 생성 (FBodySetup, FConstraintSetup)
- [x] PhysicsAsset.h/cpp 생성
- [x] PhysicsAssetEditorState 추가 (ViewerState.h)
- [x] PhysicsAssetEditorBootstrap.h/cpp 생성
- [x] SPhysicsAssetEditorWindow.h/cpp 생성
- [x] SlateManager 통합

---

## 생성된 파일 목록

### 새로 생성된 파일 (Phase 1)
- `Runtime/Engine/Physics/PhysicsTypes.h` - EPhysicsShapeType, EConstraintType, IPhysicsAssetPreview
- `Runtime/Engine/Physics/PhysicsAsset.h` - UPhysicsAsset 클래스
- `Runtime/Engine/Physics/PhysicsAsset.cpp` - UPhysicsAsset 구현 (UPROPERTY 자동 직렬화)
- `Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.h` - ViewerState 생성/소멸 선언
- `Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.cpp` - Bootstrap 구현 (World/Viewport 생성, 파일 I/O)
- `Slate/Windows/SPhysicsAssetEditorWindow.h` - 에디터 윈도우 선언
- `Slate/Windows/SPhysicsAssetEditorWindow.cpp` - UI 구현 (Sub Widget 사용)

### 새로 생성된 파일 (Phase 1.5 리팩토링)
- `Runtime/Engine/Physics/FBodySetup.h` - USTRUCT로 분리 (UPROPERTY 자동 직렬화)
- `Runtime/Engine/Physics/FConstraintSetup.h` - USTRUCT로 분리 (UPROPERTY 자동 직렬화)
- `Runtime/Engine/Viewer/PhysicsAssetEditorState.h` - 에디터 상태 별도 헤더
- `Slate/Widgets/PhysicsAssetEditor/SkeletonTreeWidget.h/cpp` - 본/바디 트리 UI
- `Slate/Widgets/PhysicsAssetEditor/BodyPropertiesWidget.h/cpp` - 바디 속성 편집 UI
- `Slate/Widgets/PhysicsAssetEditor/ConstraintPropertiesWidget.h/cpp` - 제약 조건 속성 편집 UI

### 수정된 파일
- `Runtime/Core/Misc/Enums.h` - EViewerType::PhysicsAsset 추가
- `Runtime/Engine/Viewer/ViewerState.h` - PhysicsAssetEditorState.h include
- `Slate/SlateManager.h` - SPhysicsAssetEditorWindow include 추가
- `Slate/SlateManager.cpp` - OpenAssetViewer에 PhysicsAsset 케이스 추가

---

## 데이터 구조 요약

### FBodySetup
- BoneName, BoneIndex
- ShapeType (Capsule/Sphere/Box)
- LocalTransform
- Shape 파라미터 (Radius, HalfHeight, Extent)
- 물리 속성 (Mass, Damping, Gravity)

### FConstraintSetup
- JointName
- ParentBodyIndex, ChildBodyIndex
- ConstraintType (BallAndSocket/Hinge)
- Swing1Limit, Swing2Limit, TwistLimitMin/Max
- Stiffness, Damping

### UPhysicsAsset
- SkeletalMeshPath
- SkeletalBodySetups[]
- ConstraintSetup[]
- BodySetupIndexMap (캐시)

---

## 구현 진행 상황

### Phase 1 완료
- 상태: **완료**
- 구현 내용:
  - 기본 데이터 구조 (PhysicsTypes)
  - 에셋 클래스 (UPhysicsAsset)
  - 에디터 상태 (PhysicsAssetEditorState)
  - 부트스트랩 (PhysicsAssetEditorBootstrap)
  - 에디터 윈도우 (SPhysicsAssetEditorWindow)
  - SlateManager 통합

### Phase 1.5 리팩토링 완료
- 상태: **완료**
- 코드 리뷰 반영:
  1. **USTRUCT 분리**: FBodySetup, FConstraintSetup을 별도 헤더로 분리
     - UPROPERTY로 자동 직렬화 (수동 Serialize 코드 제거)
     - TArray<USTRUCT> 자동 직렬화 지원 확인
  2. **PhysicsAssetEditorState 분리**: ViewerState.h에서 별도 헤더로 분리
  3. **UI Sub Widget 분리**: SRP 원칙 준수
     - SkeletonTreeWidget: 본/바디 계층 트리
     - BodyPropertiesWidget: 바디 Shape/물리 속성 편집
     - ConstraintPropertiesWidget: 제약 조건 속성 편집
  4. **PhysicsTypes.h 정리**: enum과 interface만 유지

### 다음 단계 (Phase 2)
- Body 편집 기능 강화
  - 기즈모 연동 (크기/위치 조절)
  - 본 자동 Shape 크기 계산
  - Shape 타입 변환 시 파라미터 보존

### Phase 3 이후
- Constraint 편집 고도화
- Auto-generate 기능
- 시뮬레이션 UI (PhysX 팀 연동 후)
- Show Flag / Debug 렌더링

---

## 향후 개선 사항

### PropertyRenderer 활용 검토
- 현재: Sub Widget에서 ImGui로 수동 속성 렌더링
- 개선: `UPropertyRenderer::RenderStructProperty()` 활용 가능
  - USTRUCT의 모든 UPROPERTY를 자동 렌더링
  - `UStruct::FindStruct(TypeName)->GetAllProperties()` 순회
- 고려사항:
  - Shape 속성은 타입별 커스텀 UI 필요 (Sphere: Radius만, Box: Extent만)
  - Physics Properties는 PropertyRenderer로 자동화 가능
  - 변경 시 `bShapePreviewDirty` 설정 등 콜백 처리 필요
- 참고: `PropertyRenderer.cpp:804-841` (RenderStructProperty)
