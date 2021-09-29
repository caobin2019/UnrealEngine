// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_DrawContainer.generated.h"

/**
 * Get Imported Draw Container curve transform and color
 */
USTRUCT(meta=(DisplayName="Get Draw Instruction", Category="Drawing", NodeColor = "0.25 0.25 0.05", Keywords = "Curve,Shape"))
struct FRigUnit_DrawContainerGetInstruction : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_DrawContainerGetInstruction()
	{
		InstructionName = NAME_None;
		Transform = FTransform::Identity;
		Color = FLinearColor::Red;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, Constant, CustomWidget = "DrawingName"))
	FName InstructionName;

	UPROPERTY(meta = (Output))
	FLinearColor Color;

	UPROPERTY(meta = (Output))
	FTransform Transform;
};

/**
 * Set Imported Draw Container curve color
 */
USTRUCT(meta = (DisplayName = "Set Draw Color", Category = "Drawing", NodeColor = "0.25 0.25 0.05", Keywords = "Curve,Shape"))
struct FRigUnit_DrawContainerSetColor: public FRigUnitMutable
{
	GENERATED_BODY()

	FRigUnit_DrawContainerSetColor()
	{
		InstructionName = NAME_None;
		Color = FLinearColor::Red;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, Constant, CustomWidget = "DrawingName"))
	FName InstructionName;

	UPROPERTY(meta = (Input))
	FLinearColor Color;
};

/**
 * Set Imported Draw Container curve thickness
 */
USTRUCT(meta = (DisplayName = "Set Draw Thickness", Category = "Drawing", NodeColor = "0.25 0.25 0.05", Keywords = "Curve,Shape"))
struct FRigUnit_DrawContainerSetThickness : public FRigUnitMutable
{
	GENERATED_BODY()

	FRigUnit_DrawContainerSetThickness()
	{
		InstructionName = NAME_None;
		Thickness = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, Constant, CustomWidget = "DrawingName"))
	FName InstructionName;

	UPROPERTY(meta = (Input))
	float Thickness;
};

/**
 * Set Imported Draw Container curve transform
 */
USTRUCT(meta = (DisplayName = "Set Draw Transform", Category = "Drawing", NodeColor = "0.25 0.25 0.05", Keywords = "Curve,Shape"))
struct FRigUnit_DrawContainerSetTransform : public FRigUnitMutable
{
	GENERATED_BODY()

	FRigUnit_DrawContainerSetTransform()
	{
		InstructionName = NAME_None;
		Transform = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, Constant, CustomWidget = "DrawingName"))
	FName InstructionName;

	UPROPERTY(meta = (Input))
	FTransform Transform;
};