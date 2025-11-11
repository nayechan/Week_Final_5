#include "pch.h"
#include "Vector.h"

void FVector::Log()
{
	char buf[160];
	sprintf_s(buf, "Vector(%f, %f, %f)", X, Y, Z);
	UE_LOG(buf);
}

FQuat::FQuat(const FMatrix& M)
{
	// (M은 스케일이 제거된 3x3 회전 행렬이라고 가정)
	const float Trace = M.M[0][0] + M.M[1][1] + M.M[2][2];

	if (Trace > 0.0f)
	{
		// W가 가장 큰 경우 (가장 안정적인 경로)
		// S = 4 * W
		float S = sqrtf(Trace + 1.0f) * 2.0f; 
		float InvS = 1.0f / S;

		this->W = 0.25f * S;
		this->X = (M.M[1][2] - M.M[2][1]) * InvS; // (M_row[j][k] - M_row[k][j])
		this->Y = (M.M[2][0] - M.M[0][2]) * InvS;
		this->Z = (M.M[0][1] - M.M[1][0]) * InvS;
	}
	else
	{
		// Trace <= 0, W가 가장 크지 않음
		// X, Y, Z 중 가장 큰 대각 성분을 찾음
		if (M.M[0][0] > M.M[1][1] && M.M[0][0] > M.M[2][2])
		{
			// X가 가장 큰 경우
			// S = 4 * X
			float S = sqrtf(1.0f + M.M[0][0] - M.M[1][1] - M.M[2][2]) * 2.0f;
			float InvS = 1.0f / S;

			this->W = (M.M[1][2] - M.M[2][1]) * InvS;
			this->X = 0.25f * S;
			this->Y = (M.M[0][1] + M.M[1][0]) * InvS; // (M_row[i][j] + M_row[j][i])
			this->Z = (M.M[0][2] + M.M[2][0]) * InvS;
		}
		else if (M.M[1][1] > M.M[2][2])
		{
			// Y가 가장 큰 경우
			// S = 4 * Y
			float S = sqrtf(1.0f + M.M[1][1] - M.M[0][0] - M.M[2][2]) * 2.0f;
			float InvS = 1.0f / S;

			this->W = (M.M[2][0] - M.M[0][2]) * InvS;
			this->X = (M.M[0][1] + M.M[1][0]) * InvS;
			this->Y = 0.25f * S;
			this->Z = (M.M[1][2] + M.M[2][1]) * InvS;
		}
		else
		{
			// Z가 가장 큰 경우
			// S = 4 * Z
			float S = sqrtf(1.0f + M.M[2][2] - M.M[0][0] - M.M[1][1]) * 2.0f;
			float InvS = 1.0f / S;

			this->W = (M.M[0][1] - M.M[1][0]) * InvS;
			this->X = (M.M[0][2] + M.M[2][0]) * InvS;
			this->Y = (M.M[1][2] + M.M[2][1]) * InvS;
			this->Z = 0.25f * S;
		}
	}
}

const FMatrix FMatrix::ZUpToYUp = FMatrix
(
	0, 0, 1, 0,
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
);

const FMatrix FMatrix::YUpToZUp = FMatrix
(
	0, 1, 0, 0,
	0, 0, 1, 0,
	1, 0, 0, 0,
	0, 0, 0, 1
);


FVector FMatrix::TransformPosition(const FVector& V) const
{
	return {
		V.X * M[0][0] + V.Y * M[1][0] + V.Z * M[2][0] + M[3][0],
		V.X * M[0][1] + V.Y * M[1][1] + V.Z * M[2][1] + M[3][1],
		V.X * M[0][2] + V.Y * M[1][2] + V.Z * M[2][2] + M[3][2]
	};
}

FVector FMatrix::TransformVector(const FVector& V) const
{
	return {
		V.X * M[0][0] + V.Y * M[1][0] + V.Z * M[2][0],
		V.X * M[0][1] + V.Y * M[1][1] + V.Z * M[2][1],
		V.X * M[0][2] + V.Y * M[1][2] + V.Z * M[2][2]
	 };
}

FMatrix FMatrix::OrthoMatrix(const FAABB & AABB)
{
	return OrthoMatrix(AABB.Max.X, AABB.Min.X, AABB.Max.Y, AABB.Min.Y, AABB.Max.Z, AABB.Min.Z);
}

FMatrix FMatrix::OrthoMatrix(float R, float L, float T, float B, float F, float N)
{
	// A, 0, 0, 0
	// 0, B, 0, 0,
	// 0, 0, C, 0,
	// D, E, F, 1
	const float M_A = 2 / (R - L);
	const float M_B = 2 / (T - B);
	const float M_C = 1 / (F - N);
	const float M_D = -(R + L) / (R - L);
	const float M_E = -(T + B) / (T - B);
	const float M_F = -N / (F - N);
	return FMatrix(
		M_A, 0, 0, 0,
		0, M_B, 0, 0,
		0, 0, M_C, 0,
		M_D, M_E, M_F, 1);
}

FMatrix FMatrix::CreateProjectionMatrix(float FieldOfView, float AspectRatio, float ViewWidth, float ViewHeight, float NearClip, float FarClip, float ZoomFactor, ECameraProjectionMode ProjectionMode)
{
	if (ProjectionMode == ECameraProjectionMode::Perspective)
	{
		return FMatrix::PerspectiveFovLH(
			DegreesToRadians(FieldOfView),
			AspectRatio,
			NearClip, FarClip);
	}
	else // Orthographic
	{
		// world unit = 100 pixels (예시)
		const float PixelsPerWorldUnit = 10.0f;

		// 뷰포트 크기를 월드 단위로 변환
		float OrthoWidth = (ViewWidth / PixelsPerWorldUnit) * ZoomFactor;
		float OrthoHeight = (ViewHeight / PixelsPerWorldUnit) * ZoomFactor;

		return FMatrix::OrthoLH(
			OrthoWidth,
			OrthoHeight,
			NearClip, FarClip);
	}
}

FTransform::FTransform(const FMatrix& M)
{
	FVector Scale3D;
	Scale3D.X = FVector(M.M[0][0], M.M[0][1], M.M[0][2]).Size();
	Scale3D.Y = FVector(M.M[1][0], M.M[1][1], M.M[1][2]).Size();
	Scale3D.Z = FVector(M.M[2][0], M.M[2][1], M.M[2][2]).Size();
	this->Scale3D = Scale3D;

	this->Translation = FVector(M.M[3][0], M.M[3][1], M.M[3][2]);

	FMatrix RotationMatrix = M;
	constexpr float Epsilon = 1e-6f;
	if (fabs(this->Scale3D.X) > Epsilon)
	{
		RotationMatrix.M[0][0] /= this->Scale3D.X;
		RotationMatrix.M[0][1] /= this->Scale3D.X;
		RotationMatrix.M[0][2] /= this->Scale3D.X;
	}
	if (fabs(this->Scale3D.Y) > Epsilon)
	{
		RotationMatrix.M[1][0] /= this->Scale3D.Y;
		RotationMatrix.M[1][1] /= this->Scale3D.Y;
		RotationMatrix.M[1][2] /= this->Scale3D.Y;
	}
	if (fabs(this->Scale3D.Z) > Epsilon)
	{
		RotationMatrix.M[2][0] /= this->Scale3D.Z;
		RotationMatrix.M[2][1] /= this->Scale3D.Z;
		RotationMatrix.M[2][2] /= this->Scale3D.Z;
	}
	
	// (행렬 -> 쿼터니언 변환)
	this->Rotation = FQuat(RotationMatrix);
}
