// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/Engine.h"
#include "MagicLeapContactsTypes.generated.h"

/** Result types for contacts requests. */
UENUM(BlueprintType)
enum class EMagicLeapContactsResult : uint8
{
	/** This handle is not yet recognized. */
	HandleNotFound,
	/** Request is completed, its corresponding result has been returned, and its related resources are marked for deletion. */
	Completed,
	/** Request failed due to system being in an illegal state, for e.g., when the user hasn't successfully logged-in. */
	IllegalState,
};

/** Search query field types. */
UENUM(BlueprintType)
enum class EMagicLeapContactsSearchField : uint8
{
	/** Search field for nickname. */
	Name,
	/** Search field for phone. */
	Phone,
	/** Search field for email. */
	Email,
	/** Search across all fields. */
	All,
};

/** Result types for contacts operations. */
UENUM(BlueprintType)
enum class EMagicLeapContactsOperationStatus : uint8
{
	/** Operation succeeded. */
	Success,
	/** Operation failed. */
	Fail,
	/** MagicLeapContact with the details specified for an insert already exists. */
	Duplicate,
	/** MagicLeapContact to be deleted/updated doesn't exist. */
	NotFound,
};

/**
	Stores a tagged value, such as phone number or email address.  Optional tag indicates what type
	of value is stored, e.g. "home", "work", etc.
*/
USTRUCT(BlueprintType)
struct MAGICLEAPCONTACTS_API FMagicLeapTaggedAttribute
{
	GENERATED_USTRUCT_BODY()

	/** Name of the Tag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contacts|MagicLeap")
	FString Tag;

	/** Value of this attribute. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contacts|MagicLeap")
	FString Value;
};

/** Representation of available information for a single contact in an address book. */
USTRUCT(BlueprintType)
struct MAGICLEAPCONTACTS_API FMagicLeapContact
{
	GENERATED_USTRUCT_BODY()

	/**
		Locally-unique contact identifier.  Generated by the system.  May change across reboots.
		@note Please do not edit this value.  It is exposed as read/write merely so that it can copied via the make/break functionality.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contacts|MagicLeap")
	FString Id;

	/** Contacts's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contacts|MagicLeap")
	FString Name;

	/** Contacts's phone numbers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contacts|MagicLeap")
	TArray<FMagicLeapTaggedAttribute> PhoneNumbers;

	/** Contacts's email addresses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contacts|MagicLeap")
	TArray<FMagicLeapTaggedAttribute> EmailAddresses;
};

/** Delegate used to convey the result of a single contact operation. */
DECLARE_DYNAMIC_DELEGATE_OneParam(FMagicLeapSingleContactResultDelegate, EMagicLeapContactsOperationStatus, OpStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMagicLeapSingleContactResultDelegateMulti, EMagicLeapContactsOperationStatus, OpStatus);

/** Delegate used to convey the result of a multiple contacts operation. */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FMagicLeapMultipleContactsResultDelegate, const TArray<FMagicLeapContact>&, Contacts, EMagicLeapContactsOperationStatus, OpStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMagicLeapMultipleContactsResultDelegateMulti, const TArray<FMagicLeapContact>&, Contacts, EMagicLeapContactsOperationStatus, OpStatus);

/**
	Delegate used to pass log messages from the contacts plugin to the initiating blueprint.
	@note This is useful if the user wishes to have log messages in 3D space.
	@param LogMessage A string containing the log message.
	@param OpStatus The status of the operation associated with the log message.
*/
DECLARE_DYNAMIC_DELEGATE_TwoParams(FMagicLeapContactsLogMessage, const FString&, LogMessage, EMagicLeapContactsOperationStatus, OpStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMagicLeapContactsLogMessageMulti, const FString&, LogMessage, EMagicLeapContactsOperationStatus, OpStatus);
