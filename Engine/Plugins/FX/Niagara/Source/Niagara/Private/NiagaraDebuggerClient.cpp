// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDebuggerClient.h"

#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "IMessageContext.h"
#include "NiagaraWorldManager.h"
#include "NiagaraComponent.h"
#include "NiagaraDebugHud.h"
#include "Containers/Ticker.h"

#if WITH_NIAGARA_DEBUGGER

DEFINE_LOG_CATEGORY(LogNiagaraDebuggerClient);


FNiagaraDebuggerClient* FNiagaraDebuggerClient::Get()
{
	INiagaraModule& NiagaraModule = FModuleManager::LoadModuleChecked<INiagaraModule>("Niagara");
	return NiagaraModule.GetDebuggerClient();	
}

FNiagaraDebuggerClient::FNiagaraDebuggerClient()
{
	MessageEndpoint = FMessageEndpoint::Builder("FNiagaraDebuggerClient")
		.Handling<FNiagaraDebuggerRequestConnection>(this, &FNiagaraDebuggerClient::HandleConnectionRequestMessage)
		.Handling<FNiagaraDebuggerConnectionClosed>(this, &FNiagaraDebuggerClient::HandleConnectionClosedMessage)
		.Handling<FNiagaraDebuggerExecuteConsoleCommand>(this, &FNiagaraDebuggerClient::HandleExecConsoleCommandMessage)
 		.Handling<FNiagaraDebugHUDSettingsData>(this, &FNiagaraDebuggerClient::HandleDebugHUDSettingsMessage)
		.Handling<FNiagaraRequestSimpleClientInfoMessage>(this, &FNiagaraDebuggerClient::HandleRequestSimpleClientInfoMessage)
		.Handling<FNiagaraOutlinerCaptureSettings>(this, &FNiagaraDebuggerClient::HandleOutlinerSettingsMessage);

 	if (MessageEndpoint.IsValid())
 	{
		MessageEndpoint->Subscribe<FNiagaraDebuggerRequestConnection>();
		MessageEndpoint->Subscribe<FNiagaraDebuggerConnectionClosed>();
 	}

	SessionId = FApp::GetSessionId();
	InstanceId = FApp::GetInstanceId();
	InstanceName = FApp::GetInstanceName();
	UE_LOG(LogNiagaraDebuggerClient, Log, TEXT("Niagara Debugger Client Initialized | Session: %s | Instance: %s (%s)."), *SessionId.ToString(), *InstanceId.ToString(), *InstanceName);

	TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FNiagaraDebuggerClient::Tick));
}

FNiagaraDebuggerClient::~FNiagaraDebuggerClient()
{
	FTicker::GetCoreTicker().RemoveTicker(TickerHandle);

	CloseConnection();
}

bool FNiagaraDebuggerClient::Tick(float DeltaSeconds)
{
	//Keep ticking until we destroy the debugger.
	return true;
}

void FNiagaraDebuggerClient::UpdateClientInfo()
{
	if (MessageEndpoint.IsValid() && Connection.IsValid())
	{
		FNiagaraSimpleClientInfo* NewInfo = new FNiagaraSimpleClientInfo();

		for (TObjectIterator<UNiagaraSystem> It; It; ++It)
		{
			UNiagaraSystem* System = *It;
			if (System)
			{
				NewInfo->Systems.Add(System->GetName());
				for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
				{
					if (Handle.GetInstance())
					{
						NewInfo->Emitters.AddUnique(Handle.GetUniqueInstanceName());
					}					
				}
			}
		}

		TSet<AActor*> Actors;
		for (TObjectIterator<UNiagaraComponent> It; It; ++It)
		{
			UNiagaraComponent* Comp = *It;
			if (Comp)
			{
				NewInfo->Components.AddUnique(Comp->GetName());
				Actors.Add(Comp->GetOwner());
			}
		}

		for (AActor* Actor : Actors)
		{
			if (Actor)
			{
				NewInfo->Actors.Add(Actor->GetName());
			}
		}

		MessageEndpoint->Send(NewInfo, EMessageFlags::Reliable, nullptr, TArrayBuilder<FMessageAddress>().Add(Connection), FTimespan::Zero(), FDateTime::MaxValue());
	}
}

void FNiagaraDebuggerClient::HandleConnectionRequestMessage(const FNiagaraDebuggerRequestConnection& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (MessageEndpoint.IsValid() && Message.SessionId == SessionId && Message.InstanceId == InstanceId)
	{
		if (Connection.IsValid())
		{
			UE_LOG(LogNiagaraDebuggerClient, Warning, TEXT("Connection request recieved but we already have a connected debugger. Current connection being dropped and new connection accepted. | Session: %s | Instance: %s (%s)."), *SessionId.ToString(), *InstanceId.ToString(), *InstanceName);
			CloseConnection();
		}
		else
		{
			UE_LOG(LogNiagaraDebuggerClient, Log, TEXT("Connection request recieved and accepted. | Session: %s | Instance: %s (%s)."), *SessionId.ToString(), *InstanceId.ToString(), *InstanceName);
		}
		
		//Accept the connection and inform the debugger we have done so with an accepted message.
		Connection = Context->GetSender();
		MessageEndpoint->Send(new FNiagaraDebuggerAcceptConnection(SessionId, InstanceId), EMessageFlags::Reliable, nullptr, TArrayBuilder<FMessageAddress>().Add(Connection), FTimespan::Zero(), FDateTime::MaxValue());
		
		//Also send an initial update of the client info.
		UpdateClientInfo();		
	}
}

void FNiagaraDebuggerClient::HandleConnectionClosedMessage(const FNiagaraDebuggerConnectionClosed& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (MessageEndpoint.IsValid() && Message.SessionId == SessionId && Message.InstanceId == InstanceId)
	{
		if (Connection == Context->GetSender())
		{
			UE_LOG(LogNiagaraDebuggerClient, Log, TEXT("Connection Closed. | Session: %s | Instance: %s (%s)."), *SessionId.ToString(), *InstanceId.ToString(), *InstanceName);
			OnConnectionClosed();
		}
		else
		{
			UE_LOG(LogNiagaraDebuggerClient, Warning, TEXT("Recieved connection closed message for unconnected debugger. | Session: %s | Instance: %s (%s)."), *SessionId.ToString(), *InstanceId.ToString(), *InstanceName);
		}
	}
}

void FNiagaraDebuggerClient::HandleExecConsoleCommandMessage(const FNiagaraDebuggerExecuteConsoleCommand& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (ensure(Context->GetSender() == Connection))
	{
		UE_LOG(LogNiagaraDebuggerClient, Log, TEXT("Executing console command. %s | Session: %s | Instance: %s (%s)."), *Message.Command, *SessionId.ToString(), *InstanceId.ToString(), *InstanceName);
		ExecuteConsoleCommand(*Message.Command, Message.bRequiresWorld);
	}
}

void FNiagaraDebuggerClient::HandleDebugHUDSettingsMessage(const FNiagaraDebugHUDSettingsData& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (ensure(Context->GetSender() == Connection))
	{
		UE_LOG(LogNiagaraDebuggerClient, Log, TEXT("Received updated DebugHUD settings. | Session: %s | Instance: %s (%s)."), *SessionId.ToString(), *InstanceId.ToString(), *InstanceName);

		//Pass along the new settings.
		auto ApplySettingsToWorldMan = [&Message](FNiagaraWorldManager& WorldMan)
		{
			WorldMan.GetNiagaraDebugHud()->UpdateSettings(Message);

			//TODO: Move these to just take direct from the debug hud per worldman?
			//Possbly move the debug hud itself to the debugger client rather than having one per world man and they all share global state.
			WorldMan.SetDebugPlaybackMode(Message.PlaybackMode);
			WorldMan.SetDebugPlaybackRate(Message.bPlaybackRateEnabled ? Message.PlaybackRate : 1.0f);
		};

		FNiagaraWorldManager::ForAllWorldManagers(ApplySettingsToWorldMan);
			
		//TODO: Move usage to come direct from settings struct instead of this CVar.
		ExecuteConsoleCommand(*FString::Printf(TEXT("fx.Niagara.Debug.GlobalLoopTime %.3f"), Message.bLoopTimeEnabled && Message.PlaybackMode == ENiagaraDebugPlaybackMode::Loop ? Message.LoopTime : 0.0f), true);
	}
}
void FNiagaraDebuggerClient::HandleRequestSimpleClientInfoMessage(const FNiagaraRequestSimpleClientInfoMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (ensure(Context->GetSender() == Connection))
	{
		UpdateClientInfo();
	}
}

void FNiagaraDebuggerClient::HandleOutlinerSettingsMessage(const FNiagaraOutlinerCaptureSettings& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	if (ensure(Context->GetSender() == Connection))
	{
		FNiagaraOutlinerCaptureSettings::StaticStruct()->CopyScriptStruct(&OutlinerSettings, &Message);
		if(ensure(OutlinerSettings.bTriggerCapture))
		{
			if (OutlinerCountdown == 0)
			{
#if WITH_PARTICLE_PERF_STATS
				if (Message.bGatherPerfData)
				{
					if (StatsListener)
					{
						FParticlePerfStatsManager::RemoveListener(StatsListener);
					}

					StatsListener = MakeShared<FNiagaraOutlinerPerfListener, ESPMode::ThreadSafe>();
					FParticlePerfStatsManager::AddListener(StatsListener);
				}
#endif
				if (Message.CaptureDelayFrames <= 0)
				{
					UE_LOG(LogNiagaraDebuggerClient, Log, TEXT("Recieved request to capture outliner data. Capturing now. | Session: %s | Instance: %s (%s)."), *SessionId.ToString(), *InstanceId.ToString(), *InstanceName);
					UpdateOutliner(0.001f);
				}
				else
				{
					OutlinerCountdown = Message.CaptureDelayFrames;
					UE_LOG(LogNiagaraDebuggerClient, Log, TEXT("Recieved request to capture outliner data. Capturing in %u frames. | Session: %s | Instance: %s (%s)."), Message.CaptureDelayFrames, *SessionId.ToString(), *InstanceId.ToString(), *InstanceName);
					FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FNiagaraDebuggerClient::UpdateOutliner));
				}
			}
			else
			{
				UE_LOG(LogNiagaraDebuggerClient, Log, TEXT("Recieved request to capture outliner data. Ignoring as we already have a pending outliner capture. | Session: %s | Instance: %s (%s)."), *SessionId.ToString(), *InstanceId.ToString(), *InstanceName);
			}
		}
		else
		{
			UE_LOG(LogNiagaraDebuggerClient, Log, TEXT("Recieved request to capture outliner data but the capture bool is false. | Session: %s | Instance: %s."), *SessionId.ToString(), *InstanceId.ToString(), *InstanceName);
		}
	}
}

void FNiagaraDebuggerClient::CloseConnection()
{
	if (MessageEndpoint.IsValid() && Connection.IsValid())
	{
		MessageEndpoint->Send(new FNiagaraDebuggerConnectionClosed(SessionId, InstanceId), EMessageFlags::Reliable, nullptr, TArrayBuilder<FMessageAddress>().Add(Connection), FTimespan::Zero(), FDateTime::MaxValue());
	}

	OnConnectionClosed();
}

void FNiagaraDebuggerClient::OnConnectionClosed()
{
	Connection.Invalidate();
}

void FNiagaraDebuggerClient::ExecuteConsoleCommand(const TCHAR* Cmd, bool bRequiresWorld)
{
	if (bRequiresWorld)
	{
		for (TObjectIterator<UWorld> WorldIt; WorldIt; ++WorldIt)
		{
			UWorld* World = *WorldIt;
			if ((World != nullptr) &&
				(World->PersistentLevel != nullptr) &&
				(World->PersistentLevel->OwningWorld == World) &&
				((World->GetNetMode() == ENetMode::NM_Client) || (World->GetNetMode() == ENetMode::NM_Standalone)))
			{
				GEngine->Exec(*WorldIt, Cmd);
			}
		}
	}
	else
	{
		GEngine->Exec(nullptr, Cmd);
	}
}

static const FName NiagaraDebuggerClientOutlienrUpdateMessage("NiagaraDebuggerClientOutlienrUpdateMessage");
bool FNiagaraDebuggerClient::UpdateOutliner(float DeltaSeconds)
{
	if (OutlinerCountdown > 0)
	{
		--OutlinerCountdown;
		FString HudMessage = FString::Printf(TEXT("Capturing Niagara Outliner in %u frames..."), OutlinerCountdown);
		FNiagaraWorldManager::ForAllWorldManagers([&HudMessage](FNiagaraWorldManager& WorldMan){ WorldMan.GetNiagaraDebugHud()->AddMessage(NiagaraDebuggerClientOutlienrUpdateMessage, FNiagaraDebugMessage(ENiagaraDebugMessageType::Info, HudMessage, 1.0f)); });
		return true;
	}
	
	FString HudMessage = FString::Printf(TEXT("Captured Niagara Outliner Info."));
	FNiagaraWorldManager::ForAllWorldManagers([&HudMessage](FNiagaraWorldManager& WorldMan) { WorldMan.GetNiagaraDebugHud()->AddMessage(NiagaraDebuggerClientOutlienrUpdateMessage, FNiagaraDebugMessage(ENiagaraDebugMessageType::Info, HudMessage, 3.0f)); });

	//Ensure any RT writes to perf or state info are complete.
	FlushRenderingCommands();

	OutlinerCountdown = 0;
	if(ensure(Connection.IsValid()))
	{
		FNiagaraDebuggerOutlinerUpdate* Message = new FNiagaraDebuggerOutlinerUpdate();
	
		//Gather all high level state data to pass to the outliner in the debugger.
		//TODO: Move out to somewhere neater and add more info.
		for (TObjectIterator<UNiagaraComponent> CompIt; CompIt; ++CompIt)
		{
			UNiagaraComponent* Comp = *CompIt;
			if (Comp)
			{
				UWorld* World = Comp->GetWorld();
				FNiagaraOutlinerWorldData& WorldData = Message->OutlinerData.WorldData.FindOrAdd( World ? World->GetPathName() : TEXT("Null World") );
				if (World)
				{
					WorldData.bHasBegunPlay = World->HasBegunPlay();
					WorldData.WorldType = World->WorldType;
					WorldData.NetMode = World->GetNetMode();

					#if WITH_PARTICLE_PERF_STATS
					if(StatsListener)
					{
						if (FAccumulatedParticlePerfStats* WorldStats = StatsListener->GetStats(World))
						{
							WorldData.AveragePerFrameTime.GameThread = WorldStats->GetGameThreadStats().GetPerFrameAvg();
							WorldData.AveragePerFrameTime.RenderThread = WorldStats->GetRenderThreadStats().GetPerFrameAvg();

							WorldData.MaxPerFrameTime.GameThread = WorldStats->GetGameThreadStats().GetPerFrameMax();
							WorldData.MaxPerFrameTime.RenderThread = WorldStats->GetRenderThreadStats().GetPerFrameMax();
						}
					}
					#endif
				}

				UNiagaraSystem* System = Comp->GetAsset();
				FNiagaraOutlinerSystemData& Instances = WorldData.Systems.FindOrAdd(System ? System->GetPathName() : TEXT("Null System"));
				if (System)
				{
					//Add System specific data.
					#if WITH_PARTICLE_PERF_STATS
					if(StatsListener)
					{
						if (FAccumulatedParticlePerfStats* SystemStats = StatsListener->GetStats(System))
						{
							Instances.AveragePerFrameTime.GameThread = SystemStats->GetGameThreadStats().GetPerFrameAvg();
							Instances.AveragePerFrameTime.RenderThread = SystemStats->GetRenderThreadStats().GetPerFrameAvg();

							Instances.MaxPerFrameTime.GameThread = SystemStats->GetGameThreadStats().GetPerFrameMax();
							Instances.MaxPerFrameTime.RenderThread = SystemStats->GetRenderThreadStats().GetPerFrameMax();


							Instances.AveragePerInstanceTime.GameThread = SystemStats->GetGameThreadStats().GetPerInstanceAvg();
							Instances.AveragePerInstanceTime.RenderThread = SystemStats->GetRenderThreadStats().GetPerInstanceAvg();

							Instances.MaxPerInstanceTime.GameThread = SystemStats->GetGameThreadStats().GetPerInstanceMax();
							Instances.MaxPerInstanceTime.RenderThread = SystemStats->GetRenderThreadStats().GetPerInstanceMax();
						}
					}
					#endif
				}

				FNiagaraOutlinerSystemInstanceData& InstData = Instances.SystemInstances.AddDefaulted_GetRef();
				InstData.ComponentName = Comp->GetPathName();

				if (FNiagaraSystemInstance* Inst = Comp->GetSystemInstance())
				{
					InstData.ActualExecutionState = Inst->GetActualExecutionState();
					InstData.RequestedExecutionState = Inst->GetRequestedExecutionState();

					InstData.ScalabilityState = Comp->DebugCachedScalabilityState;
					InstData.bPendingKill = Comp->IsPendingKillOrUnreachable();

					InstData.PoolMethod = Comp->PoolingMethod;

					InstData.Emitters.Reserve(Inst->GetEmitters().Num());
					for (TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe>& EmitterInst : Inst->GetEmitters())
					{
						FNiagaraOutlinerEmitterInstanceData& EmitterData = InstData.Emitters.AddDefaulted_GetRef();
						if (EmitterInst->GetCachedEmitter())
						{
							//TODO: This is a bit wasteful to copy the name into each instance data. Though we can't rely on the debugger side data matchin the actul running data on the device.
							//We need to build a shared representation of the asset data from the client that we then reference from this per instance data.
							EmitterData.EmitterName = EmitterInst->GetCachedEmitter()->GetUniqueEmitterName();
							EmitterData.SimTarget = EmitterInst->GetCachedEmitter()->SimTarget;
							//Move all above to a shared asset representation.

							EmitterData.ExecState = EmitterInst->GetExecutionState();
							EmitterData.NumParticles = EmitterInst->GetNumParticles();
						}
					}
				}
				else
				{
					InstData.ActualExecutionState = ENiagaraExecutionState::Num;
					InstData.RequestedExecutionState = ENiagaraExecutionState::Num;
				}

#if WITH_PARTICLE_PERF_STATS
				if(StatsListener)
				{
					if (FAccumulatedParticlePerfStats* ComponentStats = StatsListener->GetStats(Comp))
					{
						InstData.AverageTime.GameThread = ComponentStats->GetGameThreadStats().GetPerFrameAvg();
						InstData.AverageTime.RenderThread = ComponentStats->GetRenderThreadStats().GetPerFrameAvg();

						InstData.MaxTime.GameThread = ComponentStats->GetGameThreadStats().GetPerFrameMax();
						InstData.MaxTime.RenderThread = ComponentStats->GetRenderThreadStats().GetPerFrameMax();
					}
				}
#endif
			}
		}

		//TODO: Add any component less systems too if and when they are a thing.
		//TODO: Gather some info for unloaded or currently unused systems.

	
		//Send the updated data to the debugger;
		MessageEndpoint->Send(Message, EMessageFlags::Reliable, nullptr, TArrayBuilder<FMessageAddress>().Add(Connection), FTimespan::Zero(), FDateTime::MaxValue());
	}

#if WITH_PARTICLE_PERF_STATS
	if (StatsListener)
	{
		FParticlePerfStatsManager::RemoveListener(StatsListener);
		StatsListener = nullptr;
	}
#endif

	//Clear up the timer now that we've sent the capture.
	//TODO: continuous/repeated capture mode?
	return false;
}

#endif//WITH_NIAGARA_DEBUGGER