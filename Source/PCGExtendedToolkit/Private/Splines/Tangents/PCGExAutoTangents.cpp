﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Splines/Tangents/PCGExAutoTangents.h"

#include "PCGExMath.h"
#include "..\..\..\Public\Data\PCGExPointIO.h"

void UPCGExAutoTangents::ProcessFirstPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
	PCGExMath::FApex Apex = PCGExMath::FApex::FromStartOnly(
		NextPoint.Point->Transform.GetLocation(),
		MainPoint.Point->Transform.GetLocation());

	Apex.Scale(Scale);
	OutArrive = Apex.TowardStart;
	OutLeave = Apex.TowardEnd * -1;
}

void UPCGExAutoTangents::ProcessLastPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, FVector& OutArrive, FVector& OutLeave) const
{
	PCGExMath::FApex Apex = PCGExMath::FApex::FromEndOnly(
		PreviousPoint.Point->Transform.GetLocation(),
		MainPoint.Point->Transform.GetLocation());

	Apex.Scale(Scale);
	OutArrive = Apex.TowardStart;
	OutLeave = Apex.TowardEnd * -1;
}

void UPCGExAutoTangents::ProcessPoint(const PCGEx::FPointRef& MainPoint, const PCGEx::FPointRef& PreviousPoint, const PCGEx::FPointRef& NextPoint, FVector& OutArrive, FVector& OutLeave) const
{
	PCGExMath::FApex Apex = PCGExMath::FApex(
		PreviousPoint.Point->Transform.GetLocation(),
		NextPoint.Point->Transform.GetLocation(),
		MainPoint.Point->Transform.GetLocation());

	Apex.Scale(Scale);
	OutArrive = Apex.TowardStart;
	OutLeave = Apex.TowardEnd * -1;
}
