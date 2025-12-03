#include "pch.h"
#include "VehicleComponent.h"
#include "PxPhysicsAPI.h"
#include "VehicleData.h"
#include "PhysicalMaterial.h"
#include "PhysXConversion.h"
#include "PhysicsSystem.h"
#include "PhysicsScene.h"
#include "SnippetVehicleSceneQuery.h"
#include "VehicleActor.h"

namespace
{
	enum
	{
		DRIVABLE_SURFACE = 0xffff0000,
		UNDRIVABLE_SURFACE = 0x0000ffff
	};

	enum
	{
		TIRE_TYPE_NORMAL = 0,
		TIRE_TYPE_WORN,
		MAX_NUM_TIRE_TYPES
	};

	enum
	{
		COLLISION_FLAG_GROUND = 1 << 0,
		COLLISION_FLAG_WHEEL = 1 << 1,
		COLLISION_FLAG_CHASSIS = 1 << 2,
		COLLISION_FLAG_OBSTACLE = 1 << 3,
		COLLISION_FLAG_DRIVABLE_OBSTACLE = 1 << 4,

		COLLISION_FLAG_GROUND_AGAINST = COLLISION_FLAG_CHASSIS | COLLISION_FLAG_OBSTACLE | COLLISION_FLAG_DRIVABLE_OBSTACLE,
		COLLISION_FLAG_WHEEL_AGAINST = COLLISION_FLAG_WHEEL | COLLISION_FLAG_CHASSIS | COLLISION_FLAG_OBSTACLE,
		COLLISION_FLAG_CHASSIS_AGAINST = COLLISION_FLAG_GROUND | COLLISION_FLAG_WHEEL | COLLISION_FLAG_CHASSIS | COLLISION_FLAG_OBSTACLE | COLLISION_FLAG_DRIVABLE_OBSTACLE,
		COLLISION_FLAG_OBSTACLE_AGAINST = COLLISION_FLAG_GROUND | COLLISION_FLAG_WHEEL | COLLISION_FLAG_CHASSIS | COLLISION_FLAG_OBSTACLE | COLLISION_FLAG_DRIVABLE_OBSTACLE,
		COLLISION_FLAG_DRIVABLE_OBSTACLE_AGAINST = COLLISION_FLAG_GROUND | COLLISION_FLAG_CHASSIS | COLLISION_FLAG_OBSTACLE | COLLISION_FLAG_DRIVABLE_OBSTACLE
	};

	enum
	{
		SURFACE_TYPE_TARMAC,
		MAX_NUM_SURFACE_TYPES
	};

	static PxF32 gTireFrictionMultipliers[MAX_NUM_SURFACE_TYPES][MAX_NUM_TIRE_TYPES] =
	{
		//NORMAL,	WORN
		{1.00f,		0.1f}//TARMAC
	};

	PxConvexMesh* CreateConvexMesh(const PxVec3* Verts, const PxU32 NumVerts, PxPhysics& Physics, PxCooking& Cooking)
	{
		// Create descriptor for convex mesh
		PxConvexMeshDesc convexDesc;
		convexDesc.points.count = NumVerts;
		convexDesc.points.stride = sizeof(PxVec3);
		convexDesc.points.data = Verts;
		convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

		PxConvexMesh* convexMesh = NULL;
		PxDefaultMemoryOutputStream buf;
		if (Cooking.cookConvexMesh(convexDesc, buf))
		{
			PxDefaultMemoryInputData id(buf.getData(), buf.getSize());
			convexMesh = Physics.createConvexMesh(id);
		}

		return convexMesh;
	}

	PxConvexMesh* CreateWheelMesh(const PxF32 Width, const PxF32 Radius, PxPhysics& Physics, PxCooking& Cooking)
	{
		PxVec3 points[2 * 16];
		for (PxU32 i = 0; i < 16; i++)
		{
			const PxF32 cosTheta = PxCos(i * PxPi * 2.0f / 16.0f);
			const PxF32 sinTheta = PxSin(i * PxPi * 2.0f / 16.0f);
			const PxF32 y = Radius * cosTheta;
			const PxF32 z = Radius * sinTheta;
			points[2 * i + 0] = PxVec3(-Width / 2.0f, y, z);
			points[2 * i + 1] = PxVec3(+Width / 2.0f, y, z);
		}

		return CreateConvexMesh(points, 32, Physics, Cooking);
	}

	PxConvexMesh* CreateChassisMesh(const PxVec3 Dims, PxPhysics& Physics, PxCooking& Cooking)
	{
		const PxF32 x = Dims.x * 0.5f;
		const PxF32 y = Dims.y * 0.5f;
		const PxF32 z = Dims.z * 0.5f;
		PxVec3 verts[8] =
		{
			PxVec3(x,y,-z),
			PxVec3(x,y,z),
			PxVec3(x,-y,z),
			PxVec3(x,-y,-z),
			PxVec3(-x,y,-z),
			PxVec3(-x,y,z),
			PxVec3(-x,-y,z),
			PxVec3(-x,-y,-z)
		};

		return CreateConvexMesh(verts, 8, Physics, Cooking);
	}

	PxRigidDynamic* CreateVehicleActor
	(const PxVehicleChassisData& ChassisData,
		PxMaterial** WheelMaterials, PxConvexMesh** WheelConvexMeshes, const PxU32 NumWheels, const PxFilterData& WheelSimFilterData,
		PxMaterial** ChassisMaterials, PxConvexMesh** ChassisConvexMeshes, const PxU32 NumChassisMeshes, const PxFilterData& ChassisSimFilterData, PxTransform& StartPose,
		PxPhysics& Physics)
	{
		//We need a rigid body actor for the vehicle.
		//Don't forget to add the actor to the scene after setting up the associated vehicle.
		PxRigidDynamic* VehActor = Physics.createRigidDynamic(StartPose);

		//Wheel and Chassis query filter data.
		//Optional: cars don't drive on other cars.
		PxFilterData WheelQryFilterData;
		WheelQryFilterData.word3 = UNDRIVABLE_SURFACE;
		PxFilterData ChassisQryFilterData;
		ChassisQryFilterData.word3 = UNDRIVABLE_SURFACE;

		//Add all the Wheel shapes to the actor.
		for (PxU32 i = 0; i < NumWheels; i++)
		{
			PxConvexMeshGeometry Geom(WheelConvexMeshes[i]);
			PxShape* WheelShape = PxRigidActorExt::createExclusiveShape(*VehActor, Geom, *WheelMaterials[i]);
			WheelShape->setQueryFilterData(WheelQryFilterData);
			WheelShape->setSimulationFilterData(WheelSimFilterData);
			WheelShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
			WheelShape->setLocalPose(PxTransform(PxIdentity));
		}

		//Add the Chassis shapes to the actor.
		for (PxU32 i = 0; i < NumChassisMeshes; i++)
		{
			PxShape* ChassisShape = PxRigidActorExt::createExclusiveShape(*VehActor, PxConvexMeshGeometry(ChassisConvexMeshes[i]), *ChassisMaterials[i]);
			ChassisShape->setQueryFilterData(ChassisQryFilterData);
			ChassisShape->setSimulationFilterData(ChassisSimFilterData);
			ChassisShape->setLocalPose(PxTransform(PxIdentity));
		}

		VehActor->setMass(ChassisData.mMass);
		VehActor->setMassSpaceInertiaTensor(ChassisData.mMOI);
		VehActor->setCMassLocalPose(PxTransform(ChassisData.mCMOffset, PxQuat(PxIdentity)));

		return VehActor;
	}

	void ComputeWheelCenterActorOffsets4W(const PxF32 WheelFrontZ, const PxF32 WheelRearZ, const PxVec3& ChassisDims, const PxF32 WheelWidth, const PxF32 WheelRadius, const PxU32 NumWheels, PxVec3* WheelCentreOffsets)
	{
		//ChassisDims.z is the distance from the rear of the Chassis to the front of the Chassis.
		//The front has z = 0.5*ChassisDims.z and the rear has z = -0.5*ChassisDims.z.
		//Compute a position for the front Wheel and the rear Wheel along the z-axis.
		//Compute the separation between each Wheel along the z-axis.
		const PxF32 NumLeftWheels = NumWheels / 2.0f;
		const PxF32 DeltaZ = (WheelFrontZ - WheelRearZ) / (NumLeftWheels - 1.0f);
		//Set the outside of the left and right Wheels to be flush with the Chassis.
		//Set the top of the Wheel to be just touching the underside of the Chassis.
		//Begin by setting the rear-left/rear-right/front-left,front-right Wheels.
		WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eREAR_LEFT] = PxVec3((-ChassisDims.x + WheelWidth) * 0.5f, -(ChassisDims.y / 2 + WheelRadius), WheelRearZ + 0 * DeltaZ * 0.5f);
		WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eREAR_RIGHT] = PxVec3((+ChassisDims.x - WheelWidth) * 0.5f, -(ChassisDims.y / 2 + WheelRadius), WheelRearZ + 0 * DeltaZ * 0.5f);
		WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eFRONT_LEFT] = PxVec3((-ChassisDims.x + WheelWidth) * 0.5f, -(ChassisDims.y / 2 + WheelRadius), WheelRearZ + (NumLeftWheels - 1) * DeltaZ);
		WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eFRONT_RIGHT] = PxVec3((+ChassisDims.x - WheelWidth) * 0.5f, -(ChassisDims.y / 2 + WheelRadius), WheelRearZ + (NumLeftWheels - 1) * DeltaZ);
		//Set the remaining Wheels.
		for (PxU32 i = 2, WheelCount = 4; i < NumWheels - 2; i += 2, WheelCount += 2)
		{
			WheelCentreOffsets[WheelCount + 0] = PxVec3((-ChassisDims.x + WheelWidth) * 0.5f, -(ChassisDims.y / 2 + WheelRadius), WheelRearZ + i * DeltaZ * 0.5f);
			WheelCentreOffsets[WheelCount + 1] = PxVec3((+ChassisDims.x - WheelWidth) * 0.5f, -(ChassisDims.y / 2 + WheelRadius), WheelRearZ + i * DeltaZ * 0.5f);
		}
	}

	void SetupWheelsSimulationData
	(const PxF32 WheelMass, const PxF32 WheelMOI, const PxF32 WheelRadius, const PxF32 WheelWidth,
		const PxU32 NumWheels, const PxVec3* WheelCenterActorOffsets,
		const PxVec3& ChassisCMOffset, const PxF32 ChassisMass,
		PxVehicleWheelsSimData* WheelsSimData)
	{
		//Set up the Wheels.
		PxVehicleWheelData Wheels[PX_MAX_NB_WHEELS];
		{
			for (PxU32 i = 0; i < NumWheels; i++)
			{
				Wheels[i].mMass = WheelMass;
				Wheels[i].mMOI = WheelMOI;
				Wheels[i].mRadius = WheelRadius;
				Wheels[i].mWidth = WheelWidth;
			}

			//Enable the handbrake for the rear Wheels only.
			Wheels[PxVehicleDrive4WWheelOrder::eREAR_LEFT].mMaxHandBrakeTorque = 4000.0f;
			Wheels[PxVehicleDrive4WWheelOrder::eREAR_RIGHT].mMaxHandBrakeTorque = 4000.0f;
			//Enable steering for the front Wheels only.
			Wheels[PxVehicleDrive4WWheelOrder::eFRONT_LEFT].mMaxSteer = PxPi * 0.3333f;
			Wheels[PxVehicleDrive4WWheelOrder::eFRONT_RIGHT].mMaxSteer = PxPi * 0.3333f;
		}

		//Set up the Tires.
		PxVehicleTireData Tires[PX_MAX_NB_WHEELS];
		{
			//Set up the Tires.
			for (PxU32 i = 0; i < NumWheels; i++)
			{
				Tires[i].mType = TIRE_TYPE_NORMAL;
			}
		}

		//Set up the Suspensions
		PxVehicleSuspensionData Suspensions[PX_MAX_NB_WHEELS];
		{
			//Compute the mass supported by each suspension spring.
			PxF32 SuspSprungMasses[PX_MAX_NB_WHEELS];
			PxVehicleComputeSprungMasses
			(NumWheels, WheelCenterActorOffsets,
				ChassisCMOffset, ChassisMass, 1, SuspSprungMasses);

			//Set the suspension data.
			for (PxU32 i = 0; i < NumWheels; i++)
			{
				Suspensions[i].mMaxCompression = 0.3f;
				Suspensions[i].mMaxDroop = 0.1f;
				Suspensions[i].mSpringStrength = 35000.0f;
				Suspensions[i].mSpringDamperRate = 4500.0f;
				Suspensions[i].mSprungMass = SuspSprungMasses[i];
			}

			//Set the camber angles.
			const PxF32 CamberAngleAtRest = 0.0;
			const PxF32 CamberAngleAtMaxDroop = 0.01f;
			const PxF32 CamberAngleAtMaxCompression = -0.01f;
			for (PxU32 i = 0; i < NumWheels; i += 2)
			{
				Suspensions[i + 0].mCamberAtRest = CamberAngleAtRest;
				Suspensions[i + 1].mCamberAtRest = -CamberAngleAtRest;
				Suspensions[i + 0].mCamberAtMaxDroop = CamberAngleAtMaxDroop;
				Suspensions[i + 1].mCamberAtMaxDroop = -CamberAngleAtMaxDroop;
				Suspensions[i + 0].mCamberAtMaxCompression = CamberAngleAtMaxCompression;
				Suspensions[i + 1].mCamberAtMaxCompression = -CamberAngleAtMaxCompression;
			}
		}

		//Set up the Wheel Geometry.
		PxVec3 SuspTravelDirections[PX_MAX_NB_WHEELS];
		PxVec3 WheelCentreCMOffsets[PX_MAX_NB_WHEELS];
		PxVec3 SuspForceAppCMOffsets[PX_MAX_NB_WHEELS];
		PxVec3 TireForceAppCMOffsets[PX_MAX_NB_WHEELS];
		{
			//Set the Geometry data.
			for (PxU32 i = 0; i < NumWheels; i++)
			{
				//Vertical suspension travel.
				SuspTravelDirections[i] = PxVec3(0, -1, 0);

				//Wheel center offset is offset from rigid body center of mass.
				WheelCentreCMOffsets[i] =
					WheelCenterActorOffsets[i] - ChassisCMOffset;

				//Suspension force application point 0.3 metres below 
				//rigid body center of mass.
				SuspForceAppCMOffsets[i] =
					PxVec3(WheelCentreCMOffsets[i].x, -0.3f, WheelCentreCMOffsets[i].z);

				//Tire force application point 0.3 metres below 
				//rigid body center of mass.
				TireForceAppCMOffsets[i] =
					PxVec3(WheelCentreCMOffsets[i].x, -0.3f, WheelCentreCMOffsets[i].z);
			}
		}

		//Set up the filter data of the raycast that will be issued by each suspension.
		PxFilterData QryFilterData;
		QryFilterData.word3 = UNDRIVABLE_SURFACE;

		//Set the Wheel, tire and suspension data.
		//Set the Geometry data.
		//Set the query filter data
		for (PxU32 i = 0; i < NumWheels; i++)
		{
			WheelsSimData->setWheelData(i, Wheels[i]);
			WheelsSimData->setTireData(i, Tires[i]);
			WheelsSimData->setSuspensionData(i, Suspensions[i]);
			WheelsSimData->setSuspTravelDirection(i, SuspTravelDirections[i]);
			WheelsSimData->setWheelCentreOffset(i, WheelCentreCMOffsets[i]);
			WheelsSimData->setSuspForceAppPointOffset(i, SuspForceAppCMOffsets[i]);
			WheelsSimData->setTireForceAppPointOffset(i, TireForceAppCMOffsets[i]);
			WheelsSimData->setSceneQueryFilterData(i, QryFilterData);
			WheelsSimData->setWheelShapeMapping(i, PxI32(i));
		}

		//Add a front and rear anti-roll bar
		PxVehicleAntiRollBarData BarFront;
		BarFront.mWheel0 = PxVehicleDrive4WWheelOrder::eFRONT_LEFT;
		BarFront.mWheel1 = PxVehicleDrive4WWheelOrder::eFRONT_RIGHT;
		BarFront.mStiffness = 10000.0f;
		WheelsSimData->addAntiRollBarData(BarFront);
		PxVehicleAntiRollBarData BarRear;
		BarRear.mWheel0 = PxVehicleDrive4WWheelOrder::eREAR_LEFT;
		BarRear.mWheel1 = PxVehicleDrive4WWheelOrder::eREAR_RIGHT;
		BarRear.mStiffness = 10000.0f;
		WheelsSimData->addAntiRollBarData(BarRear);
	}

	void ComputeWheelWidthsAndRadii(PxConvexMesh** WheelConvexMeshes, PxF32* WheelWidths, PxF32* WheelRadii)
	{
		for (PxU32 i = 0; i < 4; i++)
		{
			const PxU32 NumWheelVerts = WheelConvexMeshes[i]->getNbVertices();
			const PxVec3* WheelVerts = WheelConvexMeshes[i]->getVertices();
			PxVec3 WheelMin(PX_MAX_F32, PX_MAX_F32, PX_MAX_F32);
			PxVec3 WheelMax(-PX_MAX_F32, -PX_MAX_F32, -PX_MAX_F32);
			for (PxU32 j = 0; j < NumWheelVerts; j++)
			{
				WheelMin.x = PxMin(WheelMin.x, WheelVerts[j].x);
				WheelMin.y = PxMin(WheelMin.y, WheelVerts[j].y);
				WheelMin.z = PxMin(WheelMin.z, WheelVerts[j].z);
				WheelMax.x = PxMax(WheelMax.x, WheelVerts[j].x);
				WheelMax.y = PxMax(WheelMax.y, WheelVerts[j].y);
				WheelMax.z = PxMax(WheelMax.z, WheelVerts[j].z);
			}
			WheelWidths[i] = WheelMax.x - WheelMin.x;
			WheelRadii[i] = PxMax(WheelMax.y, WheelMax.z) * 0.975f;
		}
	}

	PxQueryHitType::Enum WheelSceneQueryPreFilterBlocking
	(PxFilterData filterData0, PxFilterData filterData1,
		const void* constantBlock, PxU32 constantBlockSize,
		PxHitFlags& queryFlags)
	{
		//filterData0 is the vehicle suspension query.
		//filterData1 is the shape potentially hit by the query.
		PX_UNUSED(filterData0);
		PX_UNUSED(constantBlock);
		PX_UNUSED(constantBlockSize);
		PX_UNUSED(queryFlags);
		return ((0xffffffff == (filterData1.word3 | DRIVABLE_SURFACE)) ? PxQueryHitType::eNONE : PxQueryHitType::eBLOCK);
	}

	PxVehicleDrivableSurfaceToTireFrictionPairs* CreateFrictionPairs(const PxMaterial* DefaultMaterial)
	{
		PxVehicleDrivableSurfaceType SurfaceTypes[1];
		SurfaceTypes[0].mType = SURFACE_TYPE_TARMAC;

		const PxMaterial* SurfaceMaterials[1];
		SurfaceMaterials[0] = DefaultMaterial;

		PxVehicleDrivableSurfaceToTireFrictionPairs* SurfaceTirePairs =
			PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(MAX_NUM_TIRE_TYPES, MAX_NUM_SURFACE_TYPES);

		SurfaceTirePairs->setup(MAX_NUM_TIRE_TYPES, MAX_NUM_SURFACE_TYPES, SurfaceMaterials, SurfaceTypes);

		for (PxU32 i = 0; i < MAX_NUM_SURFACE_TYPES; i++)
		{
			for (PxU32 j = 0; j < MAX_NUM_TIRE_TYPES; j++)
			{
				SurfaceTirePairs->setTypePairFriction(i, j, gTireFrictionMultipliers[i][j]);
			}
		}
		return SurfaceTirePairs;
	}
}

UVehicleComponent::UVehicleComponent()
{
	bCanEverTick = true;
}

UVehicleComponent::~UVehicleComponent()
{
	if (GetWorld() && GetWorld()->GetPhysicsScene())
	{
		GetWorld()->GetPhysicsScene()->RemoveVehicle(this);
	}

	if (ChassisMaterial)
	{
		delete ChassisMaterial;
		ChassisMaterial = nullptr;
	}
	if (WheelMaterial)
	{
		delete WheelMaterial;
		WheelMaterial = nullptr;
	}

	// 휠 접지 검사를 위한 씬 쿼리 데이터를 해제하고 포인터를 정리합니다.
	if (VehicleQueryData)
	{
		VehicleQueryData->free(GEngine.GetPhysicsSystem()->GetAllocator());
		VehicleQueryData = nullptr;
	}
	// 타이어 마찰력 데이터를 해제하고 포인터를 정리합니다.
	if (FrictionPairs)
	{
		FrictionPairs->release();
		FrictionPairs = nullptr;
	}
	// PxBatchQuery는 PxScene에 의해 생성되고 PxScene이 해제될 때 함께 해제될 수 있습니다.
	// 따라서 여기서 명시적으로 release()를 호출할 필요는 없지만, 안전한 포인터 관리를 위해 nullptr로 설정합니다.
	if (BatchQuery)
	{
		BatchQuery->release();
		BatchQuery = nullptr;
	}

	// FBodyInstance가 Actor를 이중으로 해제하는 것을 막기 위해 포인터를 제거합니다.
	BodyInstance.RigidActor = nullptr;

	// PhysXVehicle을 해제하여 Actor와 Vehicle 인스턴스를 모두 정리합니다.
	if (PhysXVehicle)
	{
		PhysXVehicle->release();
		PhysXVehicle = nullptr;
	}
}

void UVehicleComponent::EndPlay()
{
	Super::EndPlay();
}

void UVehicleComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

}

void UVehicleComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	PhysXVehicle = nullptr;
	ChassisMaterial = nullptr;
	WheelMaterial = nullptr;

	FrictionPairs = nullptr;
	VehicleQueryData = nullptr;
	BatchQuery = nullptr;
}

void UVehicleComponent::PostPhysicsTick(float DeltaTime)
{
	Super::PostPhysicsTick(DeltaTime);

	if (PhysXVehicle)
	{
		// 1. 입력 상태 수집 (이 부분은 게임의 입력 시스템에 따라 달라집니다.)
		// 예시: 가상의 입력 값
		PxF32 Accel = 0.0f;    // 가속 (0.0 ~ 1.0)
		PxF32 Brake = 0.0f;    // 브레이크 (0.0 ~ 1.0)
		PxF32 LeftSteer = 0.0f;    // 스티어링 (-1.0 ~ 1.0)
		PxF32 RightSteer = 0.0f;    // 스티어링 (-1.0 ~ 1.0)
		PxF32 HandBrake = 0.0f; // 핸드브레이크 (0.0 ~ 1.0)

		if (UInputManager::GetInstance().IsKeyDown('W'))
		{
			PhysXVehicle->mDriveDynData.mUseAutoGears = true;
			PhysXVehicle->mDriveDynData.setCurrentGear(PxVehicleGearsData::eFIRST);
			Accel = 1.0f;
		}

		if (UInputManager::GetInstance().IsKeyDown('D'))
		{
			LeftSteer = 1.0f;
		}

		if (UInputManager::GetInstance().IsKeyDown('A'))
		{
			RightSteer = 1.0f;
		}

		if (UInputManager::GetInstance().IsKeyDown('S'))
		{
			PhysXVehicle->mDriveDynData.setCurrentGear(PxVehicleGearsData::eREVERSE);
			Accel = 1.0f;
			//Brake = 1.0f;
		}

		if (UInputManager::GetInstance().IsKeyDown('F'))
		{
			HandBrake = 1.0f;
		}

		if (UInputManager::GetInstance().IsKeyDown(' '))
		{
			PhysXVehicle->mDriveDynData.setCurrentGear(PxVehicleGearsData::eNEUTRAL);
			Brake = 1.0f;
			HandBrake = 1.0f;
		}

		UpdateDriftEffects();

		// TODO: 실제 사용자 입력을 받아 Accel, Brake, Steer, HandBrake 변수에 할당합니다.
		// 예를 들어, GetWorld()->GetInputManager()->GetAccelInput(); 와 같은 함수를 사용할 수 있습니다.

		//// 기어 명령 (자동/수동에 따라 다름)
		//PxVehicleGearsData::Enum Gear = PhysXVehicle->mDriveDynData.getCurrentGear();
		// TODO: 필요하다면 기어 변경 로직을 추가합니다.
		// 예를 들어, UpArrow 키를 누르면 Gear를 PxVehicleGearsData::eNEXT로 설정.

		// 2. PxVehicleDriveDynData 업데이트
		// 가속 및 브레이크 입력 설정
		PhysXVehicle->mDriveDynData.setAnalogInput(PxVehicleDrive4WControl::eANALOG_INPUT_ACCEL, Accel);
		PhysXVehicle->mDriveDynData.setAnalogInput(PxVehicleDrive4WControl::eANALOG_INPUT_BRAKE, Brake);
		PhysXVehicle->mDriveDynData.setAnalogInput(PxVehicleDrive4WControl::eANALOG_INPUT_STEER_LEFT, LeftSteer);
		PhysXVehicle->mDriveDynData.setAnalogInput(PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT, RightSteer);
		PhysXVehicle->mDriveDynData.setAnalogInput(PxVehicleDrive4WControl::eANALOG_INPUT_HANDBRAKE, HandBrake);

		// 기어 명령 설정
		//PhysXVehicle->mDriveDynData.mUseAutoGears = true;

	}
}

void UVehicleComponent::UpdateDriftEffects()
{
	if (!PhysXVehicle) return;

	// 설정값 (나중에 FVehicleData 등으로 빼는 것을 권장)
	const float DriftThreshold = 0.3f;   // 드리프트(횡) 연기 발생 기준 

	float TotalSlip = 0.0f;
	int RearWheelStart = 2; // 뒷바퀴 기준 (후륜구동/4륜구동 가정)
	if (VehicleData.NumWheels < 4)
		RearWheelStart = 0;

	for (int i = RearWheelStart; i < VehicleData.NumWheels; ++i)
	{
		// 슬립 데이터 가져오기
		float LatSlip = PxAbs(WheelQueryResults[i].lateralSlip);

		// A. 횡 슬립 (Lateral): 드리프트
		// 0.3 rad(약 17도) 이상 미끄러질 때만 유효값으로 인정
		float EffectiveLat = (LatSlip > DriftThreshold) ? LatSlip : 0.0f;

		TotalSlip += EffectiveLat;
	}

	float AvgSlip = TotalSlip / (float)(VehicleData.NumWheels - RearWheelStart);

	// 강도 계산 (0.0 ~ 1.0)
	float SmokeIntensity = PxClamp(AvgSlip, 0.0f, 1.0f);

	if (AVehicleActor* VehicleActor = Cast<AVehicleActor>(Owner))
	{
		VehicleActor->UpdateDriftSmoke(SmokeIntensity);
	}
}

void UVehicleComponent::OnTransformUpdated()
{
	Super::OnTransformUpdated();
	if (PhysXVehicle && PhysXVehicle->getRigidDynamicActor())
	{
		PxTransform PxWorldTransform = PhysXConvert::ToPx(GetWorldTransform());
		PhysXVehicle->getRigidDynamicActor()->setGlobalPose(PxWorldTransform);
	}
}

void UVehicleComponent::SyncByPhysics(const FTransform& NewTransform)
{
	Super::SyncByPhysics(NewTransform);

	if (AVehicleActor* VehicleActor = Cast<AVehicleActor>(Owner))
	{
		for (int WheelIndex = 0; WheelIndex < VehicleData.NumWheels; WheelIndex++)
		{
			PxTransform WheelPxTransform = WheelQueryResults[WheelIndex].localPose;
			FTransform WheelTransform = PhysXConvert::FromPx(WheelPxTransform);
			VehicleActor->UpdateWheelsTransform(WheelIndex, WheelTransform.Translation, WheelTransform.Rotation);

			//float Degree = PhysXVehicle->mWheelsDynData.getWheelRotationAngle(WheelIndex) / TWO_PI * 360.0f;
			//FQuat VehicleWheelQuat = FQuat::MakeFromEulerZYX(FVector(Degree, 0, 0));
			//VehicleActor->UpdateWheelsTransform(WheelIndex, FVector::One(), VehicleWheelQuat);
		}
	}
}

void UVehicleComponent::Simulate(float DeltaTime)
{
	if (PhysXVehicle && BodyInstance.RigidActor && VehicleQueryData && BatchQuery && FrictionPairs)
	{
		//Raycasts.
		PxVehicleWheels* Vehicles[1] = { PhysXVehicle };
		PxRaycastQueryResult* RaycastResults = VehicleQueryData->getRaycastQueryResultBuffer(0);
		const PxU32 RaycastResultsSize = VehicleQueryData->getQueryResultBufferSize();
		PxVehicleSuspensionRaycasts(BatchQuery, 1, Vehicles, RaycastResultsSize, RaycastResults);

		//Vehicle update.
		const PxVec3 Grav = GetWorld()->GetPhysicsScene()->GetPxScene()->getGravity();
		PxVehicleWheelQueryResult VehicleQueryResults[1] = { {WheelQueryResults, PhysXVehicle->mWheelsSimData.getNbWheels()} };
		PxVehicleUpdates(DeltaTime, Grav, *FrictionPairs, 1, Vehicles, VehicleQueryResults);
	}
}

void UVehicleComponent::CreatePhysicsState()
{
	// 현재 컴포넌트의 월드 Transform 가져오기
	FTransform WorldTransform = GetWorldTransform();

	// 차량 생성 (Transform 전달)
	PhysXVehicle = CreateVehicle4W(VehicleData,
		GEngine.GetPhysicsSystem()->GetPhysics(),
		GEngine.GetPhysicsSystem()->GetCooking(),
		WorldTransform);

	// BodyInstance와 PhysX Actor 연결
	PxRigidDynamic* VehActor = PhysXVehicle->getRigidDynamicActor();
	BodyInstance.RigidActor = VehActor; // BodyInstance가 이 Actor를 알게 함

	// BodyInstance의 설정을 역으로 Actor에 반영
	BodyInstance.SetSimulatePhysics(true);

	BodyInstance.OwnerComponent = this;

	BodyInstance.FinalizeInternalActor(GetWorld()->GetPhysicsScene());

	GetWorld()->GetPhysicsScene()->AddVehicle(this);
}

PxVehicleDrive4W* UVehicleComponent::CreateVehicle4W(FVehicleData VehicleData, physx::PxPhysics* Physics, PxCooking* Cooking, const FTransform& WorldTransform)
{
	// 엔진 좌표계 -> PhysX 좌표계 변환
	PxTransform StartPose = PhysXConvert::ToPx(WorldTransform);
	PxScene* PxScene = GetWorld()->GetPhysicsScene()->GetPxScene();

	if (!ChassisMaterial)
	{
		ChassisMaterial = UPhysicalMaterial::CreateDefaultMaterial();
	}
	if (!WheelMaterial)
	{
		WheelMaterial = UPhysicalMaterial::CreateDefaultMaterial();
	}

	if (!FrictionPairs)
	{
		FrictionPairs = CreateFrictionPairs(WheelMaterial->MatHandle);
	}
	if (!VehicleQueryData)
	{
		VehicleQueryData = snippetvehicle::VehicleSceneQueryData::allocate(1, PX_MAX_NB_WHEELS, 1, 1, WheelSceneQueryPreFilterBlocking, NULL, GEngine.GetPhysicsSystem()->GetAllocator());
	}
	if (!BatchQuery)
	{
		BatchQuery = snippetvehicle::VehicleSceneQueryData::setUpBatchedSceneQuery(0, *VehicleQueryData, PxScene);
	}

	PxRigidDynamic* Veh4WActor = NULL;
	{
		//Construct a convex mesh for a cylindrical Wheel
		PxConvexMesh* WheelMesh = CreateWheelMesh(VehicleData.WheelWidth, VehicleData.WheelRadius, *Physics, *Cooking);
		//Assume all Wheels are identical for simplicity.
		PxConvexMesh* WheelConvexMeshes[PX_MAX_NB_WHEELS];
		PxMaterial* WheelMaterials[PX_MAX_NB_WHEELS];
		//Set the meshes and materials for the driven Wheels.
		for (PxU32 i = PxVehicleDrive4WWheelOrder::eFRONT_LEFT; i <= PxVehicleDrive4WWheelOrder::eREAR_RIGHT; i++)
		{
			WheelConvexMeshes[i] = WheelMesh;
			WheelMaterials[i] = WheelMaterial->MatHandle;
		}
		//Set the meshes and materials for the non-driven Wheels
		for (PxU32 i = PxVehicleDrive4WWheelOrder::eREAR_RIGHT + 1; i < VehicleData.NumWheels; i++)
		{
			WheelConvexMeshes[i] = WheelMesh;
			WheelMaterials[i] = WheelMaterial->MatHandle;
		}

		//Chassis just has a single convex shape for simplicity.
		PxConvexMesh* ChassisConvexMesh = CreateChassisMesh(VehicleData.ChassisDims, *Physics, *Cooking);
		PxConvexMesh* ChassisConvexMeshes[1] = { ChassisConvexMesh };
		PxMaterial* ChassisMaterials[1] = { ChassisMaterial->MatHandle };

		//Rigid body data.
		PxVehicleChassisData RigidBodyData;
		RigidBodyData.mMOI = VehicleData.GetChassisMOI();
		RigidBodyData.mMass = VehicleData.ChassisMass;
		RigidBodyData.mCMOffset = VehicleData.CenterOfMassOffset;

		PxFilterData ChassisSimFilterData(COLLISION_FLAG_CHASSIS, COLLISION_FLAG_GROUND, 0, 0);
		PxFilterData WheelSimFilterData(COLLISION_FLAG_WHEEL, COLLISION_FLAG_WHEEL, PxPairFlag::eDETECT_CCD_CONTACT | PxPairFlag::eMODIFY_CONTACTS, 0);

		Veh4WActor = CreateVehicleActor
		(RigidBodyData,
			WheelMaterials, WheelConvexMeshes, VehicleData.NumWheels, WheelSimFilterData,
			ChassisMaterials, ChassisConvexMeshes, 1, ChassisSimFilterData, StartPose,
			*Physics);

		// 휠 메시는 PxShape에 복사되었으므로 해제
		if (WheelMesh)
		{
			WheelMesh->release();
			WheelMesh = nullptr; // 안전을 위해 포인터 정리
		}

		// 섀시 메시는 PxShape에 복사되었으므로 해제
		if (ChassisConvexMesh)
		{
			ChassisConvexMesh->release();
			ChassisConvexMesh = nullptr; // 안전을 위해 포인터 정리
		}
	}

	//Set up the sim data for the Wheels.
	PxVehicleWheelsSimData* WheelsSimData = PxVehicleWheelsSimData::allocate(VehicleData.NumWheels);
	{
		//Compute the Wheel center offsets from the origin.
		PxVec3 WheelCenterActorOffsets[PX_MAX_NB_WHEELS];
		const PxF32 FrontZ = VehicleData.ChassisDims.Z * 0.3f;
		const PxF32 RearZ = -VehicleData.ChassisDims.Z * 0.3f;
		ComputeWheelCenterActorOffsets4W(FrontZ, RearZ, VehicleData.ChassisDims, VehicleData.WheelWidth, VehicleData.WheelRadius, VehicleData.NumWheels, WheelCenterActorOffsets);
		//Set up the simulation data for all Wheels.
		SetupWheelsSimulationData
		(VehicleData.WheelMass, VehicleData.GetWheelMOI(), VehicleData.WheelRadius, VehicleData.WheelWidth,
			VehicleData.NumWheels, WheelCenterActorOffsets,
			VehicleData.CenterOfMassOffset, VehicleData.ChassisMass,
			WheelsSimData);
	}

	//Set up the sim data for the vehicle drive model.
	PxVehicleDriveSimData4W DriveSimData;
	{
		//Diff
		PxVehicleDifferential4WData DiffData;
		DiffData.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD;
		DriveSimData.setDiffData(DiffData);

		//Engine
		PxVehicleEngineData EngineData;
		EngineData.mPeakTorque = VehicleData.EnginePeakTorque;
		EngineData.mMaxOmega = VehicleData.GetEngineMaxOmega();//approx 6000 rpm
		DriveSimData.setEngineData(EngineData);

		//Gears
		PxVehicleGearsData GearsData;
		GearsData.mSwitchTime = VehicleData.GearSwitchTime;
		// FVehicleData의 Gears 배열을 PxVehicleGearsData에 복사
		GearsData.mNbRatios = PxMin<PxU32>((PxU32)VehicleData.Gears.Num(), PxVehicleGearsData::eGEARSRATIO_COUNT);
		for (PxU32 i = 0; i < GearsData.mNbRatios; ++i)
		{
			GearsData.mRatios[i] = VehicleData.Gears[i];
		}
		// 후진 기어는 따로 설정
		if (VehicleData.Gears.Num() > 0)
		{
			GearsData.mRatios[PxVehicleGearsData::eREVERSE] = VehicleData.Gears[0]; // 보통 첫 번째가 후진 기어
		}

		DriveSimData.setGearsData(GearsData);
		//Clutch
		PxVehicleClutchData ClutchData;
		ClutchData.mStrength = VehicleData.ClutchStrength;
		DriveSimData.setClutchData(ClutchData);

		//Ackermann steer accuracy
		PxVehicleAckermannGeometryData AckermannData;
		AckermannData.mAccuracy = 1.0f;
		AckermannData.mAxleSeparation =
			WheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eFRONT_LEFT).z -
			WheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eREAR_LEFT).z;
		AckermannData.mFrontWidth =
			WheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eFRONT_RIGHT).x -
			WheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eFRONT_LEFT).x;
		AckermannData.mRearWidth =
			WheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eREAR_RIGHT).x -
			WheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eREAR_LEFT).x;
		DriveSimData.setAckermannGeometryData(AckermannData);
	}

	//Create a vehicle from the Wheels and drive sim data.
	PxVehicleDrive4W* VehDrive4W = PxVehicleDrive4W::allocate(VehicleData.NumWheels);
	VehDrive4W->setup(Physics, Veh4WActor, *WheelsSimData, DriveSimData, VehicleData.NumWheels - 4);

	//Free the sim data because we don't need that any more.
	WheelsSimData->free();

	return VehDrive4W;
}