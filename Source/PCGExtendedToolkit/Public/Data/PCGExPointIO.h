﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Data/PCGPointData.h"

#include "PCGEx.h"

#include "PCGExPointIO.generated.h"

namespace PCGExIO
{
	enum class EInitMode : uint8
	{
		NoOutput UMETA(DisplayName = "No Output"),
		NewOutput UMETA(DisplayName = "Create Empty Output Object"),
		DuplicateInput UMETA(DisplayName = "Duplicate Input Object"),
		Forward UMETA(DisplayName = "Forward Input Object")
	};
}

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExPointIO : public UObject
{
	GENERATED_BODY()

public:
	UPCGExPointIO();

	FName DefaultOutputLabel = PCGEx::OutputPointsLabel;

	FPCGTaggedData Source;       // Source struct
	UPCGPointData* In = nullptr; // Input PointData

	FPCGTaggedData Output;        // Source structS
	UPCGPointData* Out = nullptr; // Output PointData

	int32 NumPoints = -1;

	TMap<PCGMetadataEntryKey, int32> IndicesMap; //MetadataEntry::Index, based on Input points (output MetadataEntry will be offset)

	void Flush() { IndicesMap.Empty(); }

protected:
	mutable FRWLock MapLock;

public:
	~UPCGExPointIO()
	{
		IndicesMap.Empty();
		In = nullptr;
		Out = nullptr;
	}

	/**
	 * 
	 * @param InitOut Only initialize output if there is an existing input
	 * @return 
	 */
	void InitializeOut(PCGExIO::EInitMode InitOut = PCGExIO::EInitMode::NoOutput);

	void BuildIndices();
	void BuildMetadataEntries();
	void BuildMetadataEntriesAndIndices();
	void ClearIndices();

	/**
	 * Returns the index of the point with provided metadata key.
	 * Make sure you built indices before calling this method.
	 * @param Key 
	 * @return 
	 */
	int32 GetIndex(PCGMetadataEntryKey Key) const;

	template <class InitializeFunc, class ProcessElementFunc>
	bool OutputParallelProcessing(
		FPCGContext* Context,
		InitializeFunc&& Initialize,
		ProcessElementFunc&& LoopBody,
		const int32 ChunkSize = 32);

	template <class InitializeFunc, class ProcessElementFunc>
	bool InputParallelProcessing(
		FPCGContext* Context,
		InitializeFunc&& Initialize,
		ProcessElementFunc&& LoopBody,
		const int32 ChunkSize = 32);

	/**
	 * Write valid outputs to Context' tagged data
	 * @param Context 
	 * @param bEmplace if false (default), will try to use the source first
	 */
	bool OutputTo(FPCGContext* Context, bool bEmplace = false);
	bool OutputTo(FPCGContext* Context, bool bEmplace, int64 MinPointCount, int64 MaxPointCount);

protected:
	bool bMetadataEntryDirty = true;
	bool bIndicesDirty = true;
};

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExPointIOGroup : public UObject
{
	GENERATED_BODY()

public:
	UPCGExPointIOGroup();
	UPCGExPointIOGroup(FPCGContext* Context, FName InputLabel, PCGExIO::EInitMode InitOut = PCGExIO::EInitMode::NoOutput);
	UPCGExPointIOGroup(FPCGContext* Context, TArray<FPCGTaggedData>& Sources, PCGExIO::EInitMode InitOut = PCGExIO::EInitMode::NoOutput);

	FName DefaultOutputLabel = PCGEx::OutputPointsLabel;
	TArray<UPCGExPointIO*> Pairs;

	/**
	 * Initialize from Sources
	 * @param Context 
	 * @param Sources 
	 * @param InitOut 
	 */
	void Initialize(
		FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
		PCGExIO::EInitMode InitOut = PCGExIO::EInitMode::NoOutput);

	void Initialize(
		FPCGContext* Context, TArray<FPCGTaggedData>& Sources,
		PCGExIO::EInitMode InitOut,
		const TFunction<bool(UPCGPointData*)>& ValidateFunc,
		const TFunction<void(UPCGExPointIO*)>& PostInitFunc);

	UPCGExPointIO* Emplace_GetRef(
		const UPCGExPointIO& PointIO,
		const PCGExIO::EInitMode InitOut = PCGExIO::EInitMode::NoOutput);

	UPCGExPointIO* Emplace_GetRef(
		const FPCGTaggedData& Source, UPCGPointData* In,
		const PCGExIO::EInitMode InitOut = PCGExIO::EInitMode::NoOutput);

	bool IsEmpty() const { return Pairs.IsEmpty(); }

	void OutputTo(FPCGContext* Context, bool bEmplace = false);
	void OutputTo(FPCGContext* Context, bool bEmplace, const int64 MinPointCount, const int64 MaxPointCount);

	void ForEach(const TFunction<void(UPCGExPointIO*, const int32)>& BodyLoop);

	void Flush()
	{
		for (UPCGExPointIO* Pair : Pairs) { Pair->Flush(); }
		Pairs.Empty();
	}

	template <class InitializeFunc, class ProcessElementFunc>
	bool OutputsParallelProcessing(
		FPCGContext* Context,
		InitializeFunc&& Initialize,
		ProcessElementFunc&& LoopBody,
		int32 ChunkSize = 32);

	template <class InitializeFunc, class ProcessElementFunc>
	bool InputsParallelProcessing(
		FPCGContext* Context,
		InitializeFunc&& Initialize,
		ProcessElementFunc&& LoopBody,
		int32 ChunkSize = 32);

protected:
	mutable FRWLock PairsLock;
	TArray<bool> PairProcessingStatuses;
	bool bProcessing = false;

	static UPCGPointData* GetMutablePointData(FPCGContext* Context, const FPCGTaggedData& Source)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);
		if (!SpatialData) { return nullptr; }

		const UPCGPointData* PointData = SpatialData->ToPointData(Context);
		if (!PointData) { return nullptr; }

		return const_cast<UPCGPointData*>(PointData);
	}
};