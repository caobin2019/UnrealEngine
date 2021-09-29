// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/NetSerialization.h"
#include "PhysicsInterfaceDeclaresCore.h"
#include "Chaos/Core.h"
#include "Chaos/ParticleHandle.h"
#include "RewindData.h"
#include "NetworkPredictionCVars.h"
#include "NetworkPredictionLog.h"
#include "NetworkPredictionModelDef.h"
#include "Containers/StringFwd.h"
#include "NetworkPredictionTrace.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Containers/StringConv.h"

#if WITH_CHAOS

namespace NetworkPredictionPhysicsCvars
{
	NETSIM_DEVCVAR_SHIPCONST_INT(FullPrecision, 1, "np.Physics.FullPrecision", "Replicatre physics state with full precision. Not to be toggled during gameplay.");
	NETSIM_DEVCVAR_SHIPCONST_INT(DebugPositionCorrections, 0, "np.Physics.DebugPositionCorrections", "Prints position history when correcting physics X");

	NETSIM_DEVCVAR_SHIPCONST_FLOAT(ToleranceX, 1.0, "np.Physics.Tolerance.X", "Absolute tolerance for position");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(ToleranceR, 0.1, "np.Physics.Tolerance.R", "Normalized error tolerance between rotation (0..1)");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(ToleranceV, 1.0, "np.Physics.Tolerance.V", "Absolute error tolerance for velocity ");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(ToleranceW, 1.0, "np.Physics.Tolerance.W", "Absolute error tolerance for rotational velocity ");
};

// ------------------------------------------------------------------------------------------------------
// Actual physics state. More of these could be created to include more state or change the serialization
// ------------------------------------------------------------------------------------------------------
struct NETWORKPREDICTION_API FNetworkPredictionPhysicsState
{
	Chaos::FVec3 Location;
	Chaos::FRotation3 Rotation;
	Chaos::FVec3 LinearVelocity;
	Chaos::FVec3 AngularVelocity;

	static void NetSend(const FNetSerializeParams& P, FBodyInstance* BodyInstance)
	{
		npCheckSlow(BodyInstance);
		FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
		Chaos::FRigidBodyHandle_External& Body_External = Handle->GetGameThreadAPI();
		if (NetworkPredictionPhysicsCvars::FullPrecision())
		{
			npEnsure(Body_External.CanTreatAsKinematic());
			P.Ar << const_cast<Chaos::FVec3&>(Body_External.X());
			P.Ar << const_cast<Chaos::FRotation3&>(Body_External.R());
			Chaos::FVec3 V = Body_External.V();
			Chaos::FVec3 W = Body_External.W();
			P.Ar << V;
			P.Ar << W;
		}
		else
		{
			bool bSuccess = true;
			
			SerializePackedVector<100, 30>(const_cast<Chaos::FVec3&>(Body_External.X()), P.Ar);
			const_cast<Chaos::FRotation3&>(Body_External.R()).NetSerialize(P.Ar, nullptr, bSuccess);
			
			npEnsure(Body_External.CanTreatAsKinematic());
			Chaos::FVec3 V = Body_External.V();
			Chaos::FVec3 W = Body_External.W();
			SerializePackedVector<100, 30>(V, P.Ar);
			SerializePackedVector<100, 30>(W, P.Ar);
		}
	}

	static void NetRecv(const FNetSerializeParams& P, FNetworkPredictionPhysicsState* RecvState)
	{
		npCheckSlow(RecvState);

		if (NetworkPredictionPhysicsCvars::FullPrecision())
		{
			P.Ar << RecvState->Location;
			P.Ar << RecvState->Rotation;
			P.Ar << RecvState->LinearVelocity;
			P.Ar << RecvState->AngularVelocity;
		}
		else
		{
			bool bSuccess = true;
			SerializePackedVector<100, 30>(RecvState->Location, P.Ar);
			RecvState->Rotation.NetSerialize(P.Ar, nullptr, bSuccess);
			SerializePackedVector<100, 30>(RecvState->LinearVelocity, P.Ar);
			SerializePackedVector<100, 30>(RecvState->AngularVelocity, P.Ar);
		}

		npEnsureSlow(!RecvState->ContainsNaN());
	}

	static bool ShouldReconcile(int32 PhysicsFrame, Chaos::FRewindData* RewindData, FBodyInstance* BodyInstance, FNetworkPredictionPhysicsState* RecvState)
	{
		npCheckSlow(BodyInstance);
		FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();

		auto CompareVector = [](const FVector& Local, const FVector& Authority, const float Tolerance, const FAnsiStringView& DebugStr)
		{
			const FVector Delta = Local - Authority;

			/*
			if (Delta.SizeSquared() > (Tolerance * Tolerance))
			{
				UE_NP_TRACE_SYSTEM_FAULT("Physics Compare mismatch %s", StringCast<TCHAR>(DebugStr.GetData(), DebugStr.Len()).Get());
				UE_NP_TRACE_SYSTEM_FAULT("   Pred: %s", *Local.ToString());				
				UE_NP_TRACE_SYSTEM_FAULT("   Auth: %s", *Authority.ToString());
				UE_NP_TRACE_SYSTEM_FAULT("   Delta: %s (%.f)", *Delta.ToString(), Delta.Size());
			}
			*/


			UE_NP_TRACE_RECONCILE(Delta.SizeSquared() > (Tolerance * Tolerance), DebugStr);
			
			return false;
		};

		auto CompareQuat = [](const FQuat& Local, const FQuat& Authority, const float Tolerance, const FAnsiStringView& DebugStr)
		{
			const float Error = FQuat::ErrorAutoNormalize(Local, Authority);
			UE_NP_TRACE_RECONCILE(Error > Tolerance, DebugStr);			
			return false;
		};

		const Chaos::FGeometryParticleState LocalState = RewindData->GetPastStateAtFrame(*Handle->GetHandle_LowLevel(), PhysicsFrame);

		if (CompareVector(LocalState.X(), RecvState->Location, NetworkPredictionPhysicsCvars::ToleranceX(), "X:"))
		{
			return true;
		}

		if (CompareVector(LocalState.V(), RecvState->LinearVelocity, NetworkPredictionPhysicsCvars::ToleranceV(), "V:"))
		{
			return true;
		}

		if (CompareVector(LocalState.W(), RecvState->AngularVelocity, NetworkPredictionPhysicsCvars::ToleranceW(), "W:"))
		{
			return true;
		}

		if (CompareQuat(LocalState.R(), RecvState->Rotation, NetworkPredictionPhysicsCvars::ToleranceR(), "R:"))
		{
			return true;
		}

		return false;
	}

	static void Interpolate(const FNetworkPredictionPhysicsState* From, const FNetworkPredictionPhysicsState* To, const float PCT, FNetworkPredictionPhysicsState* Out)
	{
		npCheckSlow(From);
		npCheckSlow(To);

		npEnsureMsgfSlow(!From->ContainsNaN(), TEXT("From interpolation state contains NaN"));
		npEnsureMsgfSlow(!To->ContainsNaN(), TEXT("To interpolation state contains NaN"));

		Out->Location = FMath::Lerp(From->Location, To->Location, PCT);
		Out->LinearVelocity = FMath::Lerp(From->LinearVelocity, To->LinearVelocity, PCT);
		Out->AngularVelocity = FMath::Lerp(From->AngularVelocity, To->AngularVelocity, PCT);

		Out->Rotation = FQuat::FastLerp(From->Rotation, To->Rotation, PCT);
		Out->Rotation.Normalize();
		
		npEnsureMsgfSlow(!Out->ContainsNaN(), TEXT("Out interpolation state contains NaN. %f"), PCT);
	}

	static void PerformRollback(UPrimitiveComponent* PrimitiveComponent, FNetworkPredictionPhysicsState* RecvState)
	{
		npCheckSlow(PrimitiveComponent);
		npCheckSlow(RecvState);

		// We want to update the component and physics state together without dispatching events or getting into circular loops
		// This seems like the simplest approach (update physics first then manually move the component, skipping physics update)
		FBodyInstance* BodyInstance = PrimitiveComponent->GetBodyInstance();
		PerformRollback(BodyInstance, RecvState);
		MarshalPhysicsToComponent(PrimitiveComponent, BodyInstance);
	}

	static void PerformRollback(FBodyInstance* BodyInstance, FNetworkPredictionPhysicsState* RecvState)
	{
		FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
		Chaos::FRigidBodyHandle_External& Body_External = Handle->GetGameThreadAPI();
		
		npCheckSlow(RecvState);

		npEnsureSlow(RecvState->Rotation.IsNormalized());
		npEnsureSlow(!RecvState->Location.ContainsNaN());

		Body_External.SetX(RecvState->Location);
		Body_External.SetR(RecvState->Rotation);

		npEnsureSlow(!RecvState->LinearVelocity.ContainsNaN());
		npEnsureSlow(!RecvState->AngularVelocity.ContainsNaN());

		Body_External.SetV(RecvState->LinearVelocity);
		Body_External.SetW(RecvState->AngularVelocity);
	}

	static void MarshalPhysicsToComponent(UPrimitiveComponent* PrimitiveComponent, const FBodyInstance* BodyInstance)
	{
		const FTransform UnrealTransform = BodyInstance->GetUnrealWorldTransform();
		const FVector MoveBy = UnrealTransform.GetLocation() - PrimitiveComponent->GetComponentTransform().GetLocation();
		PrimitiveComponent->MoveComponent(MoveBy, UnrealTransform.GetRotation(), false, nullptr, MOVECOMP_SkipPhysicsMove);
	}

	static void PostResimulate(UPrimitiveComponent* PrimitiveComponent, const FBodyInstance* BodyInstance)
	{
		MarshalPhysicsToComponent(PrimitiveComponent, BodyInstance);

		// We need to force a marshal of physics data -> PrimitiveComponent in cases where a sleeping object was asleep before and after a correction,
		// but waking up and calling SyncComponentToRBPhysics() is causing some bad particle data to feed back into physics. This is probably not the right
		// way. Disabling for now to avoid the asserts but will cause the sleeping object-not-updated bug to reappear.
		
		//PrimitiveComponent->WakeAllRigidBodies();
		//PrimitiveComponent->SyncComponentToRBPhysics();
		//npEnsureSlow(ActorHandle->R().IsNormalized());
	}

	static bool StateIsConsistent(UPrimitiveComponent* PrimitiveComponent, FBodyInstance* BodyInstance)
	{
		const FTransform PhysicsTransform = BodyInstance->GetUnrealWorldTransform();
		return PhysicsTransform.EqualsNoScale(PrimitiveComponent->GetComponentTransform());
	}

	// Interpolation related functions currently need to go through a UPrimitiveComponent
	static void FinalizeInterpolatedPhysics(UPrimitiveComponent* Driver, FNetworkPredictionPhysicsState* InterpolatedState);
	static void BeginInterpolation(UPrimitiveComponent* Driver);
	static void EndInterpolation(UPrimitiveComponent* Driver);

	// Networked state to string
	static void ToString(const FNetworkPredictionPhysicsState* RecvState, FAnsiStringBuilderBase& Builder)
	{
		ToStringInternal(RecvState->Location, RecvState->Rotation, RecvState->LinearVelocity, RecvState->AngularVelocity, Builder);
	}

	// Locally stored state to string
	static void ToString(int32 PhysicsFrame, Chaos::FRewindData* RewindData, FBodyInstance* BodyInstance, FAnsiStringBuilderBase& Builder)
	{
		FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
		
		const Chaos::FGeometryParticleState LocalState = RewindData->GetPastStateAtFrame(*Handle->GetHandle_LowLevel(), PhysicsFrame);
		ToStringInternal(LocalState.X(), LocalState.R(), LocalState.V(), LocalState.W(), Builder);
	}

	// Current state to string
	static void ToString(FBodyInstance* BodyInstance, FAnsiStringBuilderBase& Builder)
	{
		if (BodyInstance)
		{
			FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
			if (Handle)
			{
				Chaos::FRigidBodyHandle_External& Body_External = Handle->GetGameThreadAPI();
				npEnsure(Body_External.CanTreatAsKinematic());
				ToStringInternal(Body_External.X(), Body_External.R(), Body_External.V(), Body_External.W(), Builder);
			}
			else
			{
				Builder.Append("Null PhysicsActorHandle\n");
			}
		}
		else
		{
			Builder.Append("Null BodyInstance\n");
		}
	}

	bool ContainsNaN() const
	{
		return Location.ContainsNaN() || Rotation.ContainsNaN() || LinearVelocity.ContainsNaN() || AngularVelocity.ContainsNaN();
	}

private:

	static void ToStringInternal(const Chaos::FVec3& Location, const Chaos::FRotation3& Rotation, const Chaos::FVec3& LinearVelocity, const Chaos::FVec3& AngularVelocity, FAnsiStringBuilderBase& Builder)
	{
		Builder.Appendf("X: X=%.2f Y=%.2f Z=%.2f\n", Location.X, Location.Y, Location.Z);
		Builder.Appendf("R: X=%.2f Y=%.2f Z=%.2f W=%.2f\n", Rotation.X, Rotation.Y, Rotation.Z, Rotation.W);
		Builder.Appendf("V: X=%.2f Y=%.2f Z=%.2f\n", LinearVelocity.X, LinearVelocity.Y, LinearVelocity.Z);
		Builder.Appendf("W: X=%.2f Y=%.2f Z=%.2f\n", AngularVelocity.X, AngularVelocity.Y, AngularVelocity.Z);
	}
};

#else

struct NETWORKPREDICTION_API FNetworkPredictionPhysicsState
{
};

#endif // WITH_CHAOS

// ------------------------------------------------------------------------------------------------------
// Generic model def for physics actors with no backing simulation
// ------------------------------------------------------------------------------------------------------
struct NETWORKPREDICTION_API FGenericPhysicsModelDef : FNetworkPredictionModelDef
{
	NP_MODEL_BODY();

#if WITH_CHAOS

	using PhysicsState = FNetworkPredictionPhysicsState;
#endif // WITH_CHAOS
	using Driver = UPrimitiveComponent;

	static const TCHAR* GetName() { return TEXT("Generic Physics Actor"); }
	static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::Physics; }
};

