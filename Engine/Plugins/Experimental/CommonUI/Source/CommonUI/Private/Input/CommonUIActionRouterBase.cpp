// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CommonUIActionRouterBase.h"
#include "Input/UIActionRouterTypes.h"
#include "Input/CommonAnalogCursor.h"
#include "Input/CommonUIInputSettings.h"

#include "Framework/Application/SlateUser.h"
#include "Slate/SObjectWidget.h"
#include "Widgets/SViewport.h"
#include "Engine/Engine.h"
#include "GameFramework/HUD.h"
#include "Engine/Console.h"
#include "Engine/Canvas.h"
#include "Stats/Stats.h"

#include "CommonUIPrivatePCH.h"
#include "CommonActivatableWidget.h"
#include "CommonUIUtils.h"
#include "CommonUISubsystemBase.h"
#include "Slate/SGameLayerManager.h"

bool bAlwaysShowCursor = false;
static const FAutoConsoleVariableRef CVarAlwaysShowCursor(
	TEXT("CommonUI.AlwaysShowCursor"),
	bAlwaysShowCursor,
	TEXT(""));

//@todo DanH: TEMP LOCATION
FGlobalUITags FGlobalUITags::GUITags;

UCommonActivatableWidget* FindOwningActivatable(const UWidget& Widget)
{
	TSharedPtr<SWidget> CurWidget = Widget.GetCachedWidget();
	ULocalPlayer* OwningLocalPlayer = Widget.GetOwningLocalPlayer();

	return UCommonUIActionRouterBase::FindOwningActivatable(CurWidget, OwningLocalPlayer);
}

//////////////////////////////////////////////////////////////////////////
// FPersistentActionCollection
//////////////////////////////////////////////////////////////////////////

class FPersistentActionCollection : public FActionRouterBindingCollection
{
public:
	FPersistentActionCollection(UCommonUIActionRouterBase& ActionRouter)
		: FActionRouterBindingCollection(ActionRouter)
	{}

	void DumpActionBindings(FString& OutputStr) const
	{
		OutputStr.Append(TEXT("\nPersistent Action Collection:"));
		DebugDumpActionBindings(OutputStr, 0);
	}

	FString DumpActionBindings() const
	{
		FString OutStr;
		DumpActionBindings(OutStr);
		return OutStr;
	}
	
};

//////////////////////////////////////////////////////////////////////////
// UCommonUIActionRouterBase
//////////////////////////////////////////////////////////////////////////

UCommonUIActionRouterBase* UCommonUIActionRouterBase::Get(const UWidget& ContextWidget)
{
	return ULocalPlayer::GetSubsystem<UCommonUIActionRouterBase>(ContextWidget.GetOwningLocalPlayer());
}

UCommonActivatableWidget* UCommonUIActionRouterBase::FindOwningActivatable(TSharedPtr<SWidget> Widget, ULocalPlayer* OwningLocalPlayer)
{
	UCommonActivatableWidget* OwningActivatable = nullptr;

	while (Widget && !OwningActivatable)
	{
		//@todo DanH: Create FActivatableWidgetMetaData and slap it onto the RebuildWidget result in CommonActivatableWidget
		Widget = Widget->GetParentWidget();
		if (Widget && Widget->GetType().IsEqual(TEXT("SObjectWidget")))
		{
			if (UCommonActivatableWidget* CandidateActivatable = Cast<UCommonActivatableWidget>(StaticCastSharedPtr<SObjectWidget>(Widget)->GetWidgetObject()))
			{
				if (CandidateActivatable->GetOwningLocalPlayer() != OwningLocalPlayer)
				{
					return nullptr;
				}
				OwningActivatable = CandidateActivatable;
			}
		}
	}

	return OwningActivatable;
}

UCommonUIActionRouterBase::UCommonUIActionRouterBase()
	: PersistentActions(MakeShared<FPersistentActionCollection>(*this))
{
	// Non-CDO behavior
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// Register "showdebug" hook.
		if (!IsRunningDedicatedServer())
		{
			AHUD::OnShowDebugInfo.AddUObject(this, &UCommonUIActionRouterBase::OnShowDebugInfo);
		}

		UConsole::RegisterConsoleAutoCompleteEntries.AddUObject(this, &UCommonUIActionRouterBase::PopulateAutoCompleteEntries);
	}
}

FUIActionBindingHandle UCommonUIActionRouterBase::RegisterUIActionBinding(const UWidget& Widget, const FBindUIActionArgs& BindActionArgs)
{
	FUIActionBindingHandle BindingHandle = FUIActionBinding::TryCreate(Widget, BindActionArgs);
	if (BindingHandle.IsValid())
	{
		FActivatableTreeNodePtr OwnerNode = nullptr;
		if (const UCommonActivatableWidget* ActivatableWidget = Cast<UCommonActivatableWidget>(&Widget))
		{
			// For an activatable widget, we want the node that pertains specifically to this widget.
			// We don't want to associate the action with one of its parents; we just want to wait for its node to be constructed.
			OwnerNode = FindNode(ActivatableWidget);
		}
		else
		{
			// For non-activatable widgets, we will accept the nearest parent node.
			OwnerNode = FindOwningNode(Widget);
		}

		if (OwnerNode)
		{
			OwnerNode->AddBinding(*FUIActionBinding::FindBinding(BindingHandle));
		}
		else if (Widget.GetCachedWidget())
		{
			// The widget is already constructed, but there's no node for it yet - defer for a frame
			FPendingWidgetRegistration& PendingRegistration = GetOrCreatePendingRegistration(Widget);
			PendingRegistration.ActionBindings.AddUnique(BindingHandle);
		}

		return BindingHandle;
	}

	return FUIActionBindingHandle();
}

bool UCommonUIActionRouterBase::RegisterLinkedPreprocessor(const UWidget& Widget, const TSharedRef<IInputProcessor>& InputPreprocessor, int32 DesiredIndex)
{
	if (FActivatableTreeNodePtr OwnerNode = FindOwningNode(Widget))
	{
		OwnerNode->AddInputPreprocessor(InputPreprocessor, DesiredIndex);
		return true;
	}
	else if (Widget.GetCachedWidget())
	{
		// The widget is already constructed, but there's no node for it yet - defer for a frame
		FPendingWidgetRegistration& PendingRegistration = GetOrCreatePendingRegistration(Widget);
		if (FPendingWidgetRegistration::FPreprocessorRegistration* ExistingEntry = PendingRegistration.Preprocessors.FindByKey(InputPreprocessor))
		{
			// Already pending - just make sure the index lines up on the off chance it changed
			ExistingEntry->DesiredIdx = DesiredIndex;
		}
		else
		{
			FPendingWidgetRegistration::FPreprocessorRegistration PreprocessorRegistration;
			PreprocessorRegistration.Preprocessor = InputPreprocessor;
			PreprocessorRegistration.DesiredIdx = DesiredIndex;
			PendingRegistration.Preprocessors.Add(PreprocessorRegistration);
		}
		return true;
	}

	return false;
}

void UCommonUIActionRouterBase::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(UCommonInputSubsystem::StaticClass());

	UCommonActivatableWidget::OnRebuilding.AddUObject(this, &UCommonUIActionRouterBase::HandleActivatableWidgetRebuilding);
	FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &UCommonUIActionRouterBase::HandlePostGarbageCollect);

	AnalogCursor = MakeAnalogCursor();
	PostAnalogCursorCreate();

	FSlateApplication::Get().OnFocusChanging().AddUObject(this, &UCommonUIActionRouterBase::HandleSlateFocusChanging);
}

void UCommonUIActionRouterBase::PostAnalogCursorCreate()
{
	RegisterAnalogCursorTick();
}

void UCommonUIActionRouterBase::RegisterAnalogCursorTick()
{
	FSlateApplication::Get().RegisterInputPreProcessor(AnalogCursor, UCommonUIInputSettings::Get().GetAnalogCursorSettings().PreprocessorPriority);
	if (bIsActivatableTreeEnabled)
	{
		FTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCommonUIActionRouterBase::Tick));
	}
}

void UCommonUIActionRouterBase::Deinitialize()
{
	Super::Deinitialize();
	
	FSlateApplication::Get().OnFocusChanging().RemoveAll(this);
	FSlateApplication::Get().UnregisterInputPreProcessor(AnalogCursor);
	FTicker::GetCoreTicker().RemoveTicker(TickHandle);
	SetActiveRoot(nullptr);
	HeldKeys.Empty();
}

bool UCommonUIActionRouterBase::ShouldCreateSubsystem(UObject* Outer) const
{
	TArray<UClass*> ChildClasses;
	GetDerivedClasses(GetClass(), ChildClasses, false);

	// Only create an instance if there is no override implementation defined elsewhere
	return ChildClasses.Num() == 0;
}

void UCommonUIActionRouterBase::SetIsActivatableTreeEnabled(bool bInIsTreeEnabled)
{
	bIsActivatableTreeEnabled = bInIsTreeEnabled;
	if (!bInIsTreeEnabled)
	{
		SetActiveRoot(nullptr);
	}
}

void UCommonUIActionRouterBase::RegisterScrollRecipient(const UWidget& ScrollableWidget)
{
	if (FActivatableTreeNodePtr OwnerNode = FindOwningNode(ScrollableWidget))
	{
		OwnerNode->AddScrollRecipient(ScrollableWidget);
	}
	else
	{
		GetOrCreatePendingRegistration(ScrollableWidget).bIsScrollRecipient = true;
	}
}

void UCommonUIActionRouterBase::UnregisterScrollRecipient(const UWidget& ScrollableWidget)
{
	if (FActivatableTreeNodePtr OwnerNode = FindOwningNode(ScrollableWidget))
	{
		OwnerNode->RemoveScrollRecipient(ScrollableWidget);
	}
	else if (FPendingWidgetRegistration* PendingRegistration = PendingWidgetRegistrations.FindByKey(ScrollableWidget))
	{
		PendingRegistration->bIsScrollRecipient = false;
	}
}

TArray<const UWidget*> UCommonUIActionRouterBase::GatherActiveAnalogScrollRecipients() const
{
	if (ActiveRootNode)
	{
		return ActiveRootNode->GatherScrollRecipients();
	}
	return TArray<const UWidget*>();
}

TArray<FUIActionBindingHandle> UCommonUIActionRouterBase::GatherActiveBindings() const
{
	TArray<FUIActionBindingHandle> BindingHandles = PersistentActions->GetActionBindings();
	if (ActiveRootNode)
	{
		ActiveRootNode->AppendAllActiveActions(BindingHandles);
	}
	return BindingHandles;
}

TSharedRef<FCommonAnalogCursor> UCommonUIActionRouterBase::MakeAnalogCursor() const
{
	// Override if desired and call CreateAnalogCursor<T> with a custom type
	return FCommonAnalogCursor::CreateAnalogCursor(*this);
}

ERouteUIInputResult UCommonUIActionRouterBase::ProcessInput(FKey Key, EInputEvent InputEvent) const
{
#if WITH_EDITOR
	// In PIE, let unmodified escape through (people expect it to close PIE)
	if (GIsPlayInEditorWorld && InputEvent == IE_Pressed && Key == EKeys::Escape)
	{
		FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
		if (!ModifierKeys.IsAltDown() && !ModifierKeys.IsCommandDown() && !ModifierKeys.IsControlDown() && !ModifierKeys.IsShiftDown())
		{
			return ERouteUIInputResult::Unhandled;
		}
	}
#endif

	ECommonInputMode ActiveMode = GetActiveInputMode();

	// Also check for repeat event here as if input is flushed when a key is being held, we will receive a released event and then continue to receive repeat events without a pressed event
	if (InputEvent == EInputEvent::IE_Pressed || InputEvent == EInputEvent::IE_Repeat)
	{
		HeldKeys.AddUnique(Key);
	}
	else if (InputEvent == EInputEvent::IE_Released)
	{
		HeldKeys.RemoveSwap(Key);
	}

	// Begin with a pass to see if the input corresponds to a hold action
	// We do this first to make sure that a higher-priority press binding doesn't prevent a hold on the same key from being triggerable
	EProcessHoldActionResult ProcessHoldResult = PersistentActions->ProcessHoldInput(ActiveMode, Key, InputEvent);
	if (bIsActivatableTreeEnabled && ActiveRootNode && ProcessHoldResult == EProcessHoldActionResult::Unhandled)
	{
		ProcessHoldResult = ActiveRootNode->ProcessHoldInput(ActiveMode, Key, InputEvent);
	}

	const auto ProcessNormalInputFunc = [Key, ActiveMode, this](EInputEvent Event)
		{
			bool bHandled = PersistentActions->ProcessNormalInput(ActiveMode, Key, Event);
			if (!bHandled && ActiveRootNode && bIsActivatableTreeEnabled)
			{
				bHandled = ActiveRootNode->ProcessNormalInput(ActiveMode, Key, Event);
			}
			return bHandled;
		};

	bool bHandledInput = ProcessHoldResult == EProcessHoldActionResult::Handled;
	if (!bHandledInput)
	{
		if (ProcessHoldResult == EProcessHoldActionResult::GeneratePress)
		{
			// A hold action was in progress but quickly aborted, so we want to generate a press action now for any normal bindings that are interested
			ProcessNormalInputFunc(IE_Pressed);
		}

		// Even if no widget cares about this input, we don't want to let anything through to the actual game while we're in menu mode
		bHandledInput = ProcessNormalInputFunc(InputEvent); 
	}

	if (bHandledInput)
	{
		return ERouteUIInputResult::Handled;
	}
	return CanProcessNormalGameInput() ? ERouteUIInputResult::Unhandled : ERouteUIInputResult::BlockGameInput;
}

UCommonInputSubsystem& UCommonUIActionRouterBase::GetInputSubsystem() const
{
	UCommonInputSubsystem* InputSubsytem = GetLocalPlayerChecked()->GetSubsystem<UCommonInputSubsystem>();
	check(InputSubsytem);
	return *InputSubsytem;
}

void UCommonUIActionRouterBase::FlushInput()
{
	const ECommonInputMode ActiveMode = GetActiveInputMode();
	for (const FKey& HeldKey : HeldKeys)
	{
		const EProcessHoldActionResult ProcessHoldResult = PersistentActions->ProcessHoldInput(ActiveMode, HeldKey, EInputEvent::IE_Released);
		if (bIsActivatableTreeEnabled && ActiveRootNode && ProcessHoldResult == EProcessHoldActionResult::Unhandled)
		{
			ActiveRootNode->ProcessHoldInput(ActiveMode, HeldKey, EInputEvent::IE_Released);
		}
	}

	HeldKeys.Empty();
}

bool UCommonUIActionRouterBase::IsWidgetInActiveRoot(const UCommonActivatableWidget* Widget) const
{
	if (Widget && ActiveRootNode)
	{
		TSharedPtr<SWidget> WidgetWalker = Widget->GetCachedWidget();
		while (WidgetWalker)
		{
			if (WidgetWalker->GetType().IsEqual(TEXT("SObjectWidget")))
			{
				if (UCommonActivatableWidget* CandidateActivatable = Cast<UCommonActivatableWidget>(StaticCastSharedPtr<SObjectWidget>(WidgetWalker)->GetWidgetObject()))
				{
					if (CandidateActivatable == ActiveRootNode->GetWidget())
					{
						return true;
					}
				}
			}
			WidgetWalker = WidgetWalker->GetParentWidget();
		}
	}
	return false;
}

void UCommonUIActionRouterBase::NotifyUserWidgetConstructed(const UCommonUserWidget& Widget)
{
	check(Widget.GetCachedWidget().IsValid());

	if (FActivatableTreeNodePtr OwnerNode = FindOwningNode(Widget))
	{
		RegisterWidgetBindings(OwnerNode, Widget.GetActionBindings());
	}
	else if (Widget.GetActionBindings().Num() > 0)
	{
		GetOrCreatePendingRegistration(Widget).ActionBindings.Append(Widget.GetActionBindings());
	}
}

void UCommonUIActionRouterBase::NotifyUserWidgetDestructed(const UCommonUserWidget& Widget)
{
	const int32 PendingRegistrationIdx = PendingWidgetRegistrations.IndexOfByKey(Widget);
	if (PendingRegistrationIdx == INDEX_NONE)
	{
		// The widget wasn't pending registration, so the bindings need to be removed
		// Not worth splitting out which bindings are persistent vs. normal, just have both collections try to remove all the bindings on the widget
		PersistentActions->RemoveBindings(Widget.GetActionBindings());
		if (FActivatableTreeNodePtr OwnerNode = FindOwningNode(Widget))
		{
			OwnerNode->RemoveBindings(Widget.GetActionBindings());
		}
	}
	else
	{
		PendingWidgetRegistrations.RemoveAt(PendingRegistrationIdx);
	}
}

void UCommonUIActionRouterBase::AddBinding(FUIActionBindingHandle Handle)
{
	if (TSharedPtr<FUIActionBinding> Binding = FUIActionBinding::FindBinding(Handle))
	{
		if (const UWidget* BoundWidget = Binding->BoundWidget.Get())
		{
			if (FActivatableTreeNodePtr OwnerNode = FindOwningNode(*BoundWidget))
			{
				if (Binding->bIsPersistent)
				{
					PersistentActions->AddBinding(*Binding);
				}
				else
				{
					OwnerNode->AddBinding(*Binding);
				}
			}
			else if (BoundWidget->GetCachedWidget())
			{
				GetOrCreatePendingRegistration(*BoundWidget).ActionBindings.AddUnique(Handle);
			}
		}
	}
}

void UCommonUIActionRouterBase::RemoveBinding(FUIActionBindingHandle Handle)
{
	if (TSharedPtr<FUIActionBinding> Binding = FUIActionBinding::FindBinding(Handle))
	{
		if (Binding->OwningCollection.IsValid())
		{
			Binding->OwningCollection.Pin()->RemoveBinding(Handle);
		}
		else if (FPendingWidgetRegistration* PendingRegistration = PendingWidgetRegistrations.FindByKey(Binding->BoundWidget.Get()))
		{
			PendingRegistration->ActionBindings.Remove(Handle);
		}
	}
}

int32 UCommonUIActionRouterBase::GetLocalPlayerIndex() const
{
	ULocalPlayer* LocalPlayer = GetLocalPlayerChecked();
	return LocalPlayer->GetGameInstance()->GetLocalPlayers().Find(LocalPlayer);
}

bool UCommonUIActionRouterBase::ShouldAlwaysShowCursor() const
{
	bool bUsingMouseForTouch = FSlateApplication::Get().IsFakingTouchEvents();
	ULocalPlayer& LocalPlayer = *GetLocalPlayerChecked();
	if (UGameViewportClient* GameViewportClient = LocalPlayer.ViewportClient)
	{
		bUsingMouseForTouch |= GameViewportClient->GetUseMouseForTouch();
	}
	return bAlwaysShowCursor || bUsingMouseForTouch;
}

ECommonInputMode UCommonUIActionRouterBase::GetActiveInputMode(ECommonInputMode DefaultInputMode) const
{
	return ActiveInputConfig.IsSet() ? ActiveInputConfig.GetValue().GetInputMode() : DefaultInputMode;
}

EMouseCaptureMode UCommonUIActionRouterBase::GetActiveMouseCaptureMode(EMouseCaptureMode DefaultMouseCapture) const
{
	return ActiveInputConfig.IsSet() ? ActiveInputConfig.GetValue().GetMouseCaptureMode() : DefaultMouseCapture;
}

void UCommonUIActionRouterBase::HandleRootWidgetSlateReleased(UCommonActivatableWidget* ActivatableWidget)
{
	ActivatableWidget->OnSlateReleased().RemoveAll(this);

	const int32 RootToRemoveIdx = RootNodes.IndexOfByPredicate([ActivatableWidget](const FActivatableTreeRootRef& RootNode) { return RootNode->GetWidget() == ActivatableWidget; });
	if (ensure(RootToRemoveIdx != INDEX_NONE))
	{
		// It's possible that the widget is destructed as a result of some other deactivation handler, causing us to get here before hearing about
		// the deactivation. Not a big deal, just need to process the deactivation right here if the node in question is the active root.
		if (ActiveRootNode == RootNodes[RootToRemoveIdx])
		{
			check(!ActiveRootNode->IsWidgetActivated());
			HandleRootNodeDeactivated(ActiveRootNode);
		}

		TWeakPtr<FActivatableTreeRoot> ToBeRemoved = RootNodes[RootToRemoveIdx];
		RootNodes.RemoveAtSwap(RootToRemoveIdx);
		// Cannot actually have this ensure here, because we may be in a function called on the ToBeRemoved node itself, keeping one remaining strong reference
		// ensureAlways(!ToBeRemoved.IsValid());

		//@todo DanH: This may not ever actually happen, since we'll likely want the loading screen to be an activatable - we should be listening for map changes instead
		if (RootNodes.Num() == 0)
		{
			//@todo DanH: This won't actually change the current config, which we may want to do with a loading screen
			ActiveInputConfig.Reset();
		}
	}
}

void UCommonUIActionRouterBase::HandleRootNodeActivated(TWeakPtr<FActivatableTreeRoot> WeakActivatedRoot)
{
	FActivatableTreeRootRef ActivatedRoot = WeakActivatedRoot.Pin().ToSharedRef();
	if (!ensureAlways(RootNodes.Contains(ActivatedRoot)))
	{
		return;
	}

	if (ActivatedRoot->GetLastPaintLayer() > 0)
	{
		const int32 CurrentRootLayer = ActiveRootNode ? ActiveRootNode->GetLastPaintLayer() : INDEX_NONE;
		if (ActivatedRoot->GetLastPaintLayer() > CurrentRootLayer)
		{
			SetActiveRoot(ActivatedRoot);
		}
	}
}

void UCommonUIActionRouterBase::HandleRootNodeDeactivated(TWeakPtr<FActivatableTreeRoot> WeakDeactivatedRoot)
{
	if (ActiveRootNode && ActiveRootNode == WeakDeactivatedRoot.Pin())
	{
		// Reset the active root widget - we'll re-establish it on the next tick
		SetActiveRoot(nullptr);
	}
}

void UCommonUIActionRouterBase::HandleLeafmostActiveNodeChanged()
{
	OnBoundActionsUpdated().Broadcast();
}

void UCommonUIActionRouterBase::HandleSlateFocusChanging(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldFocusedWidgetPath, const TSharedPtr<SWidget>& OldFocusedWidget, const FWidgetPath& NewFocusedWidgetPath, const TSharedPtr<SWidget>& NewFocusedWidget)
{
	if (FocusEvent.GetCause() == EFocusCause::SetDirectly && FocusEvent.GetUser() == GetLocalPlayerIndex() && ActiveRootNode && ActiveRootNode->IsExclusiveParentOfWidget(OldFocusedWidget))
	{
		ActiveRootNode->RefreshCachedRestorationTarget();
	}
}

void UCommonUIActionRouterBase::HandlePostGarbageCollect()
{
	FUIActionBinding::CleanRegistrations();
}

void UCommonUIActionRouterBase::ProcessRebuiltWidgets()
{
	// Begin by organizing all of the widgets that need nodes according to their direct parent
	TArray<UCommonActivatableWidget*> RootCandidates;
	TMap<UCommonActivatableWidget*, TArray<UCommonActivatableWidget*>> WidgetsByDirectParent;
	for (TWeakObjectPtr<UCommonActivatableWidget> RebuiltWidget : RebuiltWidgetsPendingNodeAssignment)
	{
		if (RebuiltWidget.IsValid() && RebuiltWidget->GetCachedWidget())
		{
			if (UCommonActivatableWidget* ActivatableParent = !RebuiltWidget->IsModal() ? ::FindOwningActivatable(*RebuiltWidget) : nullptr)
			{
				WidgetsByDirectParent.FindOrAdd(ActivatableParent).Add(RebuiltWidget.Get());
			}
			else
			{
				// Parent-less (or modal), so add an entry for it as a root candidate
				RootCandidates.Add(RebuiltWidget.Get());
			}
		}
	}

	// Build a new tree for any new roots
	for (UCommonActivatableWidget* RootWidget : RootCandidates)
	{
		FActivatableTreeRootRef RootNode = FActivatableTreeRoot::Create(*this, *RootWidget);

		TWeakPtr<FActivatableTreeRoot> WeakRoot(RootNode);
		RootNode->OnActivated.BindUObject(this, &UCommonUIActionRouterBase::HandleRootNodeActivated, WeakRoot);
		RootNode->OnDeactivated.BindUObject(this, &UCommonUIActionRouterBase::HandleRootNodeDeactivated, WeakRoot);
		RootWidget->OnSlateReleased().AddUObject(this, &UCommonUIActionRouterBase::HandleRootWidgetSlateReleased, RootWidget);
		RootNodes.Add(RootNode);

		AssembleTreeRecursive(RootNode, WidgetsByDirectParent);

		if (RootWidget->IsActivated())
		{
			// If we've created a root for a widget that's already active, process that activation now (ensures we have an appropriate active root)
			HandleRootNodeActivated(WeakRoot);
		}
	}

	// Now process any remaining entries - these are widgets that were rebuilt but should be appended to an existing node
	int32 NumWidgetsLeft = INDEX_NONE;
	while (WidgetsByDirectParent.Num() > 0 && NumWidgetsLeft != WidgetsByDirectParent.Num())
	{
		// If we run this loop twice without removing any entries from the map, we're in trouble
		NumWidgetsLeft = WidgetsByDirectParent.Num();

		// The keys in here fall into one of two categories - either they should be appended directly to an existing node, or they are a child of another key here
		// So, we can just go through looking for keys with an owner that already has a node. Then we can BuildTreeFunc from there with ease.
		for (const TPair<UCommonActivatableWidget*, TArray<UCommonActivatableWidget*>>& ParentChildrenPair : WidgetsByDirectParent)
		{
			if (FActivatableTreeNodePtr ExistingNode = FindNode(ParentChildrenPair.Key))
			{
				AssembleTreeRecursive(ExistingNode.ToSharedRef(), WidgetsByDirectParent);
				break;
			}
		}
	}

	if (WidgetsByDirectParent.Num() > 0)
	{
		//@todo DanH: Build a string to print all the remaining entries
		ensureAlwaysMsgf(false, TEXT("Somehow we rebuilt a widget that is owned by an activatable, but no node exists for that activatable. This *should* be completely impossible."));
	}
	
	// Now, we account for all the widgets that would like their actions bound
	for (const FPendingWidgetRegistration& PendingRegistration : PendingWidgetRegistrations)
	{
		const UWidget* Widget = PendingRegistration.Widget.Get();
		if (Widget && Widget->GetCachedWidget().IsValid())
		{
			FActivatableTreeNodePtr OwnerNode = FindOwningNode(*Widget);
			RegisterWidgetBindings(OwnerNode, PendingRegistration.ActionBindings);

			if ((PendingRegistration.bIsScrollRecipient || PendingRegistration.Preprocessors.Num() > 0) && ensureMsgf(OwnerNode, TEXT("Widget [%s] does not have a parent activatable widget at any level - cannot register preprocessors or as a scroll recipient"), *Widget->GetName()))
			{
				if (PendingRegistration.bIsScrollRecipient)
				{
					OwnerNode->AddScrollRecipient(*Widget);
				}

				for (const auto& PreprocessorInfo : PendingRegistration.Preprocessors)
				{
					OwnerNode->AddInputPreprocessor(PreprocessorInfo.Preprocessor.ToSharedRef(), PreprocessorInfo.DesiredIdx);
				}
			}
		}
	}

	RebuiltWidgetsPendingNodeAssignment.Reset();
	PendingWidgetRegistrations.Reset();
}

void UCommonUIActionRouterBase::AssembleTreeRecursive(const FActivatableTreeNodeRef& CurNode, TMap<UCommonActivatableWidget*, TArray<UCommonActivatableWidget*>>& WidgetsByDirectParent)
{
	TArray<UCommonActivatableWidget*> Children;
	if (WidgetsByDirectParent.RemoveAndCopyValue(CurNode->GetWidget(), Children))
	{
		for (UCommonActivatableWidget* ActivatableWidget : Children)
		{
			FActivatableTreeNodeRef NewNode = CurNode->AddChildNode(*ActivatableWidget);
			AssembleTreeRecursive(NewNode, WidgetsByDirectParent);
		}
	}
}

bool UCommonUIActionRouterBase::Tick(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UCommonUIActionRouter_Tick);
	if (PendingWidgetRegistrations.Num() > 0 || RebuiltWidgetsPendingNodeAssignment.Num() > 0)
	{
		ProcessRebuiltWidgets();
	}

	if (bIsActivatableTreeEnabled)
	{
		int32 HighestPaintLayer = ActiveRootNode ? ActiveRootNode->GetLastPaintLayer() : INDEX_NONE;
		FActivatableTreeRootPtr NewActiveRoot = ActiveRootNode;
		for (const FActivatableTreeRootRef& Root : RootNodes)
		{
			if (Root->IsWidgetActivated())
			{
				int32 CurrentRootLayer = Root->GetLastPaintLayer();
				if (CurrentRootLayer > HighestPaintLayer)
				{
					HighestPaintLayer = CurrentRootLayer;
					NewActiveRoot = Root;
				}
			}
		}

		if (NewActiveRoot != ActiveRootNode)
		{
			SetActiveRoot(NewActiveRoot);
		}
	}

	const ECommonInputMode ActiveMode = GetActiveInputMode();
	for (const FKey& HeldKey : HeldKeys)
	{
		const EProcessHoldActionResult ProcessHoldResult = PersistentActions->ProcessHoldInput(ActiveMode, HeldKey, EInputEvent::IE_Repeat);
		if (bIsActivatableTreeEnabled && ActiveRootNode && ProcessHoldResult == EProcessHoldActionResult::Unhandled)
		{
			ActiveRootNode->ProcessHoldInput(ActiveMode, HeldKey, EInputEvent::IE_Repeat);
		}
	}

	return true; //continue ticking
}

void UCommonUIActionRouterBase::OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	static const FName NAME_ActionRouter("ActionRouter");
	if (!Canvas || !HUD->ShouldDisplayDebug(NAME_ActionRouter))
	{
		return;
	}
	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	DisplayDebugManager.SetFont(GEngine->GetSmallFont());


	static UEnum* InputModeEnum = StaticEnum<ECommonInputMode>();
	static UEnum* MouseCaptureModeEnum = StaticEnum<EMouseCaptureMode>();
	static UEnum* InputTypeEnum = StaticEnum<ECommonInputType>();

	check(InputModeEnum);
	check(MouseCaptureModeEnum);
	check(InputTypeEnum);

	const UCommonInputSubsystem& InputSystem = GetInputSubsystem();
	ECommonInputType CurrentInputType = InputSystem.GetCurrentInputType();

	ULocalPlayer* LocalPlayer = GetLocalPlayerChecked();
	int32 ControllerId = LocalPlayer->GetControllerId();

	DisplayDebugManager.SetDrawColor(FColor::White);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("Action Router - Player [%d]: Input Type[%s]"), ControllerId, *InputTypeEnum->GetNameStringByValue((int64)CurrentInputType)));
	if (ActiveInputConfig.IsSet())
	{
		FString InputModeStr = InputModeEnum->GetNameStringByValue((int64)ActiveInputConfig->GetInputMode());
		FString MouseCaptureStr = MouseCaptureModeEnum->GetNameStringByValue((int64)ActiveInputConfig->GetMouseCaptureMode());

		DisplayDebugManager.DrawString(FString::Printf(TEXT("    Input Mode [%s] Mouse Capture [%s]"), *InputModeStr, *MouseCaptureStr));
	}
	else
	{
		DisplayDebugManager.SetDrawColor(FColor::Red);
		DisplayDebugManager.DrawString(FString(TEXT("    No Input Config")));
	}

	DisplayDebugManager.SetDrawColor(FColor::White);
	DisplayDebugManager.DrawString(PersistentActions->DumpActionBindings());
}

void UCommonUIActionRouterBase::PopulateAutoCompleteEntries(TArray<FAutoCompleteCommand>& AutoCompleteList)
{
	const UConsoleSettings* ConsoleSettings = GetDefault<UConsoleSettings>();

	AutoCompleteList.AddDefaulted();

	FAutoCompleteCommand& AutoCompleteCommand = AutoCompleteList.Last();

	AutoCompleteCommand.Command = TEXT("showdebug ActionRouter");
	AutoCompleteCommand.Desc = TEXT("Toggles display of Action Router");
	AutoCompleteCommand.Color = ConsoleSettings->AutoCompleteCommandColor;
}

bool UCommonUIActionRouterBase::CanProcessNormalGameInput() const
{
	if (GetActiveInputMode() == ECommonInputMode::Menu)
	{
		// We still process normal game input in menu mode if the game viewport has mouse capture
		// This allows manipulation of preview items and characters in the world while in menus. 
		// If this is not desired, disable viewport mouse capture in your desired input config.
		const ULocalPlayer& LocalPlayer = *GetLocalPlayerChecked();
		if (TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(GetLocalPlayerIndex()))
		{
			return LocalPlayer.ViewportClient && SlateUser->DoesWidgetHaveCursorCapture(LocalPlayer.ViewportClient->GetGameViewportWidget());
		}
	}
	return true;
}

bool UCommonUIActionRouterBase::IsPendingTreeChange() const
{
	return RebuiltWidgetsPendingNodeAssignment.Num() > 0;
}

void UCommonUIActionRouterBase::RegisterWidgetBindings(const FActivatableTreeNodePtr& TreeNode, const TArray<FUIActionBindingHandle>& BindingHandles)
{
	for (FUIActionBindingHandle Handle : BindingHandles)
	{
		if (TSharedPtr<FUIActionBinding> Binding = FUIActionBinding::FindBinding(Handle))
		{
			if (Binding->bIsPersistent)
			{
				PersistentActions->AddBinding(*Binding);
			}
			else if (ensureMsgf(TreeNode, TEXT("Widget [%s] does not have a parent activatable widget at any level - cannot register standard binding to action [%s]. UserWidget parent(s): %s"),
				*Binding->BoundWidget->GetName(),
				*Binding->ActionName.ToString(),
				*CommonUIUtils::PrintAllOwningUserWidgets(Binding->BoundWidget.Get())))
			{
				TreeNode->AddBinding(*Binding);
			}
		}
	}
}

void UCommonUIActionRouterBase::RefreshActiveRootFocus()
{
	if (ActiveRootNode)
	{
		ActiveRootNode->FocusLeafmostNode();
	}
}

void UCommonUIActionRouterBase::RefreshUIInputConfig()
{
	if (ActiveInputConfig.IsSet())
	{
		ApplyUIInputConfig(ActiveInputConfig.GetValue(), /*bForceRefresh*/ true);
	}
}

void UCommonUIActionRouterBase::SetActiveRoot(FActivatableTreeRootPtr NewActiveRoot)
{
	if (ActiveRootNode)
	{
		ActiveRootNode->OnLeafmostActiveNodeChanged.Unbind();
		ActiveRootNode->SetCanReceiveInput(false);
	}

	if (bForceResetActiveRoot || !bIsActivatableTreeEnabled)
	{
		// Never activate a root while dormant or the tree is disabled
		bForceResetActiveRoot = false;
		ActiveRootNode.Reset();
	}
	else
	{
		ActiveRootNode = NewActiveRoot;
		if (NewActiveRoot)
		{
			NewActiveRoot->SetCanReceiveInput(true);
			NewActiveRoot->OnLeafmostActiveNodeChanged.BindUObject(this, &UCommonUIActionRouterBase::HandleLeafmostActiveNodeChanged);
		}
	}

	OnBoundActionsUpdated().Broadcast();
}

void UCommonUIActionRouterBase::SetForceResetActiveRoot(bool bInForceResetActiveRoot)
{
	bForceResetActiveRoot = bInForceResetActiveRoot;
}

UCommonUIActionRouterBase::FPendingWidgetRegistration& UCommonUIActionRouterBase::GetOrCreatePendingRegistration(const UWidget& Widget)
{
	if (FPendingWidgetRegistration* ExistingEntry = PendingWidgetRegistrations.FindByKey(Widget))
	{
		return *ExistingEntry;
	}

	FPendingWidgetRegistration NewEntry;
	NewEntry.Widget = &Widget;
	return PendingWidgetRegistrations[PendingWidgetRegistrations.Add(NewEntry)];;
}

FActivatableTreeNodePtr UCommonUIActionRouterBase::FindNode(const UCommonActivatableWidget* Widget) const
{
	FActivatableTreeNodePtr FoundNode;
	if (Widget)
	{
		const bool bIsModal = Widget->IsModal();
		for (const FActivatableTreeRootPtr RootNode : RootNodes)
		{
			if (!bIsModal)
			{
				FoundNode = FindNodeRecursive(RootNode, *Widget);
			}
			else if (Widget == RootNode->GetWidget())
			{
				// If we're looking for a modal's node, we only need to check the roots
				FoundNode = RootNode;
			}

			if (FoundNode.IsValid())
			{
				break;
			}
		}
	}

	return FoundNode;
}

FActivatableTreeNodePtr UCommonUIActionRouterBase::FindOwningNode(const UWidget& Widget) const
{
	const UCommonActivatableWidget* ActivatableWidget = Cast<UCommonActivatableWidget>(&Widget);
	FActivatableTreeNodePtr FoundNode = FindNode(ActivatableWidget);

	// Don't search beyond the roots if we're looking for a modal activatable
	if (!FoundNode && (!ActivatableWidget || !ActivatableWidget->IsModal()))
	{
		if (UCommonActivatableWidget* OwningActivatable = ::FindOwningActivatable(Widget))
		{
			FoundNode = FindNode(OwningActivatable);
		}
	}
	return FoundNode;
}

FActivatableTreeNodePtr UCommonUIActionRouterBase::FindNodeRecursive(const FActivatableTreeNodePtr& CurrentNode, const UCommonActivatableWidget& Widget) const
{
	FActivatableTreeNodePtr FoundNode;
	if (CurrentNode)
	{
		if (CurrentNode->GetWidget() == &Widget)
		{
			FoundNode = CurrentNode;
		}
		else
		{
			for (const FActivatableTreeNodePtr Child : CurrentNode->GetChildren())
			{
				FoundNode = FindNodeRecursive(Child, Widget);
				if (FoundNode)
				{
					break;
				}
			}
		}
	}
	return FoundNode;
}

FActivatableTreeNodePtr UCommonUIActionRouterBase::FindNodeRecursive(const FActivatableTreeNodePtr& CurrentNode, const TSharedPtr<SWidget>& Widget) const
{
	FActivatableTreeNodePtr FoundNode;
	if (CurrentNode)
	{
		TSharedPtr<SWidget> CachedWidget = CurrentNode->GetWidget() ? CurrentNode->GetWidget()->GetCachedWidget() : nullptr;

		// only want to check leaf nodes
		if (CurrentNode->GetChildren().Num() == 0)
		{
			if (CurrentNode->IsExclusiveParentOfWidget(Widget))
			{
				FoundNode = CurrentNode;
			}
		}
		else
		{
			for (const FActivatableTreeNodePtr Child : CurrentNode->GetChildren())
			{
				FoundNode = FindNodeRecursive(Child, Widget);
				if (FoundNode)
				{
					break;
				}
			}
		}
	}
	return FoundNode;
}

void UCommonUIActionRouterBase::SetActiveUIInputConfig(const FUIInputConfig& NewConfig)
{
	ApplyUIInputConfig(NewConfig, !ActiveInputConfig.IsSet());
}

void UCommonUIActionRouterBase::ApplyUIInputConfig(const FUIInputConfig& NewConfig, bool bForceRefresh)
{
	if (bForceRefresh || NewConfig != ActiveInputConfig.GetValue())
	{
		const ECommonInputMode PreviousInputMode = GetActiveInputMode();

		ActiveInputConfig = NewConfig;

		ULocalPlayer& LocalPlayer = *GetLocalPlayerChecked();

		//@todo DanH: This won't quite work for splitscreen - we need per-player viewport client settings for mouse capture
		if (UGameViewportClient* GameViewportClient = LocalPlayer.ViewportClient)
		{
			if (TSharedPtr<SViewport> ViewportWidget = GameViewportClient->GetGameViewportWidget())
			{
				if (APlayerController* PC = LocalPlayer.GetPlayerController(GetWorld()))
				{
					EMouseCaptureMode PrevCaptureMode = GameViewportClient->GetMouseCaptureMode();
					const bool bWasPermanentlyCaptured = PrevCaptureMode == EMouseCaptureMode::CapturePermanently || PrevCaptureMode == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown;

					GameViewportClient->SetMouseCaptureMode(NewConfig.GetMouseCaptureMode());
					GameViewportClient->SetHideCursorDuringCapture(NewConfig.HideCursorDuringViewportCapture() && !ShouldAlwaysShowCursor());

					FReply& SlateOperations = LocalPlayer.GetSlateOperations();
					const EMouseCaptureMode CaptureMode = NewConfig.GetMouseCaptureMode();
					switch (CaptureMode)
					{
					case EMouseCaptureMode::CapturePermanently:
					case EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown:
					{
						GameViewportClient->SetMouseLockMode(EMouseLockMode::LockOnCapture);
						PC->SetShowMouseCursor(ShouldAlwaysShowCursor());

						TSharedRef<SViewport> ViewportWidgetRef = ViewportWidget.ToSharedRef();
						SlateOperations.UseHighPrecisionMouseMovement(ViewportWidgetRef);
						SlateOperations.SetUserFocus(ViewportWidgetRef);
						SlateOperations.LockMouseToWidget(ViewportWidgetRef);
						SlateOperations.CaptureMouse(ViewportWidgetRef);
					}
					break;
					case EMouseCaptureMode::NoCapture:
					case EMouseCaptureMode::CaptureDuringMouseDown:
					case EMouseCaptureMode::CaptureDuringRightMouseDown:
					{
						GameViewportClient->SetMouseLockMode(EMouseLockMode::DoNotLock);
						PC->SetShowMouseCursor(true);

						SlateOperations.ReleaseMouseLock();
						SlateOperations.ReleaseMouseCapture();

						// If the mouse was captured previously, set it back to the center of the viewport now that we're showing it again 
						// (don't bother on touch, when refreshing an input config, or when we're setting up the initial config - the cursor isn't really relevant there)
						if (!bForceRefresh && bWasPermanentlyCaptured && GetInputSubsystem().GetCurrentInputType() != ECommonInputType::Touch)
						{
							TSharedPtr<FSlateUser> SlateUser = LocalPlayer.GetSlateUser();
							TSharedPtr<IGameLayerManager> GameLayerManager = GameViewportClient->GetGameLayerManager();
							if (ensure(SlateUser) && ensure(GameLayerManager))
							{
								FGeometry PlayerViewGeometry = GameLayerManager->GetPlayerWidgetHostGeometry(&LocalPlayer);
								const FVector2D AbsoluteViewCenter = PlayerViewGeometry.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f));
								SlateUser->SetCursorPosition(AbsoluteViewCenter);
							}
						}
					}
					break;
					}
				}
			}
		}

		if (PreviousInputMode != NewConfig.GetInputMode())
		{
			OnActiveInputModeChanged().Broadcast(NewConfig.GetInputMode());
		}
	}
}

void UCommonUIActionRouterBase::SetActiveUICameraConfig(const FUICameraConfig& NewConfig)
{
	OnCameraConfigChanged().Broadcast(NewConfig);
}

void UCommonUIActionRouterBase::HandleActivatableWidgetRebuilding(UCommonActivatableWidget& RebuildingWidget)
{
	if (RebuildingWidget.GetOwningLocalPlayer() == GetLocalPlayerChecked())
	{
		RebuiltWidgetsPendingNodeAssignment.Add(&RebuildingWidget);
	}
}

//////////////////////////////////////////////////////////////////////////
// Debug Utils - may merit its own file?
//////////////////////////////////////////////////////////////////////////

extern const TCHAR* InputEventToString(EInputEvent InputEvent);
class FActionRouterDebugUtils
{
public:
	static void HandleDebugDumpTree(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			return;
		}

		const bool bIncludeActions = !Args.IsValidIndex(0) || Args[0].ToBool();
		const bool bIncludeChildren = !Args.IsValidIndex(1) || Args[1].ToBool();
		const bool bIncludeInactive = !Args.IsValidIndex(2) || Args[2].ToBool();
		const int LocalPlayerIndex = Args.IsValidIndex(3) ? FCString::Atoi(*Args[3]) : -1;

		UGameInstance* GameInstance = World->GetGameInstance();
		const TArray<ULocalPlayer*>& LocalPlayers = GameInstance->GetLocalPlayers();
		for (int32 CurrIdx = 0; CurrIdx < LocalPlayers.Num(); ++CurrIdx)
		{
			if (LocalPlayerIndex != -1 && LocalPlayerIndex != CurrIdx)
			{
				continue;
			}
			ULocalPlayer* LocalPlayer = LocalPlayers[CurrIdx];
			if (UCommonUIActionRouterBase* ActionRouter = LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>())
			{
				FString TreeOutputStr;
				
				if (ActionRouter->ActiveRootNode)
				{
					TreeOutputStr.Append(TEXT("** Active Root **"));
					ActionRouter->ActiveRootNode->DebugDump(TreeOutputStr, bIncludeActions, bIncludeChildren, bIncludeInactive);
					TreeOutputStr.Append(TEXT("\n*****************\n"));
				}
				
				for (const FActivatableTreeRootRef& RootNode : ActionRouter->RootNodes)
				{
					if (RootNode != ActionRouter->ActiveRootNode)
					{
						RootNode->DebugDump(TreeOutputStr, bIncludeActions, bIncludeChildren, bIncludeInactive);
					}
				}

				if (bIncludeActions)
				{
					ActionRouter->PersistentActions->DumpActionBindings(TreeOutputStr);
				}

				UE_LOG(LogUIActionRouter, Display, TEXT("Dumping ActivatableWidgetTree for LocalPlayer [User %d, ControllerId %d]:\n\n%s\n"), CurrIdx, LocalPlayer->GetControllerId(), *TreeOutputStr);
			}
		}
	}

	static void HandleDumpCurrentInputConfig(UWorld* World)
	{
		if (World == nullptr)
		{
			return;
		}

		static UEnum* InputModeEnum = StaticEnum<ECommonInputMode>();
		static UEnum* MouseCaptureModeEnum = StaticEnum<EMouseCaptureMode>();

		check(InputModeEnum);
		check(MouseCaptureModeEnum);

		UGameInstance* GameInstance = World->GetGameInstance();
		FString OutStr;
		const TArray<ULocalPlayer*> LocalPlayers = GameInstance->GetLocalPlayers();
		for (int32 i = 0; i < LocalPlayers.Num(); ++i)
		{
			if (ULocalPlayer* LocalPlayer = LocalPlayers[i])
			{
				const int32 ControllerId = LocalPlayer->GetControllerId();
				if (UCommonUIActionRouterBase* ActionRouter = LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>())
				{
					if (ActionRouter->ActiveInputConfig.IsSet())
					{
						FString InputModeStr = InputModeEnum->GetNameStringByValue((int64)ActionRouter->ActiveInputConfig->GetInputMode());
						FString MouseCaptureStr = MouseCaptureModeEnum->GetNameStringByValue((int64)ActionRouter->ActiveInputConfig->GetMouseCaptureMode());
						FString HideStr = ActionRouter->ActiveInputConfig->HideCursorDuringViewportCapture() ? TEXT("Yes") : TEXT("No");
						OutStr += FString::Printf(TEXT("\tLocalPlayer[User %d, ControllerId %d] ActiveInputConfig: Input Mode [%s] Mouse Capture [%s] Hide Cursor During Capture [%s]\n"), i, ControllerId, *InputModeStr, *MouseCaptureStr, *HideStr);
					}
					else
					{
						OutStr += FString::Printf(TEXT("LocalPlayer [User %d, ControllerId %d] no ActiveInputConfig\n"), i, ControllerId);

					}
				}
				else
				{
					OutStr += FString::Printf(TEXT("LocalPlayer [User %d, Controller %d] has no ActionRouter\n"), i, ControllerId);
				}
			}
		}
		UE_LOG(LogUIActionRouter, Display, TEXT("Dumping all Input configs:\n%s"), *OutStr);
	}
};

//@todo DanH: Debug output for this stuff - Cheatscript? ShowDebug? Full monitor window a-la WidgetReflector?
static const FAutoConsoleCommandWithWorldAndArgs DumpActivatableTreeCommand(
	TEXT("CommonUI.DumpActivatableTree"),
	TEXT("Outputs the current state of the activatable tree. 4 args: bIncludeActions, bIncludeChildren, bIncludeInactive, LocalPlayerId (optional, defaults to -1 or all)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FActionRouterDebugUtils::HandleDebugDumpTree)
);

static const FAutoConsoleCommandWithWorld DumpInputConfigCommand(
	TEXT("CommonUI.DumpInputConfig"),
	TEXT("Outputs the current Input Config for each player"),
	FConsoleCommandWithWorldDelegate::CreateStatic(&FActionRouterDebugUtils::HandleDumpCurrentInputConfig)
);
