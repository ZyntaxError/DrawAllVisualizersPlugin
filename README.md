# DrawAllVisualizers Plugin

Editor Subsystem for Unreal Engine that will register always on EdMode that draws all Component Visualizers, including those that are not selected.

Tested only on UE 5.3 and don't care about older versions.

## Installation
Drop this into Plugins folder that is located in your project root folder. Might need to create it.

## Usage
* Keyboard shortcut `Toggle Draw All Visualizers`.
* Cvars `DrawAllVisualizers.Enabled` and `DrawAllVisualizers.NoCache`.
* `Draw All Visualizers` section in Project Settings.

## Logging
By default only the UI Command(keyboard shortcut) for toggling enabled state is logged.
For extra logging:
```
DefaultEngine.ini
[Core.Log]
LogDrawAllVisualizers=verbose
```
Or console command `Log LogDrawAllVisualizers verbose`.
<br/>VeryVerbose is also used, but it can be ultra spammy.

## Known issues
* Components hidden by world partition are still drawn.
* Unselected spline components can be edited, but only when something(not necessarily the spline) is selected.

## Possible future work
* Draw also to game view when not detached? Could not easily figure out how to do this.
* Don't activate EdMode until it's needed? Could speedup the editor startup by around 350 microseconds, but might complicate things.

## Remarks
* DeveloperSettings cannot override values if you do `DefaultEngine.ini [ConsoleVariables] DrawAllVisualizers.Enabled=True`.

Another workable alternative to EdMode is to hook into
```cpp
FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor").GetLevelEditorInstance().Pin()->GetEditorModeManager().GetInteractiveToolsContext()->OnRender
```
One minor issue with that choice is that it does not have possibly completely unused FViewport parameter for `DrawHUD()`.
There is `IToolsContextRenderAPI` extension for it that derives some values from FViewport, but it's hidden inside
`UE_5.3/Engine/Source/Editor/Experimental/EditorInteractiveToolsFramework/Private/EdModeInteractiveToolsContext.cpp`.
