﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFusePoints.h"
#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "PCGExCommon.h"

#define LOCTEXT_NAMESPACE "PCGExFusePointsElement"

#define PCGEX_FUSE_FOREACH_POINTPROPERTYNAME(MACRO)\
MACRO(Density) \
MACRO(Extents) \
MACRO(Color) \
MACRO(Position) \
MACRO(Rotation)\
MACRO(Scale) \
MACRO(Steepness) \
MACRO(Seed)

#define PCGEX_FUSE_TRANSFERT(_NAME) Context->_NAME##FuseMethod = Settings->bOverride##_NAME ? Context->_NAME##FuseMethod : Settings->FuseMethod;

// TYPE, NAME, ACCESSOR
#define PCGEX_FUSE_FOREACH_POINTPROPERTY(MACRO)\
MACRO(float, Density, Density, 0) \
MACRO(FVector, Extents, GetExtents(), FVector::Zero()) \
MACRO(FVector4, Color, Color, FVector4::Zero()) \
MACRO(FVector, Position, Transform.GetLocation(), FVector::Zero()) \
MACRO(FRotator, Rotation, Transform.Rotator(), FRotator::ZeroRotator)\
MACRO(FVector, Scale, Transform.GetScale3D(), FVector::Zero()) \
MACRO(float, Steepness, Steepness, 0) \
MACRO(int32, Seed, Seed, 0)

#define PCGEX_FUSE_FOREACH_POINTPROPERTY_ASSIGN(MACRO)\
MACRO(Density, Density) \
MACRO(Color, Color) \
MACRO(Transform, Transform) \
MACRO(Steepness, Steepness) \
MACRO(Seed, Seed)

#define PCGEX_FUSE_IGNORE(...) // Ignore
#define PCGEX_FUSE_DECLARE(_TYPE, _NAME, _ACCESSOR, _DEFAULT_VALUE, ...) _TYPE Out##_NAME = Context->_NAME##FuseMethod == EPCGExFuseMethod::Skip ? RootPoint._ACCESSOR : _DEFAULT_VALUE;
#define PCGEX_FUSE_ASSIGN(_NAME, _ACCESSOR, ...) NewPoint._ACCESSOR = Out##_NAME;

#define PCGEX_FUSE_UPDATE(_NAME) if(!bOverride##_NAME){_NAME##FuseMethod = FuseMethod;}

#define PCGEX_FUSE_FUSE(_TYPE, _NAME, _ACCESSOR, ...)\
	switch (Context->_NAME##FuseMethod){\
	case EPCGExFuseMethod::Average: Out##_NAME += Point._ACCESSOR; break;\
	case EPCGExFuseMethod::Min: PCGEx::Maths::CWMin(Out##_NAME, Point._ACCESSOR); break;\
	case EPCGExFuseMethod::Max: PCGEx::Maths::CWMax(Out##_NAME, Point._ACCESSOR); break;\
	case EPCGExFuseMethod::Weight: PCGEx::Maths::Lerp(Out##_NAME, Point._ACCESSOR, Weight); break;\
}

#define PCGEX_FUSE_POST(_TYPE, _NAME, _ACCESSOR, ...)\
if(Context->_NAME##FuseMethod == EPCGExFuseMethod::Average){ PCGEx::Maths::CWDivide(Out##_NAME, AverageDivider); }
//else if(Context->_NAME##FuseMethod == EPCGExFuseMethod::Skip){ Out##_NAME = RootPoint._ACCESSOR; }

PCGEx::EIOInit UPCGExFusePointsSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::NewOutput; }

UPCGExFusePointsSettings::UPCGExFusePointsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RefreshFuseMethodHiddenNames();
}

void UPCGExFusePointsSettings::RefreshFuseMethodHiddenNames()
{
	if (!FuseMethodOverrides.IsEmpty())
	{
		for (FPCGExInputDescriptorWithFuseMethod& Descriptor : FuseMethodOverrides)
		{
			Descriptor.HiddenDisplayName = Descriptor.Selector.GetName().ToString();
		}

		PCGEX_FUSE_FOREACH_POINTPROPERTYNAME(PCGEX_FUSE_UPDATE)
	}
}

void UPCGExFusePointsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	RefreshFuseMethodHiddenNames();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FPCGElementPtr UPCGExFusePointsSettings::CreateElement() const { return MakeShared<FPCGExFusePointsElement>(); }

FPCGContext* FPCGExFusePointsElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExFusePointsContext* Context = new FPCGExFusePointsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExFusePointsSettings* Settings = Context->GetInputSettings<UPCGExFusePointsSettings>();
	check(Settings);

	Context->FuseMethod = Settings->FuseMethod;
	Context->Radius = FMath::Pow(Settings->Radius, 2);
	Context->bComponentWiseRadius = Settings->bComponentWiseRadius;
	Context->Radiuses = Settings->Radiuses;

	PCGEX_FUSE_FOREACH_POINTPROPERTYNAME(PCGEX_FUSE_TRANSFERT)

	return Context;
}

bool FPCGExFusePointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFusePointsElement::Execute);

	FPCGExFusePointsContext* Context = static_cast<FPCGExFusePointsContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetState(PCGExMT::EState::Done);
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

	auto Initialize = [&Context](const UPCGExPointIO* IO)
	{
		Context->FusedPoints.Reset(IO->NumPoints);
	};

	auto ProcessPoint = [&Context](const FPCGPoint& Point, const int32 Index, const UPCGExPointIO* IO)
	{
		const FVector PtPosition = Point.Transform.GetLocation();
		double Distance = 0;
		PCGExFuse::FFusedPoint* FuseTarget = nullptr;

		{
			FReadScopeLock ScopeLock(Context->PointsLock);
			if (Context->bComponentWiseRadius)
			{
				for (PCGExFuse::FFusedPoint& FusedPoint : Context->FusedPoints)
				{
					if (abs(PtPosition.X - FusedPoint.Position.X) <= Context->Radiuses.X &&
						abs(PtPosition.Y - FusedPoint.Position.Y) <= Context->Radiuses.Y &&
						abs(PtPosition.Z - FusedPoint.Position.Z) <= Context->Radiuses.Z)
					{
						Distance = FVector::DistSquared(FusedPoint.Position, PtPosition);
						FuseTarget = &FusedPoint;
						break;
					}
				}
			}
			else
			{
				for (PCGExFuse::FFusedPoint& FusedPoint : Context->FusedPoints)
				{
					Distance = FVector::DistSquared(FusedPoint.Position, PtPosition);
					if (Distance < Context->Radius)
					{
						FuseTarget = &FusedPoint;
						break;
					}
				}
			}
		}

		FWriteScopeLock ScopeLock(Context->PointsLock);
		if (!FuseTarget)
		{
			FuseTarget = &Context->FusedPoints.Emplace_GetRef();
			FuseTarget->MainIndex = Index;
			FuseTarget->Position = PtPosition;
		}

		FuseTarget->Add(Index, Distance);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->CurrentIO->InputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ProcessingGraph2ndPass);
		}
	}


	auto InitializeReconcile = [&Context]()
	{
		Context->OutPoints = &Context->CurrentIO->Out->GetMutablePoints();
	};

	auto FusePoints = [&Context](int32 ReadIndex)
	{
		FPCGPoint NewPoint;
		Context->CurrentIO->Out->Metadata->InitializeOnSet(NewPoint.MetadataEntry);

		{
			FReadScopeLock ScopeLock(Context->PointsLock);
			PCGExFuse::FFusedPoint& FusedPointData = Context->FusedPoints[ReadIndex];
			int32 NumFused = static_cast<double>(FusedPointData.Fused.Num());
			double AverageDivider = static_cast<double>(NumFused);

			FPCGPoint RootPoint = Context->CurrentIO->In->GetPoint((FusedPointData.MainIndex));

			FTransform& OutTransform = NewPoint.Transform;
			PCGEX_FUSE_FOREACH_POINTPROPERTY(PCGEX_FUSE_DECLARE)

			for (int i = 0; i < NumFused; i++)
			{
				const double Weight = 1 - (FusedPointData.Distances[i] / FusedPointData.MaxDistance);
				FPCGPoint Point = Context->CurrentIO->In->GetPoint(FusedPointData.Fused[i]);

				PCGEX_FUSE_FOREACH_POINTPROPERTY(PCGEX_FUSE_FUSE)
			}

			PCGEX_FUSE_FOREACH_POINTPROPERTY(PCGEX_FUSE_POST)

			OutTransform.SetLocation(OutPosition);
			OutTransform.SetRotation(OutRotation.Quaternion());
			OutTransform.SetScale3D(OutScale);

			PCGEX_FUSE_FOREACH_POINTPROPERTY_ASSIGN(PCGEX_FUSE_ASSIGN)
			NewPoint.SetExtents(OutExtents);
		}
		{
			FWriteScopeLock ScopeLock(Context->PointsLock);
			Context->OutPoints->Add(NewPoint);
		}
	};

	if (Context->IsState(PCGExMT::EState::ProcessingGraph2ndPass))
	{
		//TODO: Stabilized alternative -- parallel doesn't guarantee order, so result is non-deterministic
		if (PCGEx::Common::ParallelForLoop(Context, Context->FusedPoints.Num(), InitializeReconcile, FusePoints, Context->ChunkSize))
		{
			Context->OutPoints = nullptr;
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->FusedPoints.Empty();
		Context->OutputPoints();
		return true;
	}

	return false;
}


#define PCGEX_FUSE_TRANSFERT(_NAME) Context->_NAME##FuseMethod = Settings->bOverride##_NAME ? Context->_NAME##FuseMethod : Settings->FuseMethod;

// TYPE, NAME, ACCESSOR
#define PCGEX_FUSE_FOREACH_POINTPROPERTY(MACRO)\
MACRO(float, Density, Density, 0) \
MACRO(FVector, Extents, GetExtents(), FVector::Zero()) \
MACRO(FVector4, Color, Color, FVector4::Zero()) \
MACRO(FVector, Position, Transform.GetLocation(), FVector::Zero()) \
MACRO(FRotator, Rotation, Transform.Rotator(), FRotator::ZeroRotator)\
MACRO(FVector, Scale, Transform.GetScale3D(), FVector::Zero()) \
MACRO(float, Steepness, Steepness, 0) \
MACRO(int32, Seed, Seed, 0)

#define PCGEX_FUSE_FOREACH_POINTPROPERTY_ASSIGN(MACRO)\
MACRO(Density, Density) \
MACRO(Color, Color) \
MACRO(Transform, Transform) \
MACRO(Steepness, Steepness) \
MACRO(Seed, Seed)

#undef PCGEX_FUSE_IGNORE
#undef PCGEX_FUSE_DECLARE
#undef PCGEX_FUSE_ASSIGN
#undef PCGEX_FUSE_UPDATE
#undef PCGEX_FUSE_FUSE
#undef PCGEX_FUSE_POST

#undef LOCTEXT_NAMESPACE