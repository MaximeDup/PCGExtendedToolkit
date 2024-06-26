// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Blending/PCGExDataBlending.h"
#include "UObject/Object.h"

#include "PCGExData.h"
#include "Data/PCGExData.h"

#include "PCGExDataFilter.generated.h"

namespace PCGExDataFilter
{
	class TFilterHandler;
}

UENUM(BlueprintType)
enum class EPCGExOperandType : uint8
{
	Attribute UMETA(DisplayName = "Attribute", ToolTip="Use a local attribute value."),
	Constant UMETA(DisplayName = "Constant", ToolTip="Use a constant, static value."),
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExFilterDefinitionBase : public UPCGExParamDataBase
{
	GENERATED_BODY()

public:
	int32 Priority = 0;

	virtual void BeginDestroy() override;
	virtual PCGExDataFilter::TFilterHandler* CreateHandler() const;
};

namespace PCGExDataFilter
{
	constexpr PCGExMT::AsyncState State_FilteringPoints = __COUNTER__;

	const FName OutputFilterLabel = TEXT("Filter");
	const FName SourceFiltersLabel = TEXT("Filters");
	const FName OutputInsideFiltersLabel = TEXT("Inside");
	const FName OutputOutsideFiltersLabel = TEXT("Outside");

	class PCGEXTENDEDTOOLKIT_API TFilterHandler
	{
	public:
		explicit TFilterHandler(const UPCGExFilterDefinitionBase* InDefinition):
			Definition(InDefinition)
		{
		}

		const UPCGExFilterDefinitionBase* Definition;
		TArray<bool> Results;

		int32 Index = 0;
		bool bValid = true;

		virtual void Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO);
		virtual bool Test(const int32 PointIndex) const;
		virtual void PrepareForTesting(PCGExData::FPointIO* PointIO);

		#if !PLATFORM_WINDOWS
		virtual bool IsClusterFilter() const { return false; }
		#endif

		virtual ~TFilterHandler()
		{
			Results.Empty();
		}
	};

	class PCGEXTENDEDTOOLKIT_API TFilterManager
	{
	public:
		explicit TFilterManager(PCGExData::FPointIO* InPointIO);

		TArray<TFilterHandler*> Handlers;
		bool bValid = false;

		PCGExData::FPointIO* PointIO = nullptr;

		template <typename T_DEF>
		void Register(const FPCGContext* InContext, const TArray<TObjectPtr<T_DEF>>& InDefinitions, PCGExData::FPointIO* InPointIO)
		{
			Register(InContext, InDefinitions, [&](TFilterHandler* Handler) { Handler->Capture(InContext, InPointIO); });
		}

		template <typename T_DEF, class CaptureFunc>
		void Register(const FPCGContext* InContext, const TArray<TObjectPtr<T_DEF>>& InDefinitions, CaptureFunc&& InCaptureFn)
		{
			for (T_DEF* Def : InDefinitions)
			{
				TFilterHandler* Handler = Def->CreateHandler();
				InCaptureFn(Handler);

				if (!Handler->bValid)
				{
					delete Handler;
					continue;
				}

				Handlers.Add(Handler);
			}

			bValid = !Handlers.IsEmpty();

			if (!bValid) { return; }

			// Sort mappings so higher priorities come last, as they have to potential to override values.
			Handlers.Sort([&](const TFilterHandler& A, const TFilterHandler& B) { return A.Definition->Priority < B.Definition->Priority; });

			// Update index & partials
			for (int i = 0; i < Handlers.Num(); i++)
			{
				Handlers[i]->Index = i;
				PostProcessHandler(Handlers[i]);
			}
		}

		virtual void PrepareForTesting();

		virtual void Test(const int32 PointIndex);

		virtual ~TFilterManager()
		{
			PCGEX_DELETE_TARRAY(Handlers)
		}

	protected:
		virtual void PostProcessHandler(TFilterHandler* Handler);
	};

	class PCGEXTENDEDTOOLKIT_API TDirectFilterManager : public TFilterManager
	{
	public:
		explicit TDirectFilterManager(PCGExData::FPointIO* InPointIO);

		TArray<bool> Results;

		virtual void Test(const int32 PointIndex) override;
		virtual void PrepareForTesting() override;
	};

	template <typename T_DEF>
	static bool GetInputFilters(FPCGContext* InContext, const FName InLabel, TArray<TObjectPtr<T_DEF>>& OutFilters)
	{
		const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(InLabel);

		TSet<FName> UniqueStatesNames;
		for (const FPCGTaggedData& InputState : Inputs)
		{
			if (const T_DEF* State = Cast<T_DEF>(InputState.Data))
			{
				OutFilters.AddUnique(const_cast<T_DEF*>(State));
			}
		}

		UniqueStatesNames.Empty();

		if (OutFilters.IsEmpty())
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing valid filters."));
			return false;
		}

		return true;
	}
}
