#pragma once

#include "Vector.h"

struct Plane
{
    // unit vector
    FVector Normal = { 0.f, 1.f, 0.f };

    // distance from origin to the nearest point in the plane
    float Distance = 0.f;
};

struct Frustum
{
    Plane TopFace;
    Plane BottomFace;
    Plane RightFace;
	Plane LeftFace;
    Plane NearFace;
	Plane FarFace;
};

