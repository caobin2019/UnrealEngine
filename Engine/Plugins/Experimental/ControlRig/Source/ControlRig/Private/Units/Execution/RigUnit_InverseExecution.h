// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "ControlRigDefines.h"
#include "RigUnit_InverseExecution.generated.h"

/**
 * Event for driving elements based off the skeleton hierarchy
 */
USTRUCT(meta=(DisplayName="Backwards Solve", Category="Inverse", TitleColor="1 0 0", NodeColor="0.1 0.1 0.1", Keywords="Inverse,Reverse,Backwards,Event"))
struct CONTROLRIG_API FRigUnit_InverseExecution : public FRigUnit
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	virtual FName GetEventName() const override { return EventName; }

	// The execution result
	UPROPERTY(EditAnywhere, Transient, DisplayName = "Execute", Category = "InverseExecution", meta = (Output))
	FControlRigExecuteContext ExecuteContext;

	static FName EventName;
};
