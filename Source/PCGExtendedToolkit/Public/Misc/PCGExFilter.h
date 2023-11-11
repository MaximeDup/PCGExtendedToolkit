﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"

class PCGEXTENDEDTOOLKIT_API FPCGExFilter
{
public:
	template <typename T, typename dummy = void>
	static int64 Filter(const T& InValue, const PCGExPartition::FRule& Settings)
	{
		const double Upscaled = static_cast<double>(InValue) * Settings.Upscale;
		const double Filtered = (Upscaled - FGenericPlatformMath::Fmod(Upscaled, Settings.FilterSize)) / Settings.FilterSize;
		return static_cast<int64>(Filtered);
	}

	template <typename dummy = void>
	static int64 Filter(const FVector2D& InValue, const PCGExPartition::FRule& Settings)
	{
		int64 Result = 0;
		switch (Settings.Component)
		{
		case EPCGExComponentSelection::X:
			Result = Filter(InValue.X, Settings);
			break;
		case EPCGExComponentSelection::Y:
		case EPCGExComponentSelection::Z:
		case EPCGExComponentSelection::W:
			Result = Filter(InValue.Y, Settings);
			break;
		case EPCGExComponentSelection::XYZ:
		case EPCGExComponentSelection::XZY:
		case EPCGExComponentSelection::ZXY:
		case EPCGExComponentSelection::YXZ:
		case EPCGExComponentSelection::YZX:
		case EPCGExComponentSelection::ZYX:
		case EPCGExComponentSelection::Length:
			Result = Filter(InValue.SquaredLength(), Settings);
			break;
		default: ;
		}
		return Result;
	}

	template <typename dummy = void>
	static int64 Filter(const FVector& InValue, const PCGExPartition::FRule& Settings)
	{
		int64 Result = 0;
		switch (Settings.Component)
		{
		case EPCGExComponentSelection::X:
			Result = Filter(InValue.X, Settings);
			break;
		case EPCGExComponentSelection::Y:
			Result = Filter(InValue.Y, Settings);
			break;
		case EPCGExComponentSelection::Z:
		case EPCGExComponentSelection::W:
			Result = Filter(InValue.Z, Settings);
			break;
		case EPCGExComponentSelection::XYZ:
		case EPCGExComponentSelection::XZY:
		case EPCGExComponentSelection::YXZ:
		case EPCGExComponentSelection::YZX:
		case EPCGExComponentSelection::ZXY:
		case EPCGExComponentSelection::ZYX:
		case EPCGExComponentSelection::Length:
			Result = Filter(InValue.SquaredLength(), Settings);
			break;
		default: ;
		}
		return Result;
	}

	template <typename dummy = void>
	static int64 Filter(const FVector4& InValue, const PCGExPartition::FRule& Settings)
	{
		if (Settings.Component == EPCGExComponentSelection::W)
		{
			return Filter(InValue.W, Settings);
		}
		return Filter(FVector{InValue}, Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FRotator& InValue, const PCGExPartition::FRule& Settings)
	{
		return Filter(FVector{InValue.Euler()}, Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FQuat& InValue, const PCGExPartition::FRule& Settings)
	{
		return Filter(FVector{InValue.Euler()}, Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FTransform& InValue, const PCGExPartition::FRule& Settings)
	{
		return Filter(InValue.GetLocation(), Settings);
	}

	template <typename dummy = void>
	static int64 Filter(const FString& InValue, const PCGExPartition::FRule& Settings)
	{
		return GetTypeHash(InValue);
	}

	template <typename dummy = void>
	static int64 Filter(const FName& InValue, const PCGExPartition::FRule& Settings)
	{
		return Filter(InValue.ToString(), Settings);
	}
};
