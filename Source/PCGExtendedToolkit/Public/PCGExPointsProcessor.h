﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPin.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGEx.h"
#include "PCGExMT.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data\PCGExPointIO.h"
#include "PCGExOperation.h"

#include "PCGExPointsProcessor.generated.h"

#define PCGEX_BIND_OPERATION(_NAME, _TYPE) Context->_NAME = Context->RegisterOperation<_TYPE>(Settings->_NAME);

struct FPCGExPointsProcessorContext;

namespace PCGEx
{
	struct PCGEXTENDEDTOOLKIT_API FAPointLoop
	{
		virtual ~FAPointLoop() = default;

		FAPointLoop()
		{
		}

		FPCGExPointsProcessorContext* Context = nullptr;

		PCGExData::FPointIO* PointIO = nullptr;

		int32 NumIterations = -1;
		int32 ChunkSize = 32;

		int32 CurrentIndex = -1;
		bool bAsyncEnabled = true;

		inline PCGExData::FPointIO& GetPointIO() const;

		virtual bool Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) = 0;
		virtual bool Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) = 0;

	protected:
		int32 GetCurrentChunkSize() const
		{
			return FMath::Min(ChunkSize, NumIterations - CurrentIndex);
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FPointLoop : public FAPointLoop
	{
		FPointLoop()
		{
		}

		virtual bool Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
	};

	struct PCGEXTENDEDTOOLKIT_API FBulkPointLoop : public FPointLoop
	{
		FBulkPointLoop()
		{
		}

		TArray<FPointLoop> SubLoops;

		virtual void Init();
		virtual bool Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
	};

	struct PCGEXTENDEDTOOLKIT_API FAsyncPointLoop : public FPointLoop
	{
		FAsyncPointLoop()
		{
		}

		virtual bool Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
	};

	struct PCGEXTENDEDTOOLKIT_API FBulkAsyncPointLoop : public FAsyncPointLoop
	{
		FBulkAsyncPointLoop()
		{
		}

		TArray<FAsyncPointLoop> SubLoops;

		virtual void Init();
		virtual bool Advance(const TFunction<void(PCGExData::FPointIO&)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody) override;
	};
}


/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExPointsProcessorSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend struct FPCGExPointsProcessorContext;
	friend class FPCGExPointsProcessorElementBase;

public:
	UPCGExPointsProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	//PCGEX_NODE_INFOS(PointsProcessorSettings, "Points Processor Settings", "TOOLTIP_TEXT");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	virtual FName GetMainPointsInputLabel() const;
	virtual bool GetMainPointsInputAcceptMultipleData() const;
	virtual FName GetMainPointsOutputLabel() const;
	virtual PCGExData::EInit GetPointOutputInitMode() const;

	/** Forces execution on main thread. Work is still chunked. Turning this off ensure linear order of operations, and, in most case, determinism.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance")
	bool bDoAsyncProcessing = true;

	/** Chunk size for parallel processing.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance")
	int32 ChunkSize = -1;

	template <typename T>
	static T* EnsureOperation(UPCGExOperation* Operation) { return Operation ? static_cast<T*>(Operation) : NewObject<T>(); }

	template <typename T>
	static T* EnsureOperation(UPCGExOperation* Operation, FPCGExPointsProcessorContext* InContext)
	{
		T* RetValue = Operation ? static_cast<T*>(Operation) : NewObject<T>();
		RetValue->BindContext(InContext);
		return RetValue;
	}

protected:
	virtual int32 GetPreferredChunkSize() const;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorContext : public FPCGContext
{
	friend class FPCGExPointsProcessorElementBase;

	virtual ~FPCGExPointsProcessorContext() override;

public:
	mutable FRWLock ContextLock;
	PCGExData::FPointIOGroup* MainPoints = nullptr;

	int32 GetCurrentPointsIndex() const { return CurrentPointsIndex; };
	PCGExData::FPointIO* CurrentIO = nullptr;

	const UPCGPointData* GetCurrentIn() const { return CurrentIO->GetIn(); }
	UPCGPointData* GetCurrentOut() const { return CurrentIO->GetOut(); }

	bool AdvancePointsIO();
	UWorld* World = nullptr;

	PCGExMT::AsyncState GetState() const { return CurrentState; }
	bool IsState(const PCGExMT::AsyncState OperationId) const { return CurrentState == OperationId; }
	bool IsSetup() const { return IsState(PCGExMT::State_Setup); }
	bool IsDone() const { return IsState(PCGExMT::State_Done); }
	void Done();

	FPCGExAsyncManager* GetAsyncManager();
	void SetAsyncState(PCGExMT::AsyncState WaitState) { SetState(WaitState, false); }

	virtual void SetState(PCGExMT::AsyncState OperationId, bool bResetAsyncWork = true);
	virtual void Reset();

	virtual bool ValidatePointDataInput(UPCGPointData* PointData);
	virtual void PostInitPointDataInput(const PCGExData::FPointIO& PointData);

	int32 ChunkSize = 0;
	bool bDoAsyncProcessing = true;

	void OutputPoints() { MainPoints->OutputTo(this); }

	bool BulkProcessMainPoints(TFunction<void(PCGExData::FPointIO&)>&& Initialize, TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody);
	bool ProcessCurrentPoints(TFunction<void(PCGExData::FPointIO&)>&& Initialize, TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody, bool bForceSync = false);
	bool ProcessCurrentPoints(TFunction<void(const int32, const PCGExData::FPointIO&)>&& LoopBody, bool bForceSync = false);

	template <class InitializeFunc, class LoopBodyFunc>
	bool Process(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody, const int32 NumIterations, const bool bForceSync = false)
	{
		AsyncLoop.NumIterations = NumIterations;
		AsyncLoop.bAsyncEnabled = bDoAsyncProcessing && !bForceSync;
		return AsyncLoop.Advance(Initialize, LoopBody);
	}

	template <class LoopBodyFunc>
	bool Process(LoopBodyFunc&& LoopBody, const int32 NumIterations, const bool bForceSync = false)
	{
		AsyncLoop.NumIterations = NumIterations;
		AsyncLoop.bAsyncEnabled = bDoAsyncProcessing && !bForceSync;
		return AsyncLoop.Advance(LoopBody);
	}

	void Output(FPCGTaggedData& OutTaggedData, UPCGData* OutData, const FName OutputLabel);
	void Output(UPCGData* OutData, const FName OutputLabel);
	void Output(const PCGExData::FPointIO& PointIO);

	template <typename T>
	T MakeLoop()
	{
		T Loop = T{};
		Loop.Context = this;
		Loop.ChunkSize = ChunkSize;
		Loop.bAsyncEnabled = bDoAsyncProcessing;
		return Loop;
	}
	
	template <typename T>
	T* RegisterOperation(UPCGExOperation* Operation)
	{
		T* RetValue = nullptr;
		
		if (!Operation)
		{
			RetValue = NewObject<T>();
			OwnedProcessorOperations.Add(RetValue);
		}
		else
		{
			RetValue = static_cast<T*>(Operation);
		}
		RetValue->BindContext(this);
		return RetValue;
	}

protected:
	PCGExMT::FAsyncParallelLoop AsyncLoop;
	FPCGExAsyncManager* AsyncManager = nullptr;

	PCGEx::FPointLoop ChunkedPointLoop;
	PCGEx::FAsyncPointLoop AsyncPointLoop;
	PCGEx::FBulkAsyncPointLoop BulkAsyncPointLoop;

	PCGExMT::AsyncState CurrentState;
	int32 CurrentPointsIndex = -1;

	TArray<UPCGExOperation*> ProcessorOperations;
	TSet<UPCGExOperation*> OwnedProcessorOperations;

	void CleanupOperations();

	virtual void ResetAsyncWork();
	virtual bool IsAsyncWorkComplete();
};

class PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorElementBase : public FPCGPointProcessingElementBase
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool Validate(FPCGContext* InContext) const;
	virtual void InitializeContext(FPCGExPointsProcessorContext* InContext, const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) const;
	//virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
