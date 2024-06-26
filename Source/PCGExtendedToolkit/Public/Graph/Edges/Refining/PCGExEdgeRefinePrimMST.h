﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "PCGExEdgeRefinePrimMST.generated.h"

class UPCGExHeuristicLocalDistance;
class UPCGExHeuristicDistance;

namespace PCGExCluster
{
	struct FScoredNode;
	struct FNode;
}

/**
 * 
 */
UCLASS(BlueprintType, DisplayName = "MST (Prim)")
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeRefinePrimMST : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual void PrepareForPointIO(PCGExData::FPointIO* InPointIO) override;
	virtual void PreProcess(PCGExCluster::FCluster* InCluster, PCGExGraph::FGraph* InGraph, PCGExData::FPointIO* InEdgesIO) override;
	virtual void Process(PCGExCluster::FCluster* InCluster, PCGExGraph::FGraph* InGraph, PCGExData::FPointIO* InEdgesIO) override;

	virtual void Cleanup() override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExHeuristicModifiersSettings HeuristicsModifiers;

protected:
	TObjectPtr<UPCGExHeuristicLocalDistance> HeuristicsOperation = nullptr;
};
