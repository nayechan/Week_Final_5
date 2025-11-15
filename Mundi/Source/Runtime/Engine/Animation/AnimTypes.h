#pragma once
#include "Vector.h"
#include "Archive.h"
#include "UEContainer.h"

/**
 * 단일 본의 키프레임을 포함하는 Raw 애니메이션 시퀀스 트랙
 * Position, Rotation, Scale 키프레임을 각각 저장
 */
struct FRawAnimSequenceTrack
{
public:
	/** 위치 키프레임 (Translation) */
	TArray<FVector> PositionKeys;

	/** 회전 키프레임 (Quaternion) */
	TArray<FQuat> RotationKeys;

	/** 스케일 키프레임 */
	TArray<FVector> ScaleKeys;

	/** 기본 생성자 */
	FRawAnimSequenceTrack() {}

	/** 위치 키프레임 개수 반환 */
	int32 GetNumPosKeys() const { return PositionKeys.Num(); }

	/** 회전 키프레임 개수 반환 */
	int32 GetNumRotKeys() const { return RotationKeys.Num(); }

	/** 스케일 키프레임 개수 반환 */
	int32 GetNumScaleKeys() const { return ScaleKeys.Num(); }

	/** 트랙이 비어있는지 확인 */
	bool IsEmpty() const
	{
		return PositionKeys.Num() == 0 && RotationKeys.Num() == 0 && ScaleKeys.Num() == 0;
	}

	/** 아카이브로 직렬화 */
	friend FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrack& Track)
	{
		// 위치 키 직렬화
		int32 NumPositionKeys = Track.PositionKeys.Num();
		Ar << NumPositionKeys;
		if (Ar.IsLoading())
		{
			Track.PositionKeys.SetNum(NumPositionKeys);
		}
		for (int32 i = 0; i < NumPositionKeys; ++i)
		{
			Ar << Track.PositionKeys[i];
		}

		// 회전 키 직렬화
		int32 NumRotationKeys = Track.RotationKeys.Num();
		Ar << NumRotationKeys;
		if (Ar.IsLoading())
		{
			Track.RotationKeys.SetNum(NumRotationKeys);
		}
		for (int32 i = 0; i < NumRotationKeys; ++i)
		{
			Ar << Track.RotationKeys[i];
		}

		// 스케일 키 직렬화
		int32 NumScaleKeys = Track.ScaleKeys.Num();
		Ar << NumScaleKeys;
		if (Ar.IsLoading())
		{
			Track.ScaleKeys.SetNum(NumScaleKeys);
		}
		for (int32 i = 0; i < NumScaleKeys; ++i)
		{
			Ar << Track.ScaleKeys[i];
		}

		return Ar;
	}
};

/**
 * 특정 본에 대한 애니메이션 트랙
 * 본 인덱스와 해당 본의 Raw 키프레임 데이터를 포함
 */
struct FBoneAnimationTrack
{
public:
	/** 스켈레톤의 본 배열에서의 인덱스 */
	int32 BoneIndex;

	/** 참조 및 디버깅을 위한 본 이름 */
	FString BoneName;

	/** 이 본의 Raw 키프레임 데이터 */
	FRawAnimSequenceTrack InternalTrack;

	/** 기본 생성자 */
	FBoneAnimationTrack()
		: BoneIndex(-1)
	{
	}

	/** 본 인덱스를 받는 생성자 */
	explicit FBoneAnimationTrack(int32 InBoneIndex)
		: BoneIndex(InBoneIndex)
	{
	}

	/** 본 인덱스와 이름을 받는 생성자 */
	FBoneAnimationTrack(int32 InBoneIndex, const FString& InBoneName)
		: BoneIndex(InBoneIndex)
		, BoneName(InBoneName)
	{
	}

	/** 트랙이 유효한지 확인 */
	bool IsValid() const
	{
		return BoneIndex >= 0 && !InternalTrack.IsEmpty();
	}

	/** 아카이브로 직렬화 */
	friend FArchive& operator<<(FArchive& Ar, FBoneAnimationTrack& Track)
	{
		Ar << Track.BoneIndex;

		// FString 직렬화 (내부 포인터가 아닌 실제 문자열 데이터 직렬화)
		if (Ar.IsSaving())
		{
			Serialization::WriteString(Ar, Track.BoneName);
		}
		else if (Ar.IsLoading())
		{
			Serialization::ReadString(Ar, Track.BoneName);
		}

		Ar << Track.InternalTrack;
		return Ar;
	}
};

/**
 * 벡터 애니메이션 커브 (Position/Scale용)
 * 시간별 벡터 값을 저장
 */
struct FVectorAnimCurve
{
public:
	/** 키프레임 시간 배열 (초 단위) */
	TArray<float> Times;

	/** 키프레임 값 배열 (각 Time에 대응) */
	TArray<FVector> Values;

	/** 기본 생성자 */
	FVectorAnimCurve() {}

	/** 키프레임 개수 반환 */
	int32 GetNumKeys() const { return Times.Num(); }

	/** 커브가 비어있는지 확인 */
	bool IsEmpty() const { return Times.Num() == 0; }

	/** 키프레임 추가 */
	void AddKey(float Time, const FVector& Value)
	{
		Times.Add(Time);
		Values.Add(Value);
	}

	/** 아카이브로 직렬화 */
	friend FArchive& operator<<(FArchive& Ar, FVectorAnimCurve& Curve)
	{
		int32 NumKeys = Curve.Times.Num();
		Ar << NumKeys;
		if (Ar.IsLoading())
		{
			Curve.Times.SetNum(NumKeys);
			Curve.Values.SetNum(NumKeys);
		}
		for (int32 i = 0; i < NumKeys; ++i)
		{
			Ar << Curve.Times[i];
			Ar << Curve.Values[i];
		}
		return Ar;
	}
};

/**
 * 쿼터니언 애니메이션 커브 (Rotation용)
 * 시간별 회전 값을 저장
 */
struct FQuatAnimCurve
{
public:
	/** 키프레임 시간 배열 (초 단위) */
	TArray<float> Times;

	/** 키프레임 값 배열 (각 Time에 대응) */
	TArray<FQuat> Values;

	/** 기본 생성자 */
	FQuatAnimCurve() {}

	/** 키프레임 개수 반환 */
	int32 GetNumKeys() const { return Times.Num(); }

	/** 커브가 비어있는지 확인 */
	bool IsEmpty() const { return Times.Num() == 0; }

	/** 키프레임 추가 */
	void AddKey(float Time, const FQuat& Value)
	{
		Times.Add(Time);
		Values.Add(Value);
	}

	/** 아카이브로 직렬화 */
	friend FArchive& operator<<(FArchive& Ar, FQuatAnimCurve& Curve)
	{
		int32 NumKeys = Curve.Times.Num();
		Ar << NumKeys;
		if (Ar.IsLoading())
		{
			Curve.Times.SetNum(NumKeys);
			Curve.Values.SetNum(NumKeys);
		}
		for (int32 i = 0; i < NumKeys; ++i)
		{
			Ar << Curve.Times[i];
			Ar << Curve.Values[i];
		}
		return Ar;
	}
};

/**
 * Transform 애니메이션 커브
 * 본 하나의 Position, Rotation, Scale 커브를 저장
 */
struct FTransformAnimCurve
{
public:
	/** 위치 커브 */
	FVectorAnimCurve PositionCurve;

	/** 회전 커브 */
	FQuatAnimCurve RotationCurve;

	/** 스케일 커브 */
	FVectorAnimCurve ScaleCurve;

	/** 기본 생성자 */
	FTransformAnimCurve() {}

	/** 커브가 비어있는지 확인 */
	bool IsEmpty() const
	{
		return PositionCurve.IsEmpty() && RotationCurve.IsEmpty() && ScaleCurve.IsEmpty();
	}

	/** 아카이브로 직렬화 */
	friend FArchive& operator<<(FArchive& Ar, FTransformAnimCurve& Curve)
	{
		Ar << Curve.PositionCurve;
		Ar << Curve.RotationCurve;
		Ar << Curve.ScaleCurve;
		return Ar;
	}
};

/**
 * 애니메이션 커브 데이터
 * FBX AnimCurve에서 추출한 실제 키프레임 데이터를 저장
 * 프레임 샘플링이 아닌 실제 키프레임만 저장
 */
struct FAnimationCurveData
{
public:
	/** 각 본의 Transform 커브 배열 (본 인덱스 순서대로) */
	TArray<FTransformAnimCurve> BoneTransformCurves;

	/** 기본 생성자 */
	FAnimationCurveData() {}

	/** 본 개수 반환 */
	int32 GetNumBones() const { return BoneTransformCurves.Num(); }

	/** 데이터가 비어있는지 확인 */
	bool IsEmpty() const { return BoneTransformCurves.Num() == 0; }

	/** 초기화 */
	void Reset()
	{
		BoneTransformCurves.clear();
	}

	/** 아카이브로 직렬화 */
	friend FArchive& operator<<(FArchive& Ar, FAnimationCurveData& Data)
	{
		int32 NumBones = Data.BoneTransformCurves.Num();
		Ar << NumBones;
		if (Ar.IsLoading())
		{
			Data.BoneTransformCurves.SetNum(NumBones);
		}
		for (int32 i = 0; i < NumBones; ++i)
		{
			Ar << Data.BoneTransformCurves[i];
		}
		return Ar;
	}
};
