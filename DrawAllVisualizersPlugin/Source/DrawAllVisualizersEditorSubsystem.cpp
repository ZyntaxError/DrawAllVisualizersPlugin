// Copyright (c) Zyni https://github.com/ZyntaxError
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "DrawAllVisualizersEditorSubsystem.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "Selection.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Kismet2/DebuggerCommands.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY(LogDrawAllVisualizers)

TAutoConsoleVariable<bool> CVarDrawAllVisualizersEnabled(
	TEXT("DrawAllVisualizers.Enabled"), false,
	TEXT("Draw all component visualizers?"),
	ECVF_Default);

TAutoConsoleVariable<bool> CVarDrawAllVisualizersNoCache(
	TEXT("DrawAllVisualizers.NoCache"), false,
	TEXT("Skip cache? Try this if cached mode is not working for you for some reason"),
	ECVF_Default);

bool UDrawAllVisualizersEditorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsRunningCommandlet()) return false;

	return Super::ShouldCreateSubsystem(Outer);
}

void UDrawAllVisualizersEditorSubsystem::Deinitialize()
{
	UE_LOGFMT(LogDrawAllVisualizers, Verbose, "Subsystem Deinitialize");
	Super::Deinitialize();

	FEditorModeRegistry::Get().UnregisterMode(DrawAllVisualizers::FDrawAllVisualizersEdMode::EM_DrawAllVisualizers);
	DrawAllVisualizers::FDrawAllVisualizersCommands::Unregister();
}

void UDrawAllVisualizersEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOGFMT(LogDrawAllVisualizers, Verbose, "Subsystem Initialize");
	Super::Initialize(Collection);

	FEditorModeRegistry::Get().RegisterMode<DrawAllVisualizers::FDrawAllVisualizersEdMode>(
		DrawAllVisualizers::FDrawAllVisualizersEdMode::EM_DrawAllVisualizers,
		FText::FromString(TEXT("DrawAllVisualizers")),
		FSlateIcon(),
		false);

	GLevelEditorModeTools().ActivateMode(DrawAllVisualizers::FDrawAllVisualizersEdMode::EM_DrawAllVisualizers);

	DrawAllVisualizers::FDrawAllVisualizersCommands::Register();

	FPlayWorldCommands::GlobalPlayWorldActions->MapAction(
		DrawAllVisualizers::FDrawAllVisualizersCommands::Get().ToggleDrawAllVisualizersEnabledCommand,
		FExecuteAction::CreateLambda([]()
		{
			bool bNewEnabled = !CVarDrawAllVisualizersEnabled.GetValueOnGameThread();
			UE_LOGFMT(LogDrawAllVisualizers, Display, "Toggle Draw All Visualizers enabled {0}", bNewEnabled);

			CVarDrawAllVisualizersEnabled->SetWithCurrentPriority(bNewEnabled);
		}),
		FCanExecuteAction()
		);


}

void UDrawAllVisualizersSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	UE_LOGFMT(LogDrawAllVisualizers, Verbose, "Subsystem Initialize {0} IsTemplate {1}", bEnabled, IsTemplate());
	if (IsTemplate())
	{
		// If the console variables are saved then this should be the easier way.
		// ImportConsoleVariableValues();

		CVarDrawAllVisualizersEnabled->Set(bEnabled, ECVF_SetByProjectSetting);
		CVarDrawAllVisualizersNoCache->Set(bNoCache, ECVF_SetByProjectSetting);
	}
#endif
}

FName UDrawAllVisualizersSettings::GetCategoryName() const
{
	return FName(TEXT("Editor"));
}

#if WITH_EDITOR
void UDrawAllVisualizersSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
	}
}
#endif

namespace DrawAllVisualizers
{
FDrawAllVisualizersCommands::FDrawAllVisualizersCommands():
	TCommands<FDrawAllVisualizersCommands>(TEXT("DrawAllVisualizers"),
	                                       NSLOCTEXT("Contexts", "DrawAllVisualizers", "DrawAllVisualizers"),
	                                       NAME_None, FAppStyle::GetAppStyleSetName())
{
}

void FDrawAllVisualizersCommands::RegisterCommands()
{
#define LOCTEXT_NAMESPACE "DrawAllVisualizers"
	UI_COMMAND(ToggleDrawAllVisualizersEnabledCommand,
	           "Toggle Draw All Visualizers", "Toggles whether unselected Component Visualizers are drawn",
	           EUserInterfaceActionType::None,
	           FInputChord());
#undef LOCTEXT_NAMESPACE
}

template <typename Func>
void ForeachActorComponentVisualizer(Func F)
{
	// Could perhaps just use TObjectIterator<UActorComponent>, but this deeper level route is better for learning.

	bool IsPlaying = GEditor->IsPlayingSessionInEditor();
	TInlineComponentArray<UActorComponent*> Components;

	const auto& Worlds = GEditor->GetWorldContexts();
	for (const auto& WorldContext : Worlds)
	{
		const UWorld* World = WorldContext.World();
		if (World == nullptr || World->WorldType == EWorldType::EditorPreview) continue;

		if (IsPlaying)
		{
			if (World->WorldType != EWorldType::Game & World->WorldType != EWorldType::PIE) continue;
		}

		for (const ULevel* Level : World->GetLevels())
		{
			if (!IsValid(Level)) continue;

			for (auto Actor : Level->Actors)
			{
				if (!IsValid(Actor)) continue;

				Components.Reset();
				Actor->GetComponents(Components, false);

				for (auto Component : Components)
				{
					// Editor does this. This does not.
					// if (!Comp->IsRegistered()) continue;

					TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(Component->GetClass());
					if (!Visualizer.IsValid()) continue;

					F(Actor.Get(), Component, Visualizer);
				}
			}
		}
	}
}

const FEditorModeID FDrawAllVisualizersEdMode::EM_DrawAllVisualizers("EM_DrawAllVisualizers");

FDrawAllVisualizersEdMode::FDrawAllVisualizersEdMode()
{
	UE_LOGFMT(LogDrawAllVisualizers, Verbose, "FDrawAllVisualizersEdMode()");
}

FDrawAllVisualizersEdMode::~FDrawAllVisualizersEdMode()
{
	UE_LOGFMT(LogDrawAllVisualizers, Verbose, "~FDrawAllVisualizersEdMode()");

	USelection::SelectionChangedEvent.RemoveAll(this);
	FEditorDelegates::PostPIEStarted.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);

	if (OnObjectConstructedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectConstructed.Remove(OnObjectConstructedHandle);
		OnObjectConstructedHandle.Reset();
	}
}

void FDrawAllVisualizersEdMode::Initialize()
{
	UE_LOGFMT(LogDrawAllVisualizers, Verbose, "FDrawAllVisualizersEdMode Initialize");
	FEdMode::Initialize();

	USelection::SelectionChangedEvent.AddSP(this, &FDrawAllVisualizersEdMode::OnSelectionChanged);
	FEditorDelegates::PostPIEStarted.AddSP(this, &FDrawAllVisualizersEdMode::OnPieStartOrEnd);
	FEditorDelegates::EndPIE.AddSP(this, &FDrawAllVisualizersEdMode::OnPieStartOrEnd);

	// Not hooking into FCoreUObjectDelegates::OnObjectConstructed yet. It's kinda high frequency event so don't use it until needed.
}

void FDrawAllVisualizersEdMode::Enter()
{
	// FEdMode::Enter();	// Don't need this.
	UE_LOGFMT(LogDrawAllVisualizers, Verbose, "FDrawAllVisualizersEdMode Enter");
	bNeedActivateEdMode = false;
}

void FDrawAllVisualizersEdMode::Exit()
{
	// FEdMode::Exit();		// Don't need this.

	// Register mode again on next Tick when it gets disabled.
	// Main causes of this is FEditorModeTools::DeactivateAllModes(), which happens from map loads.

	// Previous way was to hook into OnMapOpen and do this:
	// if (ChangeType == EMapChangeType::TearDownWorld) bNeedActivateEdMode = true;
	// else if(bNeedActivateEdMode) GLevelEditorModeTools().ActivateMode(EM_DrawAllVisualizers);
	//
	// Previous way however had few issues:
	//	- Must hook into FLevelEditorModule::OnLevelEditorCreated() to delay activating the mode because
	//		on the first Tick FEditorModeTools::ExitAllModesPendingDeactivate() is called. Didn't check what puts it to pending deactivate list.
	//		- ExitAllModesPendingDeactivate also does "check(PendingDeactivateModes.Num() == 0);" so can't really work around it.
	//	- Would not survive any calls to DeactivateAllModes().

	bool bExitRequested = IsEngineExitRequested();
	UE_LOGFMT(LogDrawAllVisualizers, Verbose, "FDrawAllVisualizersEdMode Exit IsEngineExitRequested {0}", bExitRequested);
	if (!bExitRequested)
	{
		GEditor->GetTimerManager()->SetTimerForNextTick([]
		{
			GLevelEditorModeTools().ActivateMode(EM_DrawAllVisualizers);
		});
	}
}

FString FDrawAllVisualizersEdMode::GetReferencerName() const
{
	return EM_DrawAllVisualizers.ToString();
}

bool FDrawAllVisualizersEdMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	// Compatible with everything as want to be active all of the time.
	return true;
}

void FDrawAllVisualizersEdMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	// FEdMode::Render(View, Viewport, PDI);	// Don't need this.

	const bool bEnabledNew = CVarDrawAllVisualizersEnabled.GetValueOnGameThread();
	if (bEnabledNew != bEnabled)
	{
		bEnabled = bEnabledNew;
		if (!bEnabledNew)
		{
			CachedVisualizers.Empty();
			if (OnObjectConstructedHandle.IsValid())
			{
				FCoreUObjectDelegates::OnObjectConstructed.Remove(OnObjectConstructedHandle);
				OnObjectConstructedHandle.Reset();
			}
		}
		bNeedRebuildCachedVisualizers = true;
		GEditor->RedrawAllViewports(false);
	}

	if (!bEnabledNew) return;

	const UDrawAllVisualizersSettings* Settings = GetDefault<UDrawAllVisualizersSettings>();

	bNoCache = CVarDrawAllVisualizersNoCache.GetValueOnGameThread();
	if (bNoCache)
	{
		CachedVisualizers.Empty();
		bNeedRebuildCachedVisualizers = true;
		if (bNeedRebuildSelectedActors) RebuildSelectedActors();

		ForeachActorComponentVisualizer([&](AActor* Actor, const UActorComponent* Component, const TSharedPtr<FComponentVisualizer>& Visualizer)
		{
			if (SelectedActors.Contains(Actor)) return;
			if (Settings->IgnoredVisualizers.Contains(Component->GetClass()->GetFName())) return;
			Visualizer->DrawVisualization(Component, View, PDI);
		});
		return;
	}

	if (bNeedRebuildCachedVisualizers) RebuildCachedVisualizers();

	if (bNeedRebuildSelectedActors) RebuildSelectedActors();

	// Editor does check like this. This does not.
	// if (GCurrentLevelEditingViewportClient != nullptr && GCurrentLevelEditingViewportClient->IsInGameView()) return;

	// Collect stale entries and remove later. Don't know and don't care if current container allows removal during iteration.
	// Safer for possible container changes in the future this way.
	TInlineComponentArray<TWeakObjectPtr<UActorComponent>> StaleEntries;

	for (const auto& Tuple : CachedVisualizers)
	{
		const FCachedVisualizer& CachedVisualizer = Tuple.Value;
		const UActorComponent* Component = Tuple.Key.Get();
		if (Component == nullptr)
		{
			// Also happens when modifying the actor or components.
			// Like when moving with the transform gizmo or editing values from details panel.
			// New component created this way is caught by OnObjectConstructed.

			StaleEntries.Add(Tuple.Key);
			continue;
		}

		if (CachedVisualizer.IsSelected) continue;

		UWorld* World = Component->GetWorld();
		if (World == nullptr || World->WorldType == EWorldType::EditorPreview) continue;

		// Editor does this. This does not.
		// if (!Component->IsRegistered()) continue;

		CachedVisualizer.Visualizer->DrawVisualization(Component, View, PDI);
	}

	for (auto Elem : StaleEntries)
	{
		CachedVisualizers.Remove(Elem);
	}

	if (Settings->bDisplayVisualizerTypeCountsOnScreen)
	{
		DrawOnScreenDebugs();
	}
}

void FDrawAllVisualizersEdMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	// FEdMode::DrawHUD(ViewportClient, Viewport, View, Canvas);	// Don't need this.

	if (!bEnabled) return;

	if (bNoCache)
	{
		const UDrawAllVisualizersSettings* Settings = GetDefault<UDrawAllVisualizersSettings>();

		ForeachActorComponentVisualizer([&](AActor* Actor, const UActorComponent* Component, const TSharedPtr<FComponentVisualizer>& Visualizer)
		{
			if (SelectedActors.Contains(Actor)) return;
			if (Settings->IgnoredVisualizers.Contains(Component->GetClass()->GetFName())) return;
			Visualizer->DrawVisualizationHUD(Component, Viewport, View, Canvas);
		});
		return;
	}

	for (const auto& Tuple : CachedVisualizers)
	{
		const FCachedVisualizer& CachedVisualizer = Tuple.Value;
		const UActorComponent* Component = Tuple.Key.Get();
		if (Component == nullptr) continue;

		if (CachedVisualizer.IsSelected) continue;

		UWorld* World = Component->GetWorld();
		if (World == nullptr || World->WorldType == EWorldType::EditorPreview) continue;

		CachedVisualizer.Visualizer->DrawVisualizationHUD(Component, Viewport, View, Canvas);
	}
}

void FDrawAllVisualizersEdMode::OnSelectionChanged(UObject* Obj)
{
	bNeedRebuildSelectedActors = true;
}

void FDrawAllVisualizersEdMode::OnPieStartOrEnd(bool bIsSimulating)
{
	UE_LOGFMT(LogDrawAllVisualizers, Verbose, "OnPieStartOrEnd simulating {0}", bIsSimulating);
	bNeedRebuildCachedVisualizers = true;
	bNeedRebuildSelectedActors = true;
}

void FDrawAllVisualizersEdMode::OnObjectConstructed(UObject* Obj)
{
	if (!bEnabled | bNeedRebuildCachedVisualizers) return;

	UActorComponent* Component = Cast<UActorComponent>(Obj);
	if (Component == nullptr) return;

	UWorld* World = Component->GetWorld();
	UE_LOG(LogDrawAllVisualizers, VeryVerbose, TEXT("OnObjectConstructed %s world ptr %p"), *Component->GetPathName(), World);

	// Have to choose between storing TWeakObjectPtr or immediately check for component visualizer from the map.
	// TWeakObjectPtr would allow optimization opportunities later if multiple components are created per frame.
	// On the other hand 99% of components might not have visualizer so would be creating weak objects for nothing.

	// Some objects have null world for unknown reasons while loading map. So can't check it yet.
	// Almost everything is also constructed twice :shrug:, few are not and those are ignored if world is checked at this stage.

	// Could have small cache of last used class types that can be checked in semi branchless fashion before going all the way to FindComponentVisualizer.

	TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(Component->GetClass());
	if (!Visualizer.IsValid()) return;
	if (GetDefault<UDrawAllVisualizersSettings>()->IgnoredVisualizers.Contains(Component->GetClass()->GetFName())) return;

	UE_LOGFMT(LogDrawAllVisualizers, VeryVerbose, "Add visualizer {0}", Component->GetPathName());

	// It could be selected, but need to ignore it.
	// Adding component to selected actor will not be drawn by the built in visualizer drawing system until selection is updated.
	// Want to be better than that and draw it immediately.
	CachedVisualizers.Add(Component, {Visualizer, false});
}

void FDrawAllVisualizersEdMode::RebuildCachedVisualizers()
{
	bNeedRebuildCachedVisualizers = false;
	CachedVisualizers.Reset();
	const UDrawAllVisualizersSettings* Settings = GetDefault<UDrawAllVisualizersSettings>();

	ForeachActorComponentVisualizer([&](AActor* Actor, UActorComponent* Component, const TSharedPtr<FComponentVisualizer>& Visualizer)
	{
		if (Settings->IgnoredVisualizers.Contains(Component->GetClass()->GetFName())) return;
		UE_LOGFMT(LogDrawAllVisualizers, VeryVerbose, "Add visualizer {0} registered {1}", Component->GetPathName(), Component->IsRegistered());

		CachedVisualizers.Add(Component, {Visualizer, false});
	});

	if (!OnObjectConstructedHandle.IsValid())
	{
		OnObjectConstructedHandle = FCoreUObjectDelegates::OnObjectConstructed.AddSP(
			this, &FDrawAllVisualizersEdMode::OnObjectConstructed);
	}

	UE_LOGFMT(LogDrawAllVisualizers, Verbose, "visualizers found {0}", CachedVisualizers.Num());
}

void FDrawAllVisualizersEdMode::RebuildSelectedActors()
{
	bNeedRebuildSelectedActors = false;

	SelectedActors.Reset();
	for (FSelectionIterator It = GEditor->GetSelectedActorIterator(); It; ++It)
	{
		SelectedActors.Add(static_cast<AActor*>(*It));
	}

	int NumVisualizersSelected = 0;
	for (auto& Element : CachedVisualizers)
	{
		UActorComponent* Component = Element.Key.Get();
		if (Component == nullptr)
		{
			Element.Value.IsSelected = false;
			continue;
		}

		AActor* Outer = Cast<AActor>(Component->GetOuter());
		Element.Value.IsSelected = SelectedActors.Contains(Outer);
		NumVisualizersSelected += Element.Value.IsSelected;
	}

	UE_LOGFMT(LogDrawAllVisualizers, Verbose, "SelectedActors {0} visualizers affected {1}/{2}",
	          SelectedActors.Num(), NumVisualizersSelected, CachedVisualizers.Num());
}

void FDrawAllVisualizersEdMode::DrawOnScreenDebugs()
{
	TMap<FName, int> VisualizerCounts;

	for (auto& Tuple : CachedVisualizers)
	{
		UActorComponent* Component = Tuple.Key.Get();
		UWorld* World = Component->GetWorld();
		if (World == nullptr || World->WorldType == EWorldType::EditorPreview) continue;

		VisualizerCounts.FindOrAdd(Component->GetClass()->GetFName()) += 1;
	}

	VisualizerCounts.ValueStableSort([](int v1, int v2) { return v1 > v2; });

	TStringBuilder<200> Builder;
	Builder << "Visualized component types:\n";
	for (auto& Tuple : VisualizerCounts)
	{
		Builder << "    " << Tuple.Key << " " << Tuple.Value << '\n';
	}

	GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 1, FColor::Red, Builder.ToString());
}
}
