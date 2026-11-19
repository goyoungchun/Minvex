# Minvex API

**C++20 Graphics SDK · Version 1.0.1 · Namespace `lv`**

모듈별 주요 타입과 API를 정리한 사용 안내임. 전체 선언은 연결된 공개 헤더에서 확인할 수 있음.

설치·실행 요구사항과 기능 개요는 [README](../README.md)를 참고함. 이 문서는 공개 API의 동작·한도·소유권·오류·호환성을 설명함.

## SDK 연결

```cmake
find_package(Minvex 1.0 CONFIG REQUIRED COMPONENTS viewer graphics)
add_executable(my_viewer main.cpp)
target_link_libraries(my_viewer PRIVATE minvex::viewer)
```

필요한 모듈을 `minvex::<모듈명>`으로 연결함. CPU 도구는 `assets`·`animation`·`scene_io`, Host 통합은 `embedded`, 편집 상태 관리는 `editor_document` 등을 사용함. `graphics`는 그래픽 SDK 여부를 검사하는 component임.

공개 헤더 경로는 `<minvex/...>`, CMake 옵션 접두사는 `MINVEX_`, SDK 데이터 경로는 `share/minvex`임. 이전 이름의 패키지·타깃·헤더 별칭은 제공하지 않으므로 SDK 소비자는 참조를 갱신하고 다시 빌드해야 함. 문서 형식은 새 저장 시 Minvex 식별자를 쓰며 이름 변경 이전의 Scene·Pose·Quality·Editor 설정 식별자는 읽기 호환을 유지함. C++ 네임스페이스는 기존 `lv`를 사용함.

## 모듈별 주요 API

| 모듈 / 헤더 | 주요 타입·함수 |
|---|---|
| **ui** · [Ui.h](../include/minvex/Ui.h) | `UiCanvas`, `UiElement`, `UiLayout/UiContainer/UiLayoutBox`, `UiStyle`, `UiTextStyle/UiTextRun`, `UiHistoryState`, `UiInput`, `UiFont`. `Add/Set/Get`, `ResolveLayout/SetScroll`, `BeginEdit/CommitEdit/CancelEdit`, `Undo/Redo/History`, `ProcessInput/PollActivations`, `Fonts`, `SetFontDirectory`, `Save/Load`, `Rasterize` |
| **core** · [Math.h](../include/minvex/Math.h), [Mesh.h](../include/minvex/Mesh.h) | `Vec3`, `Quaternion`, `Mat4`, `Transform`, `Bounds`, `MeshData`, `MakeCube()`, `DecomposeTRS()`. 생성 헤더 `Version.h`의 `Version`, `BuildInfo`, `GetBuildInfo()` |
| **assets** · [Asset.h](../include/minvex/Asset.h), [AssetLoading.h](../include/minvex/AssetLoading.h) | `ModelHandle`, `MorphTarget`, `MaterialAsset`, `ShadingModel`, `ImportSettings`, `ImporterRegistry`, `AssetRegistry::LoadModel/PrepareModel/ReimportModel`, `InspectModel()`, `ValidateModel()`, `CopyModelToProject()`, `AssetLoader`, `AssetHotReload` |
| **scene** · [Scene.h](../include/minvex/Scene.h) | `Scene`, `ObjectId`, `Camera`, `AmbientLight`, `DirectionalLight`, `PointLight`, `SpotLight`, `RectLight`, `LocalShadowSettings`. `AddModel()`, `CreateObject()`, `SetParent()`, `SetTransform()`, `SetModel()`, `GetMaterial/SetMaterial/ResetMaterial`, `GetAnimation()`, `UpdateAnimations()`, `SceneAnimationEvent`, `PollAnimationEvents/DispatchAnimationEvents()` |
| **animation** · [Animation.h](../include/minvex/Animation.h) | `SkeletonAsset`, `SkinAsset`, `PoseAsset`, `PoseBuffer`, `PoseChannels`, `AdditivePose/AdditiveAnimationState`, `AnimationClip`, `AnimationEvent/AnimationEventTrack/AnimationEventOccurrence`, `AnimationBlendState`, `BlendTreeDesc/BlendTreeAsset/BlendTreeState`, `MorphTrack`, `MorphPose`, `AnimationPlayer`. `EvaluateAnimation()`, `BlendPoses()`, `ComposePose()`, `CreateBlendTree()`, `EvaluateBlendWeights/EvaluateBlendTree()`, `Play/PlayBlendTree()`, `CrossFade()`, `SetBlendParameter()`, `Seek/SeekBlendTree()`, `EvaluatePose/Evaluate()`, `SetAdditiveAnimation()`, `MakeAdditivePose/ApplyAdditivePose()`, `CapturePose()` |
| **scene_io** · [SceneIO.h](../include/minvex/SceneIO.h), [PoseIO.h](../include/minvex/PoseIO.h) | `SceneArchive`, `SaveScene(scene, path, library={})`, `LoadScene(path, registry)`, `SavePose(pose, skeleton, path)`, `LoadPose(path, skeleton)` |
| **viewer** · [Viewer.h](../include/minvex/Viewer.h) | `ViewerOptions`, `ModelOptions`, `ViewerRunOptions`, `Viewer`. `AddModel()`, `AddModelAsync()`, `GetAssetLoad()`, `CommitModelLoad()`, `SetHotReload()`, `SetCamera()`, `SetMaterial/ResetMaterial`, `SetQuality()`, `CrossFadeAnimation()`, `PlayBlendTree/SetBlendParameter/SeekBlendTree`, `SetAdditiveAnimation/SetAdditiveWeight/SetAdditiveMask/SeekAdditive/ClearAdditiveAnimation`, `GetScene()`, `Run()` |
| **embedded** · [EmbeddedRenderer.h](../include/minvex/EmbeddedRenderer.h) | `HostGraphicsContext`, `HostRenderTarget`, `EmbeddedFrameOptions`, `EmbeddedRenderer`. `Initialize()`, `Update()`, `Render()`, `WaitIdle()`, `DetachTarget()`, `Shutdown()` |
| **editor_document** · [EditorDocument.h](../include/minvex/EditorDocument.h) | `EditorDocument`는 Editor의 Scene·선택·미저장 상태 관리 모델. `OpenModel()`, `Open()`, `Save()`, `ImportModel()`, `PlaceModel()`, `SetWorldTransform()`, `SetMaterial/ResetMaterial`, `CrossFadeAnimation()`, `PlayBlendTree/SetBlendParameter/SeekBlendTree`, `SetAdditiveAnimation/SetAdditiveWeight/SetAdditiveMask/SeekAdditive/ClearAdditiveAnimation`, `BeginEdit/CommitEdit/CancelEdit`, `Undo/Redo`, `ClearUndoHistory()`, `SetMorphWeights/ClearMorphWeights` |
| **quality** · [Quality.h](../include/minvex/Quality.h) | `QualityPreset`, `QualityProfile`, `QualityOverrides`, `QualityCapabilities`, `QualityState`. `MakeQualityProfile()`, `ExpandQuality()`, `ResolveQuality()`, `SaveQuality()`, `LoadQuality()` |
| **telemetry** · [SystemInfo.h](../include/minvex/SystemInfo.h), [Telemetry.h](../include/minvex/Telemetry.h) | `HardwareInfo`, `MetricStatus`, `TelemetrySnapshot`. `SystemInfo::QueryHardware()`, `SystemMonitor::Sample()`, `FrameMonitor`, `EstimateQualityResources()`, `RecommendQuality()`, `SaveTelemetry()` |
| **animation** · [Skeleton.h](../include/minvex/Skeleton.h) | `JointId`, `SkeletonJoint`, `SkeletonAsset`, `SkeletonSocket`, `SharedSkeletonAsset`, `SharedSkeletonHandle`, `SkeletonBinding`. `CreateSharedSkeleton()`, `FindSkeletonJoint()`, `BindSkeleton()` |
| **optimization** · [Optimization.h](../include/minvex/Optimization.h) | `OptimizeMesh()`, `AnalyzeMeshLocality()`, `GenerateMeshLods()`, `SelectMeshLod()`, `TextureMipBytes()`, `OptimizationStatistics` |
| **debug** · [Debug.h](../include/minvex/Debug.h) | `DebugView`, `DebugOptions`, `DebugDraw`, `RenderDebugInfo`. `InspectBoneInfluences()`, `InspectJoint()`, Viewer/Embedded의 `SetDebugOptions/SetDebugDraw/GetRenderDebugInfo()` |
| **core** · [Result.h](../include/minvex/Result.h) | `Result<T>`, `Error`, `ErrorCode`. 실패 원인·힌트·입력 경로 전달 |

## 화면 UI

[Ui.h](../include/minvex/Ui.h)의 `UiCanvas`는 Scene과 독립된 화면 overlay임. `minvex::ui`로 CPU 작성·저장·native text raster를 사용하고 `Viewer::GetUi()` 또는 `EmbeddedRenderer::GetUi()`로 3D 위에 합성함. Windows는 DirectWrite, macOS는 Core Text로 텍스트를 표시함. Python/Web UI binding은 제공하지 않음.

```cpp
lv::Viewer viewer;
auto& ui = viewer.GetUi();
// 폴더 생성은 명시적이며 Viewer 생성 자체는 파일을 만들지 않음.
auto folder = ui.SetFontDirectory("Font", true);
lv::UiElement button;
button.Kind = lv::UiKind::Button;
button.Text = "Open model";
button.Layout = {24, 24, 220, 56};
button.Typography.Horizontal = lv::UiAlign::Center;
button.Normal.Fill.fill({.2f, .35f, .8f, 1});
button.Normal.Radius.fill(12);
button.Hover = button.Normal;
button.Hover->Fill.fill({.3f, .5f, 1, 1});
auto added = ui.Add(button); // Result<UiId> 확인 후 Value() 사용
lv::ViewerRunOptions run;
run.OnFrame = [](lv::Viewer& v, const lv::ViewerFrameInfo&) {
    auto events = v.GetUi().PollActivations();
    if (events) { for (auto id : events.Value()) { /* id에 대응하는 Host 동작 */ } }
};
// viewer.Run(run)의 Result도 확인함.
```

| API / 타입 | 계약 |
|---|---|
| `Add / Set / Get / Elements / Remove / Clear` | 텍스트·버튼·패널 작성, 독립 복사본 조회. Set은 ID를 유지하고 전체 description을 교체함. Add는 입력의 ID를 무시함. 실패한 검증/로드는 기존 요소를 유지함 |
| `UiLayout` | 좌상단 원점 logical pixel. 절대 배치에서 pivot 위치 = 부모 content 크기 × anchor + X/Y. 루트의 부모 영역은 viewport임. Width/Height·Fixed/Fill/Hug·Min/Max·Flex, pivot·회전 radians·ScaleX/Y·형제 Z 지원. 부모의 affine transform을 상속함 |
| `Parent / UiContainer` | 부모 Panel의 Row·Column·균등 Grid·Absolute, L/T/R/B padding, gap, justify·cross align, Visible/Clip/Scroll overflow. 부모 삭제는 subtree 삭제임. cycle·없는/비 Panel 부모·깊이 초과를 거부함 |
| `ResolveLayout / SetScroll` | authored 값을 바꾸지 않는 paint 순서의 `UiLayoutBox` 복사본. 최종 크기·local→viewport 2D affine matrix·content extent·실제 scroll·상속된 Visible/Enabled를 제공함. 스크롤은 런타임 상태이며 저장/Undo 대상이 아님 |
| `UiStyle` | sRGB RGBA 색상, TL/TR/BR/BL의 4색 bilinear gradient·개별 radius, 안쪽 border, 부드러운 shadow, 요소 전체 Opacity. radius는 렌더링 시 짧은 변의 절반으로 제한함 |
| `UiTextStyle / UiTextRun` | OS 기본 font, Family/FontFile, Size·Weight·Italic·Underline·StrikeThrough·Wrap·LineHeight·LetterSpacing·align·Padding. `Runs`가 있으면 run Text를 이어 붙여 표시하고 기존 Text는 표시하지 않음. run별 선택적 font·size·weight·color·장식·자간 override는 나머지 속성을 상속함. 넘친 글자는 content box에서 잘림. IME/caret 편집은 제공하지 않음 |
| 상태별 style | `Hover / Pressed / Focused / Disabled`는 선택적 **완전 교체**임. 우선순위 disabled → pressed/captured → hover → focused → normal. 값이 없는 상태는 다음 가능한 style을 사용함 |
| `Clip / Visible / Enabled / BlocksInput` | clip은 viewport logical rectangle이며 글자·도형·그림자·hit test에 적용함. disabled 버튼은 입력을 차단하지만 활성화하지 않음. BlocksInput=false인 요소는 입력을 통과시킴 |
| `ProcessInput / PollActivations` | 계층 paint 순서·ancestor clip·동일 affine transform으로 rounded hit-test. 같은 버튼에서 down/release해야 활성화함. Cancel/삭제/상위 비활성화는 capture를 해제함. Tab/Shift+Tab은 표시/활성 상태의 생성 순서, Enter/Space는 focus 활성화. ScrollX/Y는 logical pixel delta(오른쪽/아래 양수)이며 가장 안쪽 scroll ancestor부터 처리함 |
| `SetFontDirectory / FontDirectory / Fonts / RefreshFonts` | Font 폴더는 기본 CWD/Font이며 create=true로 생성 가능. `Fonts()`는 OS와 폴더의 TTF/OTF/TTC family를 조회하고 `Fonts(false)`는 폴더만 조회함. FontFile은 폴더 상대 경로이며 경로 탈출을 거부함. 파일을 바꾼 뒤 RefreshFonts를 명시적으로 호출함 |
| `Save / Load` | `Minvex.UI` writer v2 / reader v1~v2, 권장 `.mvxui.json`, atomic 저장. 계층·배치·rich run과 상대 FontDirectory를 저장하고 font bytes·history·scroll은 내장하지 않음. Load는 전체 검증 후 ID/parent를 재매핑하고 입력을 초기화함. 성공한 Save/Load는 현재 상태를 clean으로 표시함. Load 자체도 Undo 가능함. Scene v13에는 UI를 저장하지 않음 |
| `BeginEdit / CommitEdit / CancelEdit` | 여러 속성/요소 변경을 하나의 Undo로 묶거나 시작 상태로 취소함. 중첩 편집을 지원하지 않으며 활성 편집 중 Save/Load/Undo/Redo/ClearHistory는 InvalidState임. 빈 묶음은 이력을 추가하지 않음 |
| `Undo / Redo / History / ClearHistory` | UI 전용 content snapshot과 dirty/saved 상태를 복원함. History는 CanUndo/CanRedo/Editing/Dirty와 이력 수를 반환함. no-op·검증 실패는 redo를 지우지 않으며 새 성공 편집은 redo를 지움. ClearHistory는 content·dirty를 유지함 |
| `Rasterize` | native shaping, top-left origin의 premultiplied sRGB RGBA8 복사본. UI 변경·DPI·resize 때만 viewer bitmap을 갱신하고 Scene 품질 축소 뒤의 full-resolution pass에서 합성함. UI 합성 자원은 telemetry와 render debugger에 포함함 |

모든 canvas 수정·조회·저장·로드·입력·raster는 생성 스레드에서 호출함. `Revision / Focused / Empty`는 owner thread 전용의 unchecked query임. GetUi는 소유자의 수명 동안만 유효한 참조이며 Viewer/Embedded의 thread-safe 예외에 포함하지 않음. 독립 `UiCanvas`는 GPU 없이 Windows/macOS native text를 생성할 수 있음. 다른 플랫폼의 native font/text는 `UnsupportedFeature`임.

UI는 이전 CPU bitmap을 보관하고 요소·상태·변환·상위 clip의 변경 영역을 갱신함. 이동·삭제 전의 영역도 지운 뒤 겹치는 요소를 paint 순서대로 다시 그리며, 정적 UI는 bitmap을 재사용함. 첫 raster·DPI/viewport 변경·Font 폴더 변경·`RefreshFonts()`는 전체를 다시 그림. 실패한 raster는 보관된 이미지를 부분 갱신하지 않으며 반환한 pixel 배열은 독립 복사본임.

canvas별 bitmap과 변경 판정 데이터의 추정 보관량은 32 MiB로 제한함. 이 예산을 넘는 화면은 전체 raster로 처리하고 캐시를 보관하지 않음. 작업 중 복사본·native text·GPU 자원은 이 예산에 포함하지 않음. 여러 요소가 함께 바뀌거나 스크롤·재배치로 변경 영역이 커지면 비용도 커질 수 있음. 갱신 빈도와 요소 수는 대상 기기에서 측정해 정하며, 요소 수 한도는 프레임 성능을 보장하는 한도가 아님.



GPU 합성은 같은 크기의 overlay texture와 staging buffer를 재사용하고 변경된 bitmap을 전송함. 전송은 현재 전체 bitmap 단위이며 row alignment에 따른 padding을 포함함. GPU 논리 buffer 비용·render debugger에는 UI upload staging이 포함됨.

한도는 canvas당 512 요소, 요소당 UTF-8 텍스트 16,384 bytes, 이름/family 256 bytes, activation 1,024개, UI JSON 8 MiB, Font 폴더 검색 128개 파일·파일당 64 MiB임. 요소 크기는 축당 1..2048 logical pixels, scale .01..16, font size 1..256, line height .5..4, letter spacing -32..128, shadow blur 0..128, border 0..1024임. Padding은 글자 content 영역을 남겨야 함. raster/output UI 크기는 축당 1..4096 framebuffer pixels, DPI scale .25..4이며 element text bitmap도 축당 4096 이하임. 색상/opacity/anchor/pivot은 0..1이고 비유한 값은 거부함.

계층은 최대 32 parent 단계, Grid는 1..64 열, gap은 0..2048, container padding은 0..1024, Flex weight는 .001..1000임. Rich run은 요소당 128개, 합친 run Text는 16,384 UTF-8 bytes, FontFile은 4,096 UTF-8 bytes 이하임. history의 과거/미래 이력 합계는 최대 128개이며 active edit baseline을 포함한 snapshot 추정 저장량을 32 MiB 이내로 제한하고 오래된 이력을 먼저 제거함. 단일 content snapshot이 32 MiB를 넘으면 편집은 `InsufficientResource`로 거부됨. 이는 allocator/GPU/작업 중 복사본을 포함하는 전체 프로세스 메모리 한도가 아님.

자동 배치의 Fixed는 지정 크기, Fill은 남은 공간을 Flex 비율로 분배한 크기, Hug는 자식/실제 native text 측정 크기임. 최종 크기는 Min/Max로 제한하며 텍스트 padding 뒤에 최소 1 pixel을 남김. 공간이 부족하면 고정 크기와 최소 크기를 줄이지 않고 overflow함. Row/Column은 줄바꿈 없이 배치하며 Grid는 균등한 열/행 track을 사용함. Stretch는 cross-axis의 지정 크기도 덮어쓸 수 있음. 형제의 Order로 flow 순서를 바꾸며 같은 Order는 생성 순서임. Order/Z는 각각 ±1,000,000 이하이며 Order는 Z/keyboard 순서를 바꾸지 않음. Flow 자식의 X/Y·anchor는 배치에 사용하지 않고 pivot·rotation·scale은 최종 slot에 적용함. `Layout.Absolute=true`는 flow에서 빠져 부모 content의 anchor/X/Y를 사용함. Hug 크기는 변환 전 자식 bounds를 기준으로 하며 같은 축의 Hug 부모→Fill 자식 및 Hug 부모에 대한 absolute anchor는 순환 의존으로 거부함. 숨긴 자식은 flow 공간을 차지하지 않음.

부모 transform·Visible·Enabled와 ancestor clip이 자식에 적용됨. Z는 형제 안에서만 비교하고 각 subtree를 한 stacking 단위로 그림. 스타일/opacity를 부모에서 자동 상속하지 않음. Clip/Scroll은 부모의 padding 안쪽 **사각형**을 local 좌표에서 clip하며 `UiElement::Clip`은 viewport 축에 정렬된 추가 clip임. Content extent와 scroll 한도는 변환 전 자식 layout bounds를 사용함. SetScroll 입력은 축당 0..2,000,000이고 실제 offset은 layout의 overflow에 clamp됨. Undo/Redo/Cancel/Load는 입력 capture·활성화 큐·런타임 scroll을 초기화함. 너무 작거나 큰 합성 transform은 resolve 오류로 진단함.

리치텍스트는 markup parser 없이 구조화된 UTF-8 run으로 작성함. optional 필드가 없으면 요소의 typography를, Color가 없으면 현재 버튼 상태의 Text 색을 상속함. Family와 FontFile은 각각 상속하므로 다른 파일의 첫 family를 사용하려면 Family를 빈 문자열로 override함. 줄바꿈·정렬·padding은 paragraph 전체에 적용하고, 행 간격은 base와 run의 최대 font size × LineHeight로 통일함. OS shaping은 이어 붙인 텍스트 전체에 적용되며 glyph fallback은 native engine을 따름.

```cpp
lv::UiCanvas ui;
lv::UiElement column;
column.Kind = lv::UiKind::Panel;
column.Container.Flow = lv::UiFlow::Column;
column.Container.Padding.fill(16);
column.Layout = {24, 24, 360, 200};
auto parent = ui.Add(column); // 각 Result의 성공을 확인한 뒤 Value() 사용
lv::UiElement label;
label.Parent = parent.Value();
label.Layout.WidthSizing = lv::UiSizing::Fill;
label.Layout.HeightSizing = lv::UiSizing::Hug;
lv::UiTextRun emphasis;
emphasis.Text = "Minvex ";
emphasis.Weight = 700;
emphasis.Color = lv::UiColor{.6f, .4f, 1, 1};
lv::UiTextRun body;
body.Text = "rich text";
label.Runs = {emphasis, body};
auto added = ui.Add(label);
// BeginEdit() → 여러 Set/Add/Remove → CommitEdit() 또는 CancelEdit()
// History().Value().CanUndo/CanRedo를 확인한 뒤 Undo()/Redo() 호출
```

Undo는 삭제/로드 이전의 ID를 복원할 수 있으나, 버린 redo branch의 ID를 새 요소에 재사용하지 않음. 조회한 값 복사본과 Editor 선택 상태는 별도이며 복원 뒤 다시 조회함. Hover·focus·입력·font refresh·scroll은 dirty를 만들지 않음. Font 폴더 선택의 Undo는 경로 속성만 복원하고 이미 만든 폴더나 파일을 삭제하지 않음. 성공한 Save는 const 호출이어도 saved marker를 갱신함. 실패한 Save는 dirty와 이력을 유지함.

`InvalidArgument`는 속성/입력 검증, `InvalidHandle`은 제거/로드 후의 ID, `WrongThread`는 소유 스레드 위반, `InsufficientResource`는 요소/큐/할당 한도, `InvalidData`는 UI JSON·font 파일·native raster 실패임. UI IO 오류에는 SourcePath를 제공함. 잘못된 폰트 선택은 렌더링 때도 진단되며 다른 폰트로 조용히 바꾸지 않음. 선택된 폰트에 없는 glyph의 OS fallback은 native text engine을 따름. 시스템 font bytes를 복사/재배포하거나 사용자 font를 OS 전역에 설치하지 않음.

Standalone은 absolute `ViewerInputEvent::X/Y`와 `KeyUp`을 UI에 먼저 전달함. UI가 소비한 입력은 OnInput/기본 카메라에 전달하지 않음. Embedded Host는 `GetUi().ProcessInput()`에 **viewport 내부 logical 좌표와 logical 크기**를 전달하고 소비 결과에 따라 자체 입력을 차단함. `EmbeddedFrameOptions::UiScale`은 framebuffer/logical 비율이며 기본 1임. UI를 사용하면 Host viewport도 축당 4096 이하로 제한됨.

Editor의 **UI Designer**에서 계층·container/sizing·run별 서식·style·font를 편집하고 별도의 UI Undo/Redo를 제공함. Drag/typing의 연속 속성 변경은 하나의 편집으로 묶고 Escape로 취소할 수 있음. UI Designer에 focus가 있고 text input이 자체 편집 키를 사용하지 않을 때 Ctrl+Z·Ctrl+Y/Ctrl+Shift+Z·Ctrl+S를 UI에 전달하며 Scene 이력은 별도임. New UI·Load UI·Quit은 미저장 UI의 Save/Discard/Cancel을 제공함. Scene의 New/Open은 독립 UI를 유지함. UI는 Save UI로 따로 저장함.

Font/raster 실패는 UI 미리보기 오류로 표시하고 Scene 표시를 유지함. 속성을 고치거나 Retry preview로 다시 시도함. `minvex_ui_overlay`는 Column/Hug·rich run·묶음 theme 변경과 Undo/Redo 예제임. UI v2는 v1만 지원하는 이전 reader로 읽을 수 없음. 폰트 선택과 글자 raster 결과는 운영체제의 텍스트 엔진을 따름.

## 좌표·변환 계약

- 오른손 좌표계, Y-up, 카메라 로컬 전방 -Z임.
- 행렬은 column-major 저장·column vector 곱이며 `projection * view * model * position` 순서임. `Transform::Matrix()`는 `T * R * S`이고 원근 나눗셈 후 깊이 범위는 [0,1]임.
- 삼각형 목록과 CCW 앞면을 사용함. `MeshData`는 32-bit index, `Vertex`는 Position·Normal·Color·TexCoord·Joint/Weight 4개·Tangent를 포함함. 정확한 배열 타입은 [Mesh.h](../include/minvex/Mesh.h)를 따름.
- 비유한 값·singular Transform·잘못된 Quaternion은 공개 검증 함수의 계약에 따라 거부함. `DecomposeTRS()`는 shear를 TRS로 가장하지 않고 거부함. 모델 Import의 단위·좌표 변환과 오류는 Asset 진단을 확인함.

## Asset 로딩과 Scene 구성

| 기능 | 접근법과 역할 |
|---|---|
| 모델 읽기 | `AssetRegistry::LoadModel(path, settings)`로 glTF/GLB·OBJ/MTL·FBX를 읽고 immutable `ModelHandle`을 얻음. `ImportSettings`로 좌표·단위·자원 한도를 설정함. `InspectModel/ValidateModel`로 내용과 오류를 조회하고 `ImporterRegistry`로 importer를 등록함 |
| 캐시·재임포트 | `PrepareModel/AddPrepared`로 준비와 게시를 분리함. `ReimportModel/ReplacePrepared`는 Asset ID를 유지하며 후보를 교체함. `Scene::ReplaceModel`로 해당 Asset을 공유하는 인스턴스에 반영함 |
| 비동기 로딩 | `AssetLoader::Submit` → `Poll` → Ready 모델을 owner thread에서 게시함. `Cancel/Forget`으로 작업과 보관 결과를 정리함. Viewer는 `AddModelAsync/GetAssetLoad/CommitModelLoad/CancelModelLoad`를 제공함 |
| Hot Reload | `AssetHotReload::Watch/Synchronize/Poll`로 의존 파일 변경을 조회함. Viewer는 `SetHotReload(true)`와 `PollHotReload(now)`를 제공함. 실패 원인은 `GetAssetLoadError()`로 확인함 |
| 객체 계층 | `Scene::CreateObject/AddModel`, `SetParent`, `SetTransform/SetWorldTransform`, `SetVisible`, `RemoveObject`로 구성함. `GetObject/ObjectIds/GetWorldTransform/GetModelBounds`로 조회함. `RemoveObject`는 subtree를 함께 제거함 |
| 재질·변형 | `GetMaterial/SetMaterial/ResetMaterial`은 인스턴스 재질 override를 관리함. `GetAnimation`의 Player로 Pose·Morph·재생 상태를 제어함. 원본 Asset은 변경하지 않음 |
| 프로젝트 저장 | `SaveScene/LoadScene`은 Scene과 외부 모델 참조를 저장·복원함. `CopyModelToProject`는 모델 및 의존 자산을 프로젝트로 복사함. `SavePose/LoadPose`는 Skeleton에 대응하는 Pose를 저장·복원함 |

CPU에서 직접 Scene을 사용할 때 `UpdateAnimations(deltaSeconds)`로 시간을 진행함. Viewer에서는 기본 실행 루프가 진행함. Scene 참조를 반환하는 조회는 객체 삭제·모델 교체·문서 편집 뒤 다시 수행함.

## 카메라·품질·진단

| 기능 | 접근법과 역할 |
|---|---|
| 카메라 | `Scene::MainCamera` 또는 `Viewer::SetCamera()`로 perspective/orthographic 카메라를 설정함. `Viewer::FrameScene()`으로 전체 장면을 프레이밍함. 기본 마우스 입력은 Orbit·Pan·Zoom을 처리하며 Host 입력은 `ProcessInput()`으로 전달함 |
| 품질 | `MakeQualityProfile(Low/Medium/High)`와 `Overrides`로 해상도 비율·MSAA·조명·Shadow·FXAA·HDR·압축·LOD·FPS·On-Demand를 지정함. `SetQuality()`로 적용하고 `GetQuality()`로 요청값·적용값·조정을 확인함 |
| 지원 기능 | `GetQualityCapabilities()`는 현재 graphics context의 기능과 한도를 반환함. `ExpandQuality()`는 설정을 확장하고 `ResolveQuality()`는 capability와 출력 크기에 맞춰 해석함. `AllowFallback`으로 대체 허용 여부를 정함. `SaveQuality/LoadQuality`로 프로파일을 저장·복원함 |
| 하드웨어·시스템 | `SystemInfo::QueryHardware()`로 창 없이 장치를 열거함. `SystemMonitor::Sample(now)`로 프로세스·시스템 사용량을 조회함. `Metric::Status/Value/Source`를 함께 읽으며 미지원·미수집 값을 0으로 해석하지 않음 |
| 실행 지표 | `Viewer/EmbeddedRenderer::GetTelemetry()`로 현재 장치·Scene 렌더링·자원 비용을 조회함. `FrameMonitor`는 호출자가 제공한 CPU/GPU 시간과 cadence를 집계함. `SaveTelemetry()`는 JSON/CSV를 저장함 |
| 자원·품질 추천 | `EstimateQualityResources()`는 엔진의 논리적 자원 비용을 계산함. `RecommendQuality()`는 capability·메모리·현재 Scene 관측으로 프로파일을 추천함. 논리적 크기는 드라이버 실제 할당량과 다를 수 있음 |
| 렌더링 진단 | `SetDebugView/SetDebugOptions`로 Lit·Unlit·Wireframe·Normal·UV·Depth·Skeleton·BoneWeight·Shadow 진단을 선택함. `DebugDraw`로 선·화살표·상자·구·캡슐·축·Skeleton·카메라 frustum을 작성하고 `SetDebugDraw()`로 복사 전달함 |
| 자원 조회 | `GetRenderDebugInfo()`는 pass·draw·shader·pipeline·texture·buffer 및 최적화 통계를 반환함. native GPU handle의 소유권을 넘기지 않음. `InspectJoint/InspectBoneInfluences`로 본 행렬과 가중치를 조회함 |

## Editor 문서 통합

[EditorDocument.h](../include/minvex/EditorDocument.h)의 `EditorDocument`는 Scene 편집 모델이며 `minvex::editor_document`로 연결함. `OpenModel/Open/Save`, `ImportModel/PlaceModel`, 선택·Transform·재질·Pose·Animation 편집을 제공함. `BeginEdit/CommitEdit/CancelEdit`로 연속 조작을 묶고 `Undo/Redo`로 복원함. 파일 대화상자·Hierarchy·Inspector·Gizmo·Timeline·UI Designer는 `minvex_editor` 애플리케이션에서 제공함.

애플리케이션에서 편집 UI를 구현할 때 `EditorDocument`로 Scene 변경을 적용하고 `GetScene()`의 결과를 Embedded renderer에 전달하여 표시함. 문서의 Animation은 `UpdateAnimations()`로 진행함. Standalone Viewer는 자신의 `GetScene()`으로 장면을 관리함. UI Canvas의 저장과 Undo/Redo는 Scene 문서와 독립적임.
| **debug** · [Debug.h](../include/minvex/Debug.h) | `DebugView`, `DebugOptions`, `DebugDraw`, `RenderDebugInfo`. `InspectJoint()`, `InspectBoneInfluences()`. Viewer/Embedded의 `SetDebugOptions()`, `SetDebugDraw()`, `GetRenderDebugInfo()` |
| **optimization** · [Optimization.h](../include/minvex/Optimization.h) | `MeshOptimization`, `MeshOptimizationOptions`, `MeshLocalityStatistics`, `OptimizationStatistics`, `OptimizeMesh()`, `AnalyzeMeshLocality()`, `GenerateMeshLods()`, `SelectMeshLod()`, `TextureMipBytes()` |

## 메시 캐시·전송 최적화

`OptimizeMesh(mesh, lods={})`는 원본을 수정하지 않고 triangle 순서와 vertex 배치를 바꾼 독립 복사본을 반환함. Post-Transform Vertex Cache Optimization 뒤 Vertex Fetch Optimization을 수행하며 정점의 모든 속성·삼각형 winding·중복 삼각형·미사용 정점을 보존함. `VertexRemap[source]`는 결과 정점 번호인 전단사 매핑이며 반환된 base/LOD index는 이미 재매핑되어 있음. LOD는 최대 3개이고 입력 순서·Error를 유지함. triangle 순서는 보존하지 않으므로 triangle 순서에 의미를 부여하는 처리에는 별도 대응이 필요함.

`OptimizeMesh(mesh, lods, options)`의 `VertexCache / VertexFetch / CoarsestLodFirst` 기본값은 true임. fetch pass는 가장 적은 index의 LOD부터 정점을 모아 작은 LOD가 좁은 vertex 범위를 사용하게 함. base LOD의 fetch locality가 나빠질 수 있으며 `CoarsestLodFirst=false`로 base 우선 배치를 선택할 수 있음. `VertexFetch=false`이면 vertex 순서와 identity remap을 유지함.

`Overdraw`는 기본 false이며 불투명·정적 geometry에만 호출자가 선택함. cache pass 뒤에 실행하며 `VertexCache=true`가 필요함. `OverdrawThreshold`는 유한한 1..3, 기본 1.05이며 cache 효율과 overdraw의 교환을 제어함. 유효하지 않은 mesh·LOD·옵션은 `std::invalid_argument`를 던지고 할당 실패는 표준 예외를 따름. GPU context 없이 서로 독립적인 입력을 각 스레드에서 처리할 수 있음.

`AnalyzeMeshLocality(mesh, overdraw=false)`는 소프트웨어 모델을 반환함. `Acmr`는 triangle당 정점 변환 횟수, `Atvr`는 참조 정점당 변환 횟수, `Overfetch`는 필요한 vertex bytes 대비 fetch bytes임. cache 모델은 FIFO 16·warp 32, fetch 모델은 64-byte cache line·128 KiB cache를 사용함. `HasOverdraw=true`인 경우에만 software raster의 `PixelsCovered / PixelsShaded / Overdraw`가 유효함. index 수는 analyzer의 32-bit counter 보호를 위해 `UINT32_MAX/256` 이하임. 기존 `CacheMissesBefore/After`는 별도의 단순 16-entry LRU 모델임. 이 수치들은 실제 GPU counter·시간·FPS가 아니며 hardware별 효과는 실제 장면에서 측정함.

Renderer는 위 cache/fetch 최적화를 사용하고, 전체 vertex 수가 Native는 65,536 이하, WebGL2는 65,535 이하이면 base/LOD GPU index를 16비트로 저장함. WebGL2의 0xffff는 [primitive restart 예약값](https://registry.khronos.org/webgl/specs/latest/2.0/#NO_PRIMITIVE_RESTART_FIXED_INDEX)이므로 그보다 큰 population에는 32비트를 사용함. CPU `MeshData`의 32비트 index 계약은 유지함. overdraw 재정렬은 CPU 최적화 API에서 명시적으로 선택할 수 있으며 내장 Renderer는 자동 적용하지 않음. Renderer에 메시를 전달하면 자체 cache/fetch pass를 수행하므로 사전에 만든 triangle 순서를 보존하는 계약은 아님. 정적 instancing은 같은 변환 목록의 GPU buffer를 재사용하며 목록이 바뀌면 교체함. 소프트웨어 분석·정점 순서 최적화는 [meshoptimizer](https://github.com/zeux/meshoptimizer/tree/v1.3)의 모델과 알고리즘을 사용함.

```cpp
#include <minvex/Optimization.h>
lv::MeshOptimizationOptions options;
options.Overdraw = true; // 애플리케이션이 불투명 정적 메시임을 확인한 경우
auto optimized = lv::OptimizeMesh(mesh, lods, options);
auto locality = lv::AnalyzeMeshLocality(optimized.Geometry);
```

## 사용 예제

### Standalone Viewer

```cpp
#include <minvex/Viewer.h>
#include <cstdio>

int main() {
    lv::Viewer viewer;
    auto object = viewer.AddModel("model.glb");
    if (!object) {
        std::fprintf(stderr, "%s\n", object.Error().Message.c_str());
        return 1;
    }
    return viewer.Run() ? 0 : 1;
}
```

기본값은 1280×720, VSync·기본 입력·자동 Focus 활성화, Medium 품질임. `ViewerRunOptions::OnFrame`에서 Scene을 갱신할 수 있으며 `PlayAnimation()`으로 재생 중인 애니메이션은 기본적으로 callback 전에 진행합니다. 직접 진행하려면 `AdvanceAnimations=false`로 설정하고 `Scene::UpdateAnimations()`를 한 번 호출합니다. `ViewerFrameInfo::DeltaTimeSeconds`는 업데이트 사이의 경과 시간을 최대 0.25초로 제한한 값입니다. 이동 속도가 초당 단위라면 이 값을 곱해 프레임별 변위를 구합니다.

설치 SDK의 `share/minvex/examples/QuickStart`에는 `find_package(Minvex 1.0.1 CONFIG REQUIRED COMPONENTS viewer graphics)`로 빌드하는 전체 CMake 프로젝트가 있습니다. [빠른 시작 빌드 명령](../README.md#설치-sdk로-움직이는-장면-시작)

macOS Metal에서 `VSync=false`는 display sync 해제를 요청함. `OnFrame` 호출 빈도와 표시 FPS는 OS의 display callback 및 창 가림 상태의 영향을 받을 수 있으므로, 이 옵션만으로 주사율을 넘는 무제한 FPS를 보장하지 않음.

Standalone·Editor의 기본 창 아이콘은 실행 파일에 포함되며 별도 이미지 파일이나 실행 디렉터리에 의존하지 않음.

### Animation / Cross-fade

아래 코드는 애플리케이션의 초기화·갱신 함수 안에서 사용함.

```cpp
#include <minvex/Asset.h>

lv::AssetRegistry assets;
auto model = assets.LoadModel("character.glb");
lv::AnimationPlayer player;
player.Bind(model);
if (model->Animations.size() >= 2) {
    player.Play(0);
    player.Update(.5);
    player.CrossFade(1, .3); // Clip 1로 0.3초 동안 전환
    player.Update(.15);
    auto weight = player.BlendWeight(); // 0.5
    auto pose = player.Evaluate();
}
```

`CrossFade(clip, seconds=.3)`는 0초이면 즉시 전환함. 재전환·Paused/Stopped 상태에서는 현재 base pose를 source로 고정함. Position/Scale은 선형 보간, Rotation은 Slerp, 수동 Pose override는 마지막에 적용함. Scale 부호가 호환되지 않는 조합은 거부함.

Pause·속도 0은 전환도 멈추며 Seek·Play·Stop·ResetPose는 전환을 해제함. `Stop()`은 Clip과 override를 유지하고 time=0, `ResetPose()`는 Clip·override를 해제하여 Reference Pose로 돌아감. `ApplyPose()`·`SetJointOverride()`로 full/partial Pose를 적용하며 speed 기본값은 1, loop는 true임.

### Pose Blending Core

`PoseBuffer`는 SkeletonSignature·노드별 LocalTransforms·Channels를 소유하는 CPU 값 타입임. `PoseChannels`는 None/Translation/Rotation/Scale/All 비트이며 `|`·`&`로 조합함. `ReferencePoseBuffer()`의 채널은 None이고 `EvaluateAnimation()`은 Clip에 기록된 채널만 표시함. 없는 채널은 Reference Pose로 보완하며 읽기 전용 노드는 원본 행렬을 유지함.

```cpp
const auto& skeleton = model->Skeleton; // 유효한 ModelHandle
auto a = lv::EvaluateAnimation(skeleton, model->Animations.at(0), .5);
auto b = lv::EvaluateAnimation(skeleton, model->Animations.at(1), .25);
auto mixed = lv::BlendPoses(skeleton, a, b, .5f);
auto finalPose = lv::ApplyPoseOverrides(skeleton, mixed, player.Overrides());
auto matrices = lv::ComposePose(skeleton, finalPose); // Local/Global 행렬
```

`EvaluateAnimation()`은 유한한 0 이상 시간을 받아 duration까지 clamp함. 재생 상태·loop·Morph weight는 별도임. `BlendPoses()`는 weight [0,1], Translation/Scale Lerp·Rotation shortest-arc Slerp를 사용함. 선택한 Scale의 부호는 양쪽이 같아야 하며 endpoint에도 같은 검사를 적용함. 채널 누락은 weight 재정규화 없이 Reference와 혼합함.

`BlendPoses(..., weight, channelMask)`는 노드 수와 같은 `std::span<const PoseChannels>`를 선택적으로 받음. 빈 mask는 전체 채널 선택이며 미선택 채널은 from을 유지함. 결과 채널은 `from | (to & mask)`임. 수동 Pose는 `ApplyPoseOverrides()`로 마지막에 적용하며 해당 노드의 전체 TRS를 대체함. `AnimationPlayer::EvaluatePose()`도 같은 evaluator/composer 경로의 최종 local buffer를 반환함.

`ValidatePoseBuffer()`는 실제 Skeleton 서명·정확한 배열 길이·채널 비트·활성 TRS·읽기 전용 노드를 검사함. 누락 채널의 저장 값은 무시함. 불일치·잘못된 시간/weight·비정상 Quaternion/Scale은 `std::invalid_argument`임. 함수는 입력을 수정하거나 보관하지 않으며, immutable 입력을 공유하는 별도 버퍼 평가는 병렬 사용 가능함. Player의 동시 수정/평가는 금지임.

PoseBuffer와 채널 마스크는 평가 중간 결과이며 파일 저장 대상이 아님. 기존 Cross-fade의 live/frozen source·override 순서와 Morph 독립 채널을 유지함. 정적 SDK 소비자는 버전 변경 시 함께 재빌드해야 함.

### Blend Tree

`CreateBlendTree(model, BlendTreeDesc)`가 `shared_ptr<const BlendTreeAsset>`을 반환함. 정의·Clip 참조·삼각분할은 공유하고, `AnimationPlayer::Tree()`의 X/Y·Phase는 인스턴스마다 보유함. 정의는 생성 후 수정하지 않으며 Editor 편집은 새 Tree 생성으로 반영함.

```cpp
lv::BlendTreeDesc desc;
desc.Samples = {{0, 0, 0}, {1, 1, 0}}; // Clip index, threshold X, Y
auto tree = lv::CreateBlendTree(*model, desc);
player.PlayBlendTree(tree);
player.SetBlendParameter("Speed", .5f);
player.Update(.016);
auto weights = player.BlendWeights(); // Sample 순서와 같은 배열
```

Blend1D는 X 오름차순·Y=0인 1~64개 sample을 요구하며 인접 두 Clip을 혼합하고 범위 밖은 endpoint로 clamp함. Blend2D는 `Kind=BlendTreeKind::Blend2D`, 서로 다른 ParameterX/Y 이름과 3~64개 sample을 사용함. Delaunay facet 내부는 barycentric weight, 외부는 가장 가까운 convex-hull edge로 clamp함. `Triangles()`는 Editor/debug용 sample index 배열임.

중복·비정상 좌표·퇴화 영역·잘못된 Clip·Scale 부호를 검사함. 2D 좌표는 전체 extent로 정규화하며 점 간 거리 ≤1e-7 또는 facet 면적 기준 cross ≤1e-10인 수치 퇴화를 거부함. 이름은 최대 256 bytes임. `EvaluateBlendWeights()`는 sample별 비음수 weight를 반환하고, `EvaluateBlendTree(model, tree, x, y, phase)`는 독립 CPU PoseBuffer를 반환함. 유효 contributor의 weight를 정규화하고 등록 순서로 Lerp·progressive shortest-arc Slerp를 적용함. 누락 TRS는 Reference, Morph는 Node/Mesh 기본값으로 보완하며 수동 override를 마지막에 적용함.

`PlayBlendTree(tree, restart=true)`는 Clip/cross-fade를 해제하고 첫 sample 좌표로 시작함. 같은 handle에 restart=false이면 현재 파라미터·phase를 유지함. 공통 phase [0,1]에서 각 Clip을 `phase × duration`으로 샘플링하고 cycle duration은 현재 weight의 가중 평균임. 길이 0인 Clip은 t=0으로 샘플링하며 전체 cycle이 0이면 정지한 상수 Pose임. 발 접촉 시점의 자동 동기화는 제공하지 않음.

`SetBlendParameter(name, value)`·`SeekBlendTree(phase)`·`BlendTreeDuration()`을 제공함. 파라미터는 유한한 값, phase는 [0,1]임. 파라미터 편집은 phase를 유지하며 Pause·속도·Loop·역재생은 기존 제어를 따름. Stop은 Tree·파라미터를 유지하고 phase=0, ResetPose는 Tree도 해제함. Play는 단일 Clip으로 전환하고 Tree에서 CrossFade하면 현재 base TRS/Morph를 frozen source로 사용함.

Viewer·EditorDocument도 `PlayBlendTree/SetBlendParameter/SeekBlendTree` Result Facade를 제공함. Editor Animation Controls에서 이름·파라미터·Clip·threshold/좌표 편집과 Apply/Play, 현재 weight·2D 삼각분할 보기를 제공하며 편집은 Undo/Redo 대상임. Scene v13는 최대 1024개 공유 Tree 정의와 독립 phase/파라미터/재생 상태를 저장하며 reader는 v1~v13임. Reimport는 실제 Skeleton/Morph layout과 모든 참조 Clip index/name·Scale 호환성을 검사함.

잘못된 입력/호환성은 low-level API에서 `std::invalid_argument`임. 반환한 handle은 정의의 수명을 유지하며 `Tree()` 참조는 Player/Scene 편집 후 다시 조회함. immutable 정의의 동시 조회·서로 다른 Player 사용은 가능하고 같은 Player의 동시 수정/평가는 금지임. SDK 1.0.0 소비자는 함께 재빌드해야 하며 Pose v1·ImportVersion 6은 유지함.

### Additive Animation

`AdditivePose`는 SkeletonSignature·LocalDeltas·유효 Channels를 소유함. `MakeAdditivePose(skeleton, sample, reference)`는 Translation 차이, `reference⁻¹ × sample` local Rotation delta, Scale 비율을 생성함. 샘플에 없는 채널은 neutral delta이고 기준에 없는 채널은 Skeleton Reference 값을 사용함.

```cpp
player.PlayBlendTree(tree); // 단일 Clip 또는 Reference base도 가능함
player.SetAdditiveAnimation(clip, .4f); // 기본 기준은 Skeleton Reference Pose임
player.SetAdditiveMask(jointWeights); // 비어 있거나 노드 수와 같은 [0,1] 배열임
player.SeekAdditive(.5);
auto delta = player.EvaluateAdditive();
auto finalPose = player.EvaluatePose();
```

`ApplyAdditivePose(skeleton, base, delta, weight, mask={})`는 weight×Joint mask로 Translation을 더하고, `baseRotation × Slerp(identity, delta, w)`와 `baseScale × Lerp(1, ratio, w)`를 적용함. weight/mask는 유한한 [0,1]이며 0은 base를 유지함. 기준 Scale 절댓값은 1e-8 이상이고 sample과 부호가 같아야 함. 비정상 Quaternion·배열·서명·비율은 `ValidateAdditivePose()` 또는 합성 API에서 `std::invalid_argument`임.

Player는 TRS Additive 레이어 1개를 보유함. `SetAdditiveAnimation(clip, weight, reference={}, mask={})`의 명시적 full/partial PoseAsset은 고정 snapshot이며 빈 기본 인수는 Skeleton Reference임. TRS 채널이 없는 Clip은 거부하고 Morph weight는 기존 base/Tree 평가를 유지함. 순서는 Clip/Cross-fade/Tree → Additive → 수동 Pose override임. `EvaluateBasePose()`·`EvaluateAdditive()`·`EvaluatePose()`로 각 단계를 비교할 수 있음.

`Additive()`의 상태는 Clip·Time·Speed·Loop·Weight·State·Reference·JointMask임. 레이어 속도는 Player 속도와 곱해지고 playhead/loop는 base와 독립임. `RestoreAdditiveAnimation(state)`로 전체 설정을 검증·교체하며 `SetAdditiveWeight/SetAdditiveMask/SeekAdditive()`로 개별 편집함. Seek는 duration까지 clamp함. Pause/Resume은 base와 레이어를 함께 제어하고 Stop은 두 시간을 0으로 되돌리며 설정은 유지함. Play/PlayBlendTree/CrossFade는 레이어를 유지하고 ResetPose는 해제함. `ClearAdditiveAnimation()`은 base를 유지하며 레이어만 제거함.

Viewer·EditorDocument에도 같은 제어의 Result Facade를 제공함. Editor는 Clip·Skeleton/현재 base/Clip sample 기준 선택, weight·시간·속도·loop, Joint/Subtree mask, Base/Delta/Final 비교를 제공함. 편집은 Undo/Redo 대상이며 Scene v13는 레이어 reference·mask·재생 상태를 저장하고 reader는 v1~v13임. Reimport는 Skeleton·레이어 Clip index/name·시간과 reference/Scale 호환성을 검사함. Pose v1은 최종 TRS snapshot만 저장함.

공유 Model/Clip은 수정하지 않으며 레이어·기준 Pose·mask는 Player별로 소유함. 상태 참조는 Player/Scene 편집 후 다시 조회하고 같은 Player의 동시 수정/평가는 금지임. weight 0 또는 전체 mask 0인 레이어의 시간 변화는 정적인 base의 On-Demand 이미지를 무효화하지 않음. SDK 1.0.0 소비자는 함께 재빌드해야 하며 ImportVersion 6은 유지함.

### Animation Event

`AnimationEvent`는 TimeSeconds·Name·Payload이며 `AnimationClip::Events`는 시간순 기본 track임. `SetAnimationEvents(clip, events)`는 공유 Asset을 수정하지 않는 Player별 overlay를 교체함. 빈 overlay는 기본 track을 숨기고 `ResetAnimationEvents(clip)`은 원본을 복원함. `AnimationEvents(clip)`·`EventOverrides()`로 조회함.

```cpp
std::array<lv::AnimationEvent, 1> events{{{.35, "Footstep", "{\"side\":\"L\"}"}}};
player.SetAnimationEvents(clip, events);
player.Play(clip);
player.Update(.4);
auto batch = player.PollAnimationEvents(); // 소유권을 가진 발생 snapshot 배열임
// 또는 player.DispatchAnimationEvents(callback)에서 안전한 시점에 전달함
```

정방향은 `(old, new]`, 역방향은 `[new, old)`임. 같은 시각은 등록 순서를 유지하며 여러 loop도 모두 수집함. loop 경계에서는 정방향 duration→0, 역방향 0→duration 순서로 각각 한 번 발생함. Play·Stop·Pause·Pose 평가·0초 Clip은 자체 이벤트를 발생시키지 않음. Seek/SeekBlendTree는 기본 Silent이며 `AnimationSeekEvents::Collect`를 지정하면 wrapping 없이 이동 구간만 수집함.

Cross-fade는 target Clip만 수집함. Tree는 동일 Clip sample의 weight를 합산해 가장 큰 Clip 하나를 선택하고 동률은 첫 sample 순서임. 주 Clip 변경 시 과거 구간을 재생하지 않으며 Additive Clip은 기본 수집 대상이 아님. 발생값은 Model·Clip index/name·수집 당시 EventIndex·Event snapshot·Reverse를 포함함. index는 편집 이후의 영구 ID가 아니며 loop 반복은 별도 발생임.

Player 큐는 최대 1024개임. 초과 시 `AnimationEventOverflow`를 던지고 해당 Update/Seek의 시간·기존 큐는 유지함. Scene Update는 모든 Player의 이벤트 용량을 먼저 확인함. `PollAnimationEvents()`는 큐를 비워 소유 배열을 반환하고 `PendingAnimationEvents()`는 편집/배출 전까지 유효한 조회 span임. Scene의 batch는 ObjectId를 포함한 `SceneAnimationEvent`이며 객체 순서·Player 내 이동 순서로 반환함.

`DispatchAnimationEvents(callback)`은 큐 snapshot을 먼저 분리하고 모든 callback을 한 번씩 시도함. callback 중 객체 삭제·새 이벤트 수집은 현재 batch를 바꾸지 않음. 실패 시 첫 원예외를 Cause로 가진 `AnimationEventCallbackError`를 마지막에 던짐. Viewer·EditorDocument는 `SetAnimationEvents/ResetAnimationEvents/PollAnimationEvents/DispatchAnimationEvents`와 collecting Seek의 Result Facade를 제공하며 overflow는 InsufficientResource, callback 오류는 CallbackFailure임.

track당 최대 1024개, overlay 최대 256 track, 기본 track별·인스턴스 전체 overlay별 이름/페이로드 합계는 각각 1 MiB임. 이름은 1~256 bytes, payload는 최대 4096 bytes의 NUL 없는 UTF-8이며 시간은 유한한 [0,duration] 오름차순임. 잘못된 데이터는 `std::invalid_argument`임. Player/Scene의 동시 수정·평가는 금지이고 Viewer는 생성한 owner thread에서 사용함. callback은 명시적 Dispatch를 호출한 스레드에서 실행하며 전달된 참조는 해당 호출 동안만 유효함.

Editor는 Timeline marker, 추가·삭제·시간·이름·payload 편집, 수집 Seek 선택과 발생 로그를 제공함. marker 편집은 Undo/Redo 대상이며 큐 배출·로그는 문서를 dirty로 만들지 않음. Scene v13는 overlay와 Clip identity를 저장하고 reader는 v1~v13임. 발생 큐·callback·로그는 저장하지 않으며 Undo/Redo는 과거 큐를 재발송하지 않음. Reimport는 overlay Clip identity/duration을 검사함. SDK 1.0.0 소비자는 함께 재빌드해야 하며 Pose v1·ImportVersion 6은 유지함.

### Shared Skeleton / Leader Pose / Socket (SDK 1.0)

공개 정의는 `minvex/Skeleton.h`·`minvex/Scene.h`이며 `minvex::animation`과 `minvex::scene`에 구현됨. 제품명은 Minvex, C++ namespace는 `lv`임.

| API / 타입 | 동작 |
|---|---|
| `SkeletonSocket` | `Name`, 대상 노드 이름 `Joint`, 노드 기준 `LocalTransform` |
| `SharedSkeletonHandle` / `SharedSkeletonAsset` | 불변 Skeleton·공유 소켓 snapshot의 강한 공유 소유권. `Skeleton()`·`Sockets()`로 조회 |
| `CreateSharedSkeleton(skeleton, sockets={})` | 입력을 복사/이동하여 검증한 공유 정의 생성. 본이 하나 이상 필요함 |
| `FindSkeletonJoint(skeleton, name)` | 유일한 노드 이름을 JointId로 해석. 누락·중복 이름 거부 |
| `ValidateSkeletonSocket(skeleton, socket)` | 이름·노드 참조·유효한 offset TRS 검사 |
| `BindSkeleton(shared, modelSkeleton)` | `SkeletonBinding::ModelToShared/SharedToModel` 매핑. 일반 노드/누락된 공유 본은 `NoJoint` |
| `Scene::SetSharedSkeleton(id, handle)` | 모델 인스턴스에 공유 정의 연결/해제. 활성 Leader/Follower 관계가 있으면 변경을 거부함 |
| `Scene::SetLeaderPose(follower, leader)` | 최종 본 행렬의 source 설정. `NoObject`는 독립 Player 복원 |
| `Scene::EvaluatePose(id)` / `GetModelBounds(id)` | Leader까지 해석한 model-space Pose / Morph를 포함한 bounds 값 반환 |
| `Scene::SetSocket(id, socket)` / `GetSocket(id, name)` | 인스턴스 소켓 정의/override 및 유효 정의 조회. 인스턴스 정의가 공유 정의보다 우선함 |
| `Scene::RemoveSocket(id, name)` | 인스턴스 정의만 삭제함. 공유 정의가 있으면 복원하며, 대체 정의 없이 부착 중이면 거부함 |
| `Scene::GetSocketWorldTransform(id, name)` | 최종 Pose·객체 계층·socket offset을 포함한 world 행렬 |
| `Scene::CreateAttachmentNode(parent, socket, relative={}, name={})` | 모델 없는 부착 노드 생성. 아래에 모델·하위 노드 배치 가능 |
| `Scene::AttachToSocket(id, parent, socket, relative={})` | 기존 객체를 지정 소켓에 부착. 기본 상대 TRS는 identity로 snap함 |
| `Scene::DetachFromSocket(id, keepWorld=true)` | Scene root로 분리. false는 현재 상대 TRS 유지, true는 world TRS 보존 |
| `Scene::SetWorldTransform(id, world)` | 일반 부모 또는 애니메이션 socket frame을 포함하여 local TRS 계산 |
| `AnimationPlayer::GetBounds(pose)` | 지정한 model-node 순서 Global 행렬과 해당 Player의 Morph weight로 bounds 계산 |
| `InspectJoint(scene, id, joint)` | Leader를 포함한 local/global·해당 메시의 skin palette 진단 (`minvex::debug`) |

공유 매핑은 `IsJoint=true` 본의 유일한 이름, 가장 가까운 부모 본 이름과 model-space 기준 행렬을 검사함. 기준 행렬 오차는 요소별 `1e-5 × max(1, abs(a), abs(b))` 이내여야 함. 노드/팔레트 순서 차이와 조상 본을 유지한 부분 Skeleton은 허용함. Follower가 사용하는 모든 본은 Leader에도 있어야 하며 미등록 추가 본·다른 기준 자세·계층 변경은 거부함. 이 API는 리타기팅을 수행하지 않음. 메시 고유의 일반 노드는 Follower 기준 local 행렬을 유지하며, 매핑된 부모 본 아래에 있으면 함께 움직임.

```cpp
#include <minvex/Scene.h>

// scene과 유효한 ModelHandle bodyModel/clothingModel/weaponModel을 준비한 뒤:
auto shared = lv::CreateSharedSkeleton(bodyModel->Skeleton, {{"Weapon", "RightHand"}});
auto body = scene.AddModel(bodyModel);
auto clothes = scene.AddModel(clothingModel, {}, body);
scene.SetSharedSkeleton(body, shared);
scene.SetSharedSkeleton(clothes, shared);
scene.SetLeaderPose(clothes, body);
auto slot = scene.CreateAttachmentNode(clothes, "Weapon", {}, "Weapon slot");
scene.AddModel(weaponModel, {}, slot);
scene.GetAnimation(body).Play(0);
scene.UpdateAnimations(.016); // Socket 갱신 호출은 필요 없음
auto finalPose = scene.EvaluatePose(clothes);
scene.DetachFromSocket(slot); // world TRS를 보존하여 root로 분리
scene.AttachToSocket(slot, clothes, "Weapon"); // identity offset으로 다시 장착
```

Leader 관계와 Transform 부모 관계는 별개임. 의상이 Body world transform을 따르려면 부모도 Body로 설정함. Bone matrix는 각 객체의 model-space 기준이며 Body의 world transform을 중복 적용하지 않음. 부착 행렬은 `parentWorld × resolvedPose.Global[joint] × socket.LocalTransform × child.LocalTransform`이고 하위 자식에도 누적됨. 일반 `SetParent()`는 소켓 부착을 해제하고 현재 local TRS를 유지함. 부모의 Visible=false는 부착 subtree도 숨기지만 Transform 부모가 아닌 숨겨진 Leader는 보이는 Follower를 계속 구동함.

Follower는 자체 애니메이션 Player의 TRS·Morph 재생과 이벤트 수집을 멈추고 Leader의 최종 Clip/Cross-fade/Blend Tree → Additive → 수동 override 결과를 사용함. Follower Player는 dormant 상태로 보존되며 직접 수정은 독립 상태에만 적용됨. Follower의 Morph weight 조회·수동 override, 재질, geometry, inverse bind는 자체 모델을 사용함. Leader 연결/해제는 Follower의 pending event queue를 비우며 과거 이벤트를 복제하지 않음. 해제 후 다음 Scene Update부터 보존된 독립 Player가 다시 진행함. Leader chain은 허용하고 self/순환/다른 Scene ID는 거부함.

`CollectRenderItems()`는 한 번의 호출 안에서 Leader Pose를 한 번 평가하고 같은 모델의 Follower는 같은 `RenderItem::Pose` snapshot을 공유함. 다른 노드 순서의 Follower는 매핑된 snapshot을 생성함. snapshot은 `shared_ptr<const EvaluatedPose>`가 소유하므로 Scene 수정/삭제 후에도 유지할 수 있음. `RenderItem::Animation`·재질 포인터와 `GetSocket()` 참조는 Scene 편집/삭제 전까지 유효하며 장기 보관 시 복사함. Scene은 모델·공유 Skeleton handle을 강하게 보유하고 모델 원본·공유 Skeleton·Clip을 수정하지 않음. 같은 Scene/Player의 동시 수정·평가는 금지하며 불변 공유 정의는 다른 Scene에서 동시 조회/바인딩할 수 있음.

소켓 이름·대상 노드 이름은 NUL 없는 1~256 bytes임. 인스턴스 정의와 공유 정의는 각각 최대 256개임. 공유 소켓은 본을 대상으로 하며 해당 정의에 바인딩된 각 모델에 존재해야 함. 공유 정의 자체는 불변이고 인스턴스 override로 조정함. 부착 subtree 삭제는 일반 Scene subtree 삭제와 같으며, 삭제된 Leader를 참조하는 다른 subtree의 살아 있는 Follower는 독립 Player로 복귀함. 모델 교체/재임포트는 소켓·Shared Skeleton·Leader 본 요구와 기존 Animation 상태를 먼저 검증한 뒤 적용하며 실패 시 모든 인스턴스를 유지함.

잘못된 입력·누락된 소켓/본·호환성·순환·singular TRS는 `std::invalid_argument`임. world-preserving detach 또는 SetWorldTransform 결과에 shear가 있으면 TRS로 저장할 수 없어 변경 전에 거부함. `EditorDocument`는 공유 Skeleton·Leader·소켓·부착 API를 같은 인수로 제공하고 Result 오류 및 Undo/Redo를 적용함. 전체 Pose 저장은 Follower의 표시되는 최종 local TRS를 저장하며 local shear는 거부함. overrides-only 저장은 dormant 독립 Player의 override임. Viewer는 owner thread에서 `GetScene()`을 통해 사용함.

Scene writer는 v13이며 reader는 v1~v13임. 최대 1024개의 공유 Skeleton 정의를 중복 없이 저장하고 본/기준 자세/공유 소켓, 인스턴스 소켓 override, Leader·Transform 부모·부착 소켓을 복원함. 파일의 ObjectId는 새 런타임 ID로 매핑하며 모순된 참조·순환·256단계 초과 chain은 거부함. v1~v11에서는 새 관계가 없는 일반 객체로 읽음. 기존 16 MiB JSON·65536 객체·4096 모델 제한을 유지함. 이전 SDK reader는 v13를 읽을 수 없음. Pose v1·ImportVersion 6은 유지하고 정적 SDK 소비자는 1.0.0 헤더/라이브러리로 함께 재빌드해야 함.

예제는 [Equipment](../examples/Equipment/Main.cpp), 설치 SDK 소비자는 [SdkConsumer/Equipment](../examples/SdkConsumer/Equipment.cpp)임. C++/Editor 기능을 제공하며 Python/JavaScript의 장착 제어 바인딩은 제공하지 않음.

### PBR Material

`MaterialAsset::Shading`은 `ShadingModel::Basic/Unlit/Pbr` 중 선택함. 기존 `Unlit=true` 플래그가 우선함. `MetallicFactor/RoughnessFactor`는 [0,1]이며, BaseColor texture는 sRGB·MetallicRoughnessTexture의 G/B는 linear로 처리함. 계산 시 roughness는 최소 .045로 제한함.

아래 코드는 유효한 `viewer`와 `ObjectId id`가 있는 함수 안에서 사용함. 공유 Asset 대신 객체의 재질 override를 수정함.

```cpp
auto material = viewer.GetScene().GetMaterial(id, 0);
material.Shading = lv::ShadingModel::Pbr;
material.MetallicFactor = .8f;
material.RoughnessFactor = .3f;
auto changed = viewer.SetMaterial(id, 0, material); // Result<void> 확인 필요
// viewer.ResetMaterial(id, 0)으로 원본 복원
```

렌더링에는 Advanced lighting과 `QualityOverrides::Pbr=true`가 필요함. Low/Medium은 기본 OFF, High는 ON임. `QualityCapabilities::PbrLighting`을 확인하며 미지원은 거부하고 AllowFallback일 때만 Basic 재질 경로로 대체함. 직접광 Directional/Point/Spot/Rect, Normal Map·Skinning·Instancing·Sun Shadow를 사용하며 IBL은 제공하지 않음.

`Scene::SetMaterial/ResetMaterial`과 `EditorDocument::SetMaterial/ResetMaterial`도 제공함. SetMaterial은 재질 전체 값을 override하며 ResetMaterial로 원본을 복원함. override가 있는 객체는 개별 draw로 처리함. Document 편집은 Undo/Redo 대상이며 Scene v13에 저장함. Texture는 같은 모델 내부 index만 참조함. Reimport 시 override material index/name·texture index 호환성을 검사함. PBR 설정은 현재 Quality writer v2로 저장하며 reader는 v1~v2를 지원함. 정적 SDK 소비자는 공개 구조체 변경 시 헤더/라이브러리를 함께 재빌드해야 함.

### Morph Target

`MeshPrimitive::MorphTargets`가 POSITION/NORMAL/TANGENT delta를 소유하고, `MeshAsset::MorphWeights`·`ModelNode::MorphWeights`가 기본값을 제공함. Node 값이 없으면 Mesh 기본값을 사용함. 공유 Asset을 바꾸지 않고 Player별·Node별 override를 적용함.

```cpp
auto& player = viewer.GetScene().GetAnimation(id);
auto weights = player.EvaluateMorphWeights(node); // 유효한 Morph mesh node ID
weights[0] = .7f;
auto changed = viewer.SetMorphWeights(id, node, weights); // Result<void> 확인 필요
// viewer.ClearMorphWeights(id, node)로 animation/default 값 복원
```

`AnimationPlayer::SetMorphWeights/ClearMorphWeights`, `Viewer`·`EditorDocument`의 같은 이름 Facade를 제공함. `ClearMorphOverrides()`는 전체 수동 weight를 지움. `MorphOverrides()`는 수동 상태, `EvaluateMorph(primitive, weights)`는 CPU 기준 MeshData를 반환함. weight는 각 target의 독립 가중치이며 합을 정규화하지 않음. 유한한 [-100,100]과 정확한 target 수를 요구함. ResetPose는 Morph override도 지우고, ClearOverrides/독립 Pose 파일은 Bone TRS만 다룸.

Mesh당 최대 8개 target이며 `ImportSettings::MaxMorphTargets`로 더 낮출 수 있음. 초과 입력은 ResourceLimit으로 거부하며 자르지 않음. Morph scalar sample도 MaxAnimationSamples에 포함하고 delta는 MaxResourceBytes 예산에 포함함. glTF sparse delta·Mesh/Node 기본값·STEP/LINEAR weight animation, FBX progressive shape의 target별 변환과 지정 bake rate를 지원함. FBX TRS/Morph는 같은 float key 시간표를 사용하고 반올림으로 겹친 key는 끝 값을 보존하며 합침. 원본 double duration은 유지하고 key가 duration을 넘지 않도록 처리함. CUBICSPLINE·geometry cache는 미지원임.

평가 순서는 Clip/Cross-fade/Blend Tree → 수동 weight override → GPU Morph → GPU Skinning임. Morph cross-fade도 live/frozen source·Pause·역재생을 따름. NORMAL/TANGENT를 정규화하고 누락 채널은 base 값을 유지함. GPU는 정점 remap과 일치하는 공유 storage buffer를 사용하며 Morph Mesh의 reduced LOD/instancing은 사용하지 않음. Bounds·Picking/Focus·Culling·Shadow·On-Demand·자원 비용에 변형을 반영함. GPU packed delta 비용은 정점×target×48 bytes임.

Scene v13는 node별 override와 frozen source Morph, layout signature를 저장함. 사용 중인 node/mesh·target 이름/순서·vertex 수·기본값 변경은 Reimport에서 거부함. Pose v1은 Morph를 저장하지 않음. 반환된 override 참조는 Player/Scene 편집 후 다시 조회하며 기존 owner-thread·오류 계약을 따름. `--morph` 렌더링 예제와 `examples/assets/MorphRibbon.gltf`를 제공함.

### 프레임 제어

[Quality.h](../include/minvex/Quality.h)의 `RenderScheduler`는 caller thread에서 사용하는 CPU 값 타입임. `Due(now, targetFps)`로 제출 시점을 확인하고 `NeedsRender(mode, signature, dynamic)`로 On-Demand 갱신 필요 여부를 판단함. 실제 제출 뒤 `Submitted(now, signature)`를 호출하고 내용이 바뀌면 `Invalidate()`로 다음 프레임을 요청함. 시간은 유한한 0 이상 초, TargetFps는 0~240이며 0은 제한 없음임.

Viewer·Embedded의 `RequestRedraw()`는 다음 렌더링 가능 시점에 On-Demand 화면을 갱신합니다. Embedded에서 즉시 갱신이 필요하면 해당 `Render()` 호출의 `EmbeddedFrameOptions::ForceRedraw`를 설정합니다. `VSync`는 화면 동기화 요청이고 `TargetFps`는 새 장면 렌더링 빈도 제한입니다. Viewer에서 `VSync=true`와 `options.Quality.Overrides.TargetFps=0`을 함께 사용하면 디스플레이 주기에 맞춰 새 장면을 렌더링할 수 있습니다. Standalone에서 `OnFrame`과 `FrameLimit`은 새 장면 프레임을 기준으로 동작합니다. Embedded의 호출·Present 주기는 Host가 관리하며 실제 표시 간격은 OS·주사율·GPU 부하의 영향을 받습니다.

### Ambient / Point / Spot / Rect 조명

`Scene`는 `Sun`, 독립 `Ambient`, `PointLights / SpotLights / RectLights`와 `LocalShadowResolution`을 소유함. Scene과 같은 호출 스레드에서 수정하며 `ValidateLighting()`은 잘못된 값에 `std::invalid_argument`를 던짐. 렌더링·저장·Editor의 Result Facade도 이 검증을 수행함. 빛의 위치·방향은 world space이며 SceneObject의 부모/Animation을 자동으로 따르지 않음. 호스트가 frame callback에서 갱신할 수 있음.

실행 중인 Viewer/Embedded의 품질 적용에 잘못된 Scene 조명 값을 전달하면 `InvalidArgument`이며 이전의 유효한 품질과 렌더링 자원을 유지함. 잘못된 입력을 수정한 뒤 다시 적용하거나 렌더링할 수 있음.

| 타입 | 계약 |
|---|---|
| `AmbientLight` | `Color={1,1,1}`, `Intensity=0`. 전역 diffuse ambient이며 IBL·AO를 포함하지 않음. `Sun.Ambient`의 회색 계수와 더해짐. 독립 Ambient만 사용할 때는 `Sun.Ambient=0`으로 지정함 |
| `PointLight` | `Position`, `Color`, `Intensity=1`, `Range=10`, `Shadow`. 전 방향 조명이며 거리 `d`의 감쇠는 `max(1-d/Range,0)^2/(1+d*d)`임 |
| `SpotLight` | Point와 같은 위치·색·감쇠, `Direction={0,-1,0}`, `InnerConeRadians=.35`, `OuterConeRadians=.7`. 반각은 `0 <= inner <= outer < pi/2`, outer는 0보다 커야 함 |
| `RectLight` | `Position`, `Direction={0,-1,0}`, `Up={0,0,1}`, `Color`, `Intensity=1`, `Width=2 / Height=2`, `Range=10`, `TwoSided=false`. Width/Height는 전체 길이이며 0보다 크고 1,000,000 이하. Direction/Up은 유한하고 서로 독립인 비영 벡터여야 함 |

색·강도는 유한한 비음수이며 RGB는 linear임. 위치는 유한해야 하고 Range는 유한한 양수임. Intensity는 SDK의 RGB 강도 스케일이며 lumen/candela 단위로 교정한 계약은 아님. Point+Spot+Rect 합계는 `MaxLocalLights=8`, 그중 Rect는 `MaxRectLights=4` 이하임. Point/Spot/Rect 렌더링에는 Advanced lighting이 필요하며 Ambient와 Sun은 Basic에도 적용함. Unlit 재질은 조명의 영향을 받지 않음.

Rect는 균일한 사각형 방출면의 [LTC 면적광](https://github.com/selfshadow/ltc_code)을 사용함. diffuse와 GGX specular를 근사 적분하며, specular에는 `ShadingModel::Pbr`와 PBR 품질이 필요함. emission 방향 앞쪽에 빛을 내고 `TwoSided`로 양쪽을 선택함. 방출면의 가장 가까운 점과 수신점 사이의 거리에 따라 `max(1-d/Range,0)^2`로 줄어듦. Renderer마다 64×64 RGBA16F lookup texture 두 개, 총 64 KiB를 필요한 동안 보관하며 모든 메시가 공유함. `QualityCapabilities::RectAreaLighting`으로 filterable RGBA16F 지원을 확인함. Rect shadow·광원 texture·IES는 제공하지 않음.

`LocalShadowSettings`는 Point/Spot에 속함. `Enabled=false`, `Filter=LocalShadowFilter::Pcf`, `NearPlane=.05`, `DepthBias=.005`, `SlopeBias=.02`가 기본값임. Filter는 Hard 또는 Pcf, near는 유한한 양수이며 Enabled이면 Range보다 작아야 함. bias는 유한한 비음수 **world distance**이며 Sun의 정규화 depth bias와 단위가 다름. 그림자를 켠 Point/Spot은 합계 `MaxShadowedLocalLights=4` 이하임. 사용 장면의 world scale에 맞게 near와 bias를 설정함.

Native local shadow에는 Advanced lighting, `Quality.Shadow != Off`, `QualityCapabilities::LocalShadowMapping`이 필요함. Point는 6개의 projective 방향, Spot은 1개의 perspective 방향을 공용 R32F+depth atlas에 배치하고 radial depth를 비교함. `Scene::LocalShadowResolution`은 각 면의 크기이며 128/256/512/1024, 기본 512임. Hard는 1회, Pcf는 3×3 비교이며 Point의 PCF는 인접 면으로 sample을 다시 투영함. Sun의 PCSS 선택은 local Filter를 바꾸지 않음. 조명 수·해상도로 인한 atlas와 렌더 타깃 합계가 기기 한도/attachment budget을 넘으면 `InsufficientResource`이며 해상도나 shadowed light 수를 줄여야 함. `LocalShadowResolution`과 local Filter는 Scene v13에 저장하고 Quality v2에는 저장하지 않음.

정적인 shadow map은 재사용하며 카메라·빛 색·강도·filter/bias만 바꾸면 다시 그리지 않음. 켜짐/꺼짐 또는 강도 0 전환은 atlas 구성을 바꿀 수 있음. 물체/pose/Morph/alpha material과 빛의 위치·방향·range·near가 바뀌면 해당 map을 갱신함. 같은 frame의 world/pose snapshot을 여러 shadow pass에서 공유하며 각 면의 frustum으로 caster를 걸러냄. 개별 map 갱신과 Skin/Morph·Alpha Mask·정적 instancing을 지원함. Scene 모델 자산은 다른 렌더링 경로와 같이 immutable 계약을 따름.

`EstimateQualityResources()`의 `QualityEstimate::LocalShadowFaces / LocalShadowResolution`은 atlas 배치 정보임. `Cost.RenderTargetBytes`에는 atlas의 R32F+depth와 빈 타일을 포함한 할당 면적을, `Cost.TextureBytes`에는 필요한 LTC lookup 64 KiB를 포함함. `current`에는 이전 추정 결과를 그대로 전달하며 이 필드와 Cost를 따로 수정하지 않음. `ReusedBytes / AdditionalBytes / TemporaryPeakBytes`는 이전 자원이 살아 있는 품질 전환의 논리 크기이며 driver 정렬·지연 해제·실제 VRAM 사용량은 아님.

Windows의 내장 shader는 빌드 때 DXBC로 컴파일함. macOS는 Metal source 경로를 유지하여 full Xcode 설치를 추가 요구하지 않음. 기기별 pipeline 초기화 비용과 native 실행 결과가 동일하다고 보장하지 않음. WebGL2는 local shadow를 제공하지 않음.

```cpp
auto& scene = viewer.GetScene();
scene.Sun.Ambient = 0;
scene.Ambient = {{.2f,.3f,.5f}, .25f};
lv::PointLight point;
point.Position = {0,2,0};
point.Shadow.Enabled = true;
scene.PointLights = {point};
lv::RectLight rectangle;
rectangle.Position = {0,3,0};
rectangle.Width = 3;
rectangle.Height = 2;
scene.RectLights = {rectangle};
scene.LocalShadowResolution = 256;
auto quality = lv::MakeQualityProfile(lv::QualityPreset::Medium);
auto result = viewer.SetQuality(quality); // Result<void> 확인
```

Editor Inspector는 Ambient와 Point/Spot/Rect의 편집·추가·삭제, local shadow 설정·해상도를 제공함. `EditorDocument::SetAmbientLight`, `SetLocalLights(points,spots,rects)`, `SetLocalShadowResolution`은 Undo/Redo와 실패 시 상태 보존을 제공함. 두 인자의 `SetLocalLights(points,spots)`는 기존 Rect를 유지함. Python은 같은 값 타입과 `viewer.scene.ambient / point_lights / spot_lights / rect_lights / local_shadow_resolution`을 제공하며 getter는 독립 복사본임. 수정한 복사본을 property에 다시 대입함. Web의 JavaScript binding에는 별도 조명 편집 API가 없음.

### PCSS Shadow

단일 Directional Sun의 직교 그림자에 blocker search → 거리별 penumbra → 가변 반경 PCF를 적용함. R32F raw map을 재사용하며 local shadow는 위의 독립 Hard/PCF atlas를 사용함. cascade는 제공하지 않음. Low는 Off, Medium은 Basic, High는 PCF 기본값을 유지하고 Custom에서 PCSS를 선택함.

| RenderSettings / QualityOverrides | 기본값 · 유효 범위 |
|---|---|
| `ShadowSourceAngle` | 전체 광원 각도 .535°, [0,20]°임. 0은 Basic과 같은 hard shadow임 |
| `ShadowBlockerSamples / ShadowFilterSamples` | 16 / 32, 각각 정수 [1,64]임 |
| `ShadowMaxFilterRadius` | 32, shadow-map texel 단위 [1,64]임 |
| `ShadowDepthBias / ShadowSlopeBias` | .0008 / .003, 정규화된 map depth 단위 [0,.1]임. Basic/PCF에도 적용함 |

```cpp
auto quality = lv::MakeQualityProfile(lv::QualityPreset::Medium);
quality.Overrides.Shadow = lv::ShadowMode::Pcss;
quality.Overrides.ShadowSourceAngle = 2.f;
quality.Overrides.ShadowBlockerSamples = 16;
quality.Overrides.ShadowFilterSamples = 32;
auto changed = viewer.SetQuality(quality); // Result<void> 확인 필요
// lv::SaveQuality(quality, "SoftSun.mvxquality.json")로 독립 저장함
```

직교 map의 선형 깊이를 world 거리로 변환하고 거리×tan(광원 각도/2)로 반경을 구함. 깊이의 perspective 나눗셈은 사용하지 않음. 고정 disk 샘플과 수신면 깊이 보정으로 프레임 간 패턴과 경사면 자기 그림자를 관리하며 맵 밖은 lit로 처리함. blocker가 없으면 완전한 lit, 최대 반경은 texel 기준으로 제한함. 동일한 Skinning·Morph·Instancing·Alpha Mask shadow pass를 사용하며 추가 attachment 메모리는 필요하지 않음.

`DebugView::ShadowBlockers / ShadowBlockerDepth / ShadowPenumbra / ShadowVisibility`는 각각 blocker 수/검색 수, 수신면 기준으로 보정한 평균 선형 깊이, 반경/최대 반경, 최종 가시성의 grayscale임. blocker가 없으면 깊이=1·반경=0임. 앞의 세 뷰는 PCSS, Visibility는 활성 shadow가 필요하며 부적합한 뷰/품질 전환은 `UnsupportedFeature`이고 이전 상태를 유지함. Editor Quality Settings와 Debugger에서 같은 설정·뷰를 제공하며 `minvex_rendering_enhancement --pcss`는 접촉·부유 그림자 예제임.

`ExpandQuality`는 비유한 값·범위 초과를 `InvalidArgument`로 반환함. `ResolveQuality`는 `QualityCapabilities::ShadowMapping`을 요구하고 `AllowFallback=true`인 경우만 shadow를 끔. 설정은 값으로 복사하며 Viewer/Embedded/Editor 적용은 기존 owner/graphics thread에서 pass 밖에 호출함. 변경은 On-Demand 이미지를 무효화함. Quality writer v2는 PCSS 키를 저장하고 reader는 v1~v2임. v1에 없는 값은 기본값이며 정적 SDK 소비자는 공개 구조체 변경 시 함께 재빌드해야 함.

### Python Binding

`import minvex as mvx`로 기존 C++ 엔진을 사용함. CPython 3.12·Windows/macOS 대상이며 `MINVEX_BUILD_PYTHON=ON`일 때만 pybind11을 요구함. GPU resource handle·Shader·Embedded Host는 Python에 노출하지 않음. 기본 C++ 구성은 Python 의존성이 없음.

```sh
python -m pip wheel . --wheel-dir out/wheels
python -m pip install --force-reinstall out/wheels/minvex-1.0.1-cp312-cp312-win_amd64.whl
python python/examples/viewer.py examples/assets/MorphRibbon.gltf --frames 60
```

루트 `pyproject.toml`의 scikit-build-core 빌드이며 CMake·C++20 도구와 플랫폼 SDK가 필요함. Windows는 x64 MSVC 런타임을 사용함. macOS wheel 파일명은 빌드 환경에 따름. 기본 wheel은 `MINVEX_BUILD_RENDERER=ON`으로 구성함. `python -m pip wheel . --wheel-dir out/wheels-cpu -C cmake.define.MINVEX_BUILD_RENDERER=OFF`로 CPU-only wheel을 만들 수 있으며 이 구성의 `run()`은 `UnsupportedFeature`임. Native/CPU wheel의 버전과 파일명은 같으므로 출력 폴더를 구분하고, 같은 환경에 교체 설치할 때는 `--force-reinstall`을 사용함. PyPI 공개 설치를 전제로 하지 않으며 위 명령은 로컬 wheel 생성·설치임.

```python
import minvex as mvx

with mvx.Viewer() as viewer:
    model = viewer.add_model("character.glb")
    model.play_animation("Walk")  # index도 가능함
    quality = mvx.make_quality_profile(mvx.QualityPreset.Medium)
    quality.overrides.pbr = True
    viewer.set_quality(quality)
    material = model.get_material(0)
    material.shading = mvx.ShadingModel.Pbr
    material.roughness_factor = .3
    model.set_material(0, material)
    viewer.run(on_event=lambda e: print(e.occurrence.event.name))
```

| 대상 | 주요 API |
|---|---|
| `Viewer` | `ViewerOptions`, `add_model`, `scene`, `set_quality`, `quality`, `set_debug_view`, `frame_scene`, `request_redraw`, `run`, `request_close`, `is_running`, `close` |
| `Scene / Object` | `create_object`, `objects`, `object(id)`, `camera/sun`, `save/load`, `update_animations`; Object의 `id/name/transform/visible`, `set_parent`, `remove` |
| Animation / Blend Tree | `animation_clips`, `play_animation`, `pause/resume/stop_animation`, `seek_animation`, `animation_speed/loop/time`, `create_blend_tree(BlendTreeDesc)`, `play_blend_tree`, `set_blend_parameter`, `seek_blend_tree`, `blend_weights` |
| Pose / Additive | `skeleton_nodes`, `reference_pose`, `capture_pose`, `apply_pose/reset_pose`; `set/restore/clear_additive_animation`, `additive`, `set_additive_weight/mask`, `seek_additive` |
| Morph / Material | `morph_nodes`, `get/set/clear_morph_weights`; `get/set/reset_material`, `Material`의 PBR·texture·alpha 필드 |
| Quality / Event | `make_quality_profile`, `QualityProfile.overrides`, `expand/save/load_quality`; `AnimationEvent`, `set/reset_animation_events`, `poll/dispatch_animation_events`, `clear_animation_events` |

`Vec3`·`Quaternion`·`Transform`·Camera·Material·Quality·프레임/이벤트는 값 타입임. `model.transform`·`get_material()`·`scene.camera` 조회값을 수정한 뒤 다시 대입/설정해야 반영함. Clip 이름이 중복이면 index를 요구함. Morph node ID와 기본/현재 weight는 `morph_nodes`로 조회함. PBR 렌더링에는 C++과 같이 Advanced lighting·Pbr 품질 설정이 필요함.

`set_additive_animation(clip, weight=1, reference=Pose(), mask=[])`는 TRS 레이어 1개를 설정함. 빈 reference는 Skeleton reference이며 `reference_pose`·`capture_pose()` 또는 `Pose/PoseJoint`로 full/partial 기준을 지정함. `skeleton_nodes`는 node/name/parent/editable/is_joint 복사본이며 root parent는 None임. 비어 있지 않은 mask는 읽기 전용 노드도 포함한 전체 노드 수와 길이가 같아야 함. `additive`는 없으면 None, 있으면 독립 `AdditiveAnimationState` 복사본임. 수정한 time/speed/loop/weight/state/reference/joint_mask는 `restore_additive_animation(state)`로 검증·반영함. `Pose.joints`도 목록을 수정한 뒤 재대입함. 잘못된 mask·기준 Pose·시간은 기존 레이어를 유지하며 `InvalidArgument`임. Pause/Resume/Stop·수동 Pose override 순서는 C++ 계약과 같음.

Viewer가 C++ 소유자이며 Scene/Object는 약한 핸들임. Viewer 소멸·`close()` 후 접근은 `InvalidState`, 제거된 객체 접근은 `InvalidHandle`임. id는 식별자 값으로 유지됨. Scene load는 ID를 재매핑하고 이전 핸들을 무효화하며 `run/dispatch` 밖에서 호출함. `close()`는 owner thread에서 반복 호출 가능하고 실행/dispatch 중에는 `InvalidState`임. 실행 중에는 `request_close()` 후 run 반환 뒤 닫음.

생성 스레드에서 모델/Scene/애니메이션을 사용하며 위반은 `WrongThread`임. `request_close/is_running`만 다른 스레드에서 사용 가능함. macOS `run`은 main thread임. Blocking `run`·Import·파일 IO 중에는 GIL을 해제하고 Python callback은 owner thread에서 GIL을 확보함. `on_frame(viewer, info)` 뒤 `on_event(event)`를 전달하며 callback은 run 동안만 유지됨. 프레임·이벤트 복사본은 callback 반환·객체 삭제 후에도 유효함. 자동 애니메이션 진행을 끄려면 `advance_animations=False`임.

`mvx.Error`는 `RuntimeError` 계열이며 `code/message/hint/source_path`를 제공함. C++ Result·검증 예외를 변환하고 Python callback 예외는 `CallbackFailure`·`__cause__`로 원인을 보존함. Python 인수의 타입 오류는 `TypeError`일 수 있음. 이벤트 polling·수집 Seek·큐 배출·1회 전달은 기존 C++ 계약과 같으며 Python 객체를 저장 형식에 직렬화하지 않음.

### WebAssembly Binding

Emscripten 4.0.10·Embind·Sokol WebGL2의 별도 Web Host임. `MINVEX_BUILD_WEB`은 Emscripten에서 기본 ON이며 Native SDK·Python 구성은 기본 OFF임. Native Editor·WebGPU·pthread는 포함하지 않으며 COOP/COEP 없이 main thread에서 실행함. Fetch는 비동기이며 Import·Animation 평가는 C++ main thread 작업임.

셰이더 컴파일러 `sokol-shdc`는 빌드 호스트 기준으로 Windows·macOS·Linux x86_64/arm64에서 선택하며 고정 revision·SHA-256을 검증함. Emscripten 타깃 아키텍처와 구분함. 다른 호스트 또는 오프라인 빌드는 `MINVEX_SOKOL_SHDC`로 실행 파일을 지정함.

```sh
emcmake cmake -S . -B out/build/web -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build out/build/web --target minvex_web --parallel 4
python -m http.server 8000 --directory out/build/web/web
```

브라우저에서 `http://localhost:8000`의 파일 선택 예제를 실행함. 배포 파일은 `minvex-native.js/.wasm`, `minvex.js`, `index.html`임. HTTP(S)에서 Wasm MIME을 `application/wasm`으로 제공함. `cmake --install out/build/web --prefix out/web-sdk --component web`으로 라이선스와 함께 설치함.

```javascript
import {createViewer} from './minvex.js';
const viewer = await createViewer({canvas: document.querySelector('canvas'), quality: 'low'});
const model = await viewer.loadModel('./robot.glb');
if (model.info.clips.length) model.playAnimation(0);
// 종료 시 프레임·Fetch·입력·GPU·Embind 객체를 함께 해제함
viewer.dispose();
```

| 대상 | 주요 API |
|---|---|
| Viewer | `createViewer({canvas, quality, autoStart, onEvent, onError})`, `start/stop/render`, `setCamera/frameScene`, `setQuality/quality`, `pollAnimationEvents`, `dispose` |
| Asset | 비동기 `loadModel(url, options)`, `loadBuffer(bytes, {name, files, resolve})`, `loadFiles(FileList, {entry})`임. GLB·외부 glTF의 buffers/images·base64 URI를 지원함 |
| Object | `info`, `clone/remove`, `setTransform({position, rotation, scale})`, `setMaterial({baseColor, metallic, roughness, unlit}, index)`임. 배열은 Vec3·Quaternion·RGBA 순서임 |
| Animation | `play/pause/stop/seekAnimation`, `setAnimationSpeed`, `setMorphWeights/clearMorphWeights`, `playBlendTree`, `setBlendParameter/seekBlendTree`, `setAdditiveAnimation/setAdditiveWeight/seekAdditive/clearAdditiveAnimation`, `setAnimationEvents`임 |

`info`는 Transform·Clip·Node별 Morph weight·시간·Blend weight의 복사본임. Blend Tree description은 `kind: '1d'/'2d'`, `parameterX/parameterY`, `samples: [{clip, x, y}]`임. Additive의 `clip, weight=1, mask=[]`는 Skeleton reference를 기준으로 함. `seekAnimation(time, collect=false)`는 기본 무발생이며 이벤트는 `onEvent` 또는 polling으로 배출함. Clip·Node·Material index와 정수 품질 설정은 소수점·범위 초과 값을 `InvalidArgument`로 거부하며 다른 대상이나 적용 상태를 수정하지 않음.

메모리 입력은 임시 MEMFS에 복사한 뒤 기존 Importer를 호출하고 파일을 제거함. `files`는 URI→typed array의 Object/Map이며 `resolve(uri, absoluteURL, signal)`로 의존성을 제공할 수 있음. URL 로딩은 상대 URI를 원래 URL 기준으로 Fetch하고, File Input은 선택한 파일·상대 경로·유일한 basename으로 해석함. 네트워크/file URI를 C++ 파일 경로로 전달하지 않음. ArrayBuffer/typed array의 추가 소유 복사 전에 파일당 64 MiB·의존성 합계 128 MiB를 검사하며 초과는 `InsufficientResource`임. 이미지 최대 4096·정점 50만·Scene 객체 256개가 한도임. dispose 후 resolver가 완료되어도 다음 의존성 요청이나 데이터 복사·Import를 진행하지 않음.

WebGL2 Skinning은 최소 uniform 사양에 맞춰 Skin당 48 Joint이며 초과는 `UnsupportedFeature`임. Morph는 C++ 평가→객체·Node별 정점 stream→GPU Skinning 순서로 적용하며 공유 Asset의 weight를 바꾸지 않음. 직접광 PBR·Normal Map·Static Instancing·Frustum Culling을 재사용함. Canvas는 CSS pixel 기준임. `setCamera`는 position/target/up·projection·orthographicHeight·fovRadians·nearPlane/farPlane을 지원하고 입력은 드래그 Orbit·우클릭 Pan·휠 Zoom임.

품질은 preset 문자열 또는 `{preset, allowFallback, pbr, targetFps, resolutionScale, shadow, postProcessing, msaaSamples, onDemand}`임. Web 경로는 Shadow/PCSS·HDR·BC3 OFF, MSAA·anisotropy 1임. `allowFallback` 기본 true이며 High의 미지원 값은 조정 내역으로 반환함. false이면 거부하고 이전 설정을 유지함. 웹 PCSS는 지원 기능으로 포함하지 않음.

Viewer마다 독립 Wasm module·Canvas를 사용하며 같은 Canvas의 중복 소유는 거부함. 브라우저 GC에 GPU 해제를 맡기지 않고 `dispose()`를 호출함. Object는 약한 Viewer 참조이며 삭제 후 `InvalidHandle`, dispose 후 `InvalidState`임. 미완료 Fetch는 취소하며 File 읽기 중 dispose도 완료 후 `InvalidState`임. File 읽기 실패는 `AssetLoad`이며 cause를 보존함. context loss에서는 프레임을 멈추고 CPU Scene을 유지하며 복구 시 extension·GPU 자원을 재생성함. `MinvexError`의 `code/message/hint/sourcePath/cause`를 제공함. rAF 오류는 `onError` 또는 Canvas의 `minvexerror` 이벤트로 전달함. callback 실패는 `CallbackFailure`이며 이벤트 큐는 최대 1024개임. 초과 프레임은 오류로 중단하고 새 전달분은 저장하지 않으며 시간은 이미 진행된 상태임.

### Embedded Runtime

호출 순서는 `Initialize()` → 프레임마다 `Update()/Render()` → 종료 시 `WaitIdle()/Shutdown()`임. Host가 창·이벤트·입력·device·target·Present를 관리함. resize 전에는 GPU 완료 대기 → `DetachTarget()` → Host target 교체 순서를 따름.

같은 device의 단일 mip/slice/sample 2D target, 1~8192 framebuffer pixels를 사용함. Color는 RGBA8/BGRA8 linear/sRGB, D3D11 depth는 D32_FLOAT 또는 D32_FLOAT_S8X24_UINT임. D3D11.1 immediate context의 Host state를 복원함. Metal은 borrowed queue로 순서를 연결하거나 별도 queue 사용 전에 Host가 동기화함. Scene depth 공유·alpha 합성은 지원하지 않음.

실제 통합 코드는 [Windows Host](../examples/EmbeddedViewer/MainD3D11.cpp) · [macOS Host](../examples/EmbeddedViewer/MainMetal.mm)를 참고함.

## macOS Metal 실행 조건

macOS Metal 경로는 macOS 14.0 이상·Apple Silicon arm64를 대상으로 함. macOS preset은 `arm64`·`CMAKE_OSX_DEPLOYMENT_TARGET=14.0`을 지정함. Intel Mac용 바이너리는 제공하지 않음.

- **빌드:** `CMAKE_OSX_ARCHITECTURES=arm64`와 대상 환경에 맞는 macOS SDK·컴파일러·라이브러리를 사용함. 배포할 애플리케이션의 `CMAKE_OSX_DEPLOYMENT_TARGET`은 Host 프로젝트에서 설정함.
- **셰이더:** Metal 라이브러리는 런타임에 컴파일함. MSL 언어 버전을 명시적으로 고정하지 않음.
- **GPU 기능:** 초기화 후 `GetQualityCapabilities()`로 컬러·깊이·Shadow·HDR·BC3 포맷과 MSAA 지원 범위를 확인함. 품질 적용은 실제 컨텍스트의 capability와 `AllowFallback` 정책을 따름.
- **Embedded:** Host가 제공하는 `MTLDevice`·command queue·target의 GPU와 포맷을 사용함. owner-thread·target 수명·queue 동기화 계약을 충족해야 함.
- **Python:** macOS arm64용 CPython 3.12 wheel이 필요함. Windows용 wheel과 바이너리 호환되지 않음.

## 소유권과 스레드

| 대상 | 사용 규칙 |
|---|---|
| **Model / Animation** | `ModelHandle`은 immutable Asset의 강한 공유 참조. Player마다 시간·Pose·blend·Tree 파라미터/phase·Additive 상태를 독립 보유하며 동시 수정/평가 금지 |
| **Scene / EditorDocument** | 단일 호출 스레드 사용. Object 삭제·Scene 교체 후 기존 참조 사용 금지. Document 편집은 실패 시에도 snapshot 복원이 가능하므로 원소 참조를 다시 조회 |
| **Viewer** | 생성한 owner thread에서 사용. macOS Run은 main thread. `RequestClose()`·`IsRunning()`만 thread-safe. 프로세스당 Standalone Run 세션 1회 |
| **EmbeddedRenderer** | 생성·사용·Shutdown·파괴를 같은 render thread에서 수행. `IsInitialized()`만 thread-safe. Host 자원은 borrowed이며 사용 중 수명 보장 필요 |
| **그래픽 세션** | 프로세스당 활성 세션 하나. Standalone·Embedded·Editor를 동시에 초기화하지 않음 |
| **AssetLoader** | `Submit/Poll/Cancel/Forget`은 thread-safe. worker는 CPU Import만 수행하며 Scene 반영은 owner thread에서 명시적으로 처리. Cancel은 실행 중 파서를 강제 중단하지 않음 |

## 오류 처리

[Result.h](../include/minvex/Result.h)의 `Result<T>`는 성공 시 `Value()`, 실패 시 `Error()`로 조회함. `Error`는 `Code`, `Message`, `Hint`, `SourcePath`를 제공함. 실패한 Result의 Value 또는 성공한 Result의 Error 접근은 `std::logic_error`임.

lvalue Result의 `Value()`는 보관된 값의 mutable/const 참조임. rvalue Result의 `Value()`는 소유하는 값을 move하며 const rvalue는 copy함. 따라서 임시 Result가 사라져도 꺼낸 값 자체의 수명은 유지됨. const rvalue에서 꺼내려면 T가 copy 가능해야 함. `Error()`도 rvalue에서 값으로 꺼냄.

| 오류 | 의미 |
|---|---|
| `InvalidArgument` · `InvalidHandle` · `InvalidData` | 잘못된 인수·핸들·파일/데이터 |
| `InvalidState` · `WrongThread` | 호출 순서·수명·owner thread 위반 |
| `AssetLoad` · `SceneIo` · `QualityIo` · `TelemetryIo` | Import 또는 파일 입출력 실패 |
| `UnsupportedFeature` · `InsufficientResource` | 미지원 기능 또는 리소스 부족 |
| `GraphicsInitialization` · `Rendering` · `CallbackFailure` | 그래픽 초기화·렌더링·callback 실패 |

Facade·IO는 Result를 사용하고, low-level Asset/Scene/Animation API는 `ImportError`·`std::invalid_argument` 등의 예외를 던질 수 있음. 표준 할당 실패까지 항상 Result로 변환하는 계약은 아니므로 애플리케이션 경계에서 처리함.

## 저장 형식과 호환성

| 항목 | 현재 규칙 |
|---|---|
| **Scene** | writer v13 / reader v1~v13. 외부 모델 참조·계층·카메라·조명·Pose·Clip/Cross-fade·Blend Tree·Additive·Event overlay·재질/Morph override·Shared Skeleton·Leader Pose·Socket 저장. 로드 시 runtime ID 재매핑 |
| **Pose / Quality / Import** | Pose v1, Quality writer v2 / reader v1~v2, ImportVersion 6. Pose는 skeleton signature 호환 필요 |
| **Quality** | Low·Medium·High, Overrides가 있으면 Custom. 기본 `AllowFallback=false`; 미지원 설정 대체는 명시적으로 허용 |
| **SDK** | 정적 라이브러리. 같은 architecture·compiler/toolset·configuration·C++ runtime으로 소비자 재빌드 필요. 바이너리 ABI 비고정 |

Scene v13는 v12 이하만 지원하는 이전 SDK reader로 읽을 수 없음. Scene/Pose/Quality 저장은 임시 파일 작성 후 교체하며 모델 파일 자체를 Scene에 내장하지 않음. Undo 이력·UI 상태·Telemetry는 Scene에 저장하지 않음.

Scene의 모델·텍스처 디렉터리 상대 참조는 실제 부모 디렉터리를 기준으로 계산함. macOS의 `/var` → `/private/var` 같은 디렉터리 링크나 Windows junction을 통해 저장·로드해도 같은 리소스를 가리키며, 프로젝트를 함께 옮길 수 있음.

## 라이선스와 배포 고지

Minvex 자체 코드는 Apache-2.0을 적용함. SDK 설치 접두사 최상위의 `LICENSE`·`NOTICE`, Python wheel의 라이선스 메타데이터·파일, Web 설치 폴더의 `LICENSE`·`NOTICE`를 제공함. 외부 의존성과 외부 자산은 원래의 라이선스를 유지하며 이 선택으로 다시 라이선스하지 않음. 의존성별 원문과 `versions.json` 고지를 함께 확인함.

## 지원 범위

이 문서는 SDK 1.0.1의 공개 API를 설명합니다. 플랫폼별 실행 요구사항은 [README](../README.md#실행-요구사항)를 참고합니다.

CPU-only 구성의 native 초기화·실행은 `UnsupportedFeature`를 반환함.

Python wheel은 기본 renderer ON이며 CPU-only wheel은 `MINVEX_BUILD_RENDERER=OFF`로 구성함. Web은 Skin당 48 Joint·CPU Morph를 사용하며 Shadow/PCSS·HDR·BC3를 제공하지 않음. Asset Import는 문서화된 subset과 진단 경고를 따름. IBL·Emissive/Occlusion 렌더링·Alpha BLEND·Rect shadow는 지원하지 않음.
