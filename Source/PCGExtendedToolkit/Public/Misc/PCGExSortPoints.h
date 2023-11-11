﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExCompare.h"
#include "PCGExPointsProcessor.h"
#include "PCGSettings.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExSortPoints.generated.h"

UENUM(BlueprintType)
enum class EPCGExSortDirection : uint8
{
	Ascending UMETA(DisplayName = "Ascending"),
	Descending UMETA(DisplayName = "Descending")
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSortRule : public FPCGExInputSelectorWithComponent
{
	GENERATED_BODY()

	FPCGExSortRule(): FPCGExInputSelectorWithComponent()
	{
	}

	template <typename T>
	FPCGExSortRule(const FPCGExSortRule& Other): FPCGExInputSelectorWithComponent(Other)
	{
		Tolerance = Other.Tolerance;
	}

public:
	/** Equality tolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float Tolerance = 0.0001f;
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExSortPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PCGExSortPoints")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExSortPoints", "NodeTitle", "Sort Points"); }
	virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

public:
	/** Controls the order in which points will be ordered. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

	/** Ordered list of attribute to check to sort over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TArray<FPCGExSortRule> Rules = {FPCGExSortRule{}};

private:
	friend class FPCGExSortPointsElement;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSortPointsElement : public FPCGExPointsProcessorElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	static bool BuildRulesForPoints(
		const UPCGPointData* InData,
		const TArray<FPCGExSortRule>& DesiredRules,
		TArray<FPCGExSortRule>& OutRules);

	template <typename T>
	static int Compare(const T& A, const T& B, const FPCGExSortRule& Settings)
	{
		return FPCGExCompare::Compare(A, B, Settings.Tolerance, Settings.ComponentSelection);
	}
};
