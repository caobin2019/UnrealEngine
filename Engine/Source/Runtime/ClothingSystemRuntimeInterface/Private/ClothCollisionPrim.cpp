// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClothCollisionPrim.h"

void FClothCollisionPrim_Convex::RebuildSurfacePoints()
{
	const int32 NumPlanes = Faces.Num();
	if (NumPlanes >= 3)
	{
		SurfacePoints.Reset(NumPlanes * (NumPlanes - 1) * (NumPlanes - 2) / 6);  // Maximum number of intersections

		auto PointInHull = [this](const FVector& Point) -> bool
		{
			for (const FClothCollisionPrim_ConvexFace& Face : Faces)
			{
				if (Face.Plane.PlaneDot(Point) > KINDA_SMALL_NUMBER)
				{
					return false;
				}
			}
			return true;
		};

		for (int32 Index0 = 0; Index0 < NumPlanes; ++Index0)
		{
			for (int32 Index1 = Index0 + 1; Index1 < NumPlanes; ++Index1)
			{
				for (int32 Index2 = Index1 + 1; Index2 < NumPlanes; ++Index2)
				{
					FVector Intersection;
					if (FMath::IntersectPlanes3(Intersection, Faces[Index0].Plane, Faces[Index1].Plane, Faces[Index2].Plane) &&
						PointInHull(Intersection))
					{
						SurfacePoints.Add(Intersection);
					}
				}
			}
		}
	}
	else
	{
		SurfacePoints.Reset();
	}
}
