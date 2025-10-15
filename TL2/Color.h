#pragma once
#include "Vector.h"

struct FLinearColor
{
    float R, G, B, A;

    FLinearColor() : R(0), G(0), B(0), A(0) {}
    explicit FLinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {};
    explicit FLinearColor(const FVector& RGB) : R(RGB.X), G(RGB.Y), B(RGB.Z), A(1.0f) {};
    explicit FLinearColor(const FVector4& RGBA) : R(RGBA.X), G(RGBA.Y), B(RGBA.Z), A(RGBA.W) {};;

	inline FLinearColor& operator=(const FVector4& Other);

    inline FLinearColor operator+(const FLinearColor& Other) const;
    inline FLinearColor operator+=(const FLinearColor& Other);
    inline FLinearColor operator-(const FLinearColor& Other) const;
    inline FLinearColor operator-=(const FLinearColor& Other);

    inline FLinearColor operator*(const FLinearColor& Other) const; // Component-wise Multiplication
    inline FLinearColor operator*(float Scalar) const;              // Scaling
    inline FLinearColor& operator*=(const FLinearColor& Other);
    inline FLinearColor& operator*=(float Scalar);

    inline FLinearColor operator/(const FLinearColor& Other) const; // Component-wise Division
    inline FLinearColor operator/(float Scalar) const;
    inline FLinearColor& operator/=(const FLinearColor& Other);
    inline FLinearColor& operator/=(float Scalar);

	bool Equals(const FLinearColor& ColorB, float Tolerance) const; // Error tolerance Comparison
    FLinearColor GetClamped(float InMin, float InMax) const;

    float GetMax() const;
	float GetMin() const;

	FVector4 ToFVector4() const { return FVector4(R, G, B, A); }

    static float Dist(const FLinearColor& C1, const FLinearColor& C2);
    static float DistSquared(const FLinearColor& C1, const FLinearColor& C2);
    static FLinearColor Lerp(const FLinearColor& C1, const FLinearColor& C2, float T);
    static FLinearColor MakeRandomColor();
    static FLinearColor MakeRandomSeededColor(int32 Seed);
};