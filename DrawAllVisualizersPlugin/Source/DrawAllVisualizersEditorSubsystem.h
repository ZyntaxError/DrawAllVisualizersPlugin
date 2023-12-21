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

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "EdMode.h"
#include "DrawAllVisualizersEditorSubsystem.generated.h"

class FComponentVisualizer;
class FUICommandInfo;

DECLARE_LOG_CATEGORY_EXTERN(LogDrawAllVisualizers, Log, All)

UCLASS()
class UDrawAllVisualizersEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

protected:
	virtual void Deinitialize() override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
};

UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Draw All Visualizers"))
class UDrawAllVisualizersSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, meta = (
		ConsoleVariable = "DrawAllVisualizers.Enabled", DisplayName = "Enabled",
		ToolTip = "Draw all component visualizers?",
		ConfigRestartRequired = false))
	bool bEnabled;

	UPROPERTY(config, EditAnywhere, meta = (
		ConsoleVariable = "DrawAllVisualizers.NoCache", DisplayName = "NoCache",
		ToolTip = "Skip cache? Try this if cached mode is not working for you for some reason",
		ConfigRestartRequired = false))
	bool bNoCache;
	
	UPROPERTY(EditAnywhere)
	bool bDisplayVisualizerTypeCountsOnScreen;

	UPROPERTY(config, EditAnywhere, meta = (ToolTip = "Don't draw these FComponentVisualizers"))
	TSet<FName> IgnoredVisualizers;
	
	virtual void PostInitProperties() override;
	virtual FName GetCategoryName() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

namespace DrawAllVisualizers
{
class FDrawAllVisualizersCommands : public TCommands<FDrawAllVisualizersCommands>
{
public:
	FDrawAllVisualizersCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> ToggleDrawAllVisualizersEnabledCommand;
};

struct FCachedVisualizer
{
	TSharedPtr<FComponentVisualizer> Visualizer;

	// Visualizers for selected actors are skipped. Let default drawing system handle those.
	bool IsSelected = false;
};

class FDrawAllVisualizersEdMode : public FEdMode
{
public:
	static const FEditorModeID EM_DrawAllVisualizers;

	FDrawAllVisualizersEdMode();
	virtual ~FDrawAllVisualizersEdMode() override;

	virtual void Initialize() override;
	virtual void Enter() override;
	virtual void Exit() override;
	virtual FString GetReferencerName() const override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;

protected:
	void OnSelectionChanged(UObject* Obj);
	void OnPieStartOrEnd(bool bIsSimulating);
	void OnObjectConstructed(UObject* Obj);

	void RebuildCachedVisualizers();
	void RebuildSelectedActors();

	void DrawOnScreenDebugs();

	FDelegateHandle OnObjectConstructedHandle;
	bool bEnabled = false;
	bool bNoCache = false;
	bool bNeedRebuildCachedVisualizers = true;
	bool bNeedActivateEdMode = false;
	bool bNeedRebuildSelectedActors = true;

	// Might not be the ideal container type, but it's good enough, simple to use and convenient. 95% of cost comes from DrawVisualization() anyways.
	TMap<TWeakObjectPtr<UActorComponent>, FCachedVisualizer> CachedVisualizers;

	// Actor->IsSelectedInEditor() is insanely expensive.
	// GEditor->GetSelectedActors()->IsSelected(Actor) is one less virtual call and few checks less, but still too much.
	// Didn't profile GEditor->GetSelectedActorIterator(), but it looks less than ideal. It's used to gather values to this.
	TInlineComponentArray<AActor*> SelectedActors;
};
}
