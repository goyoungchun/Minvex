#pragma once
#include "SceneViewport.h"
#include "CameraController.h"
#include "NativeFiles.h"
#include "EditorPreferences.h"
#include <minvex/EditorDocument.h>
#include <minvex/AssetLoading.h>
#include <minvex/Terrain.h>
#if MINVEX_EDITOR_PHYSICS
#include "BoxPreview.h"
#endif

namespace lv::editor {
enum class ConsoleLevel { Info, Warning, Error };
struct ConsoleEntry {ConsoleLevel Level;double Time;std::string Message;std::filesystem::path Source;ObjectId Object=NoObject;};
enum class EditorFileAction { OpenModel, OpenScene, ImportAssets, SaveAs, NewProject };
enum class UnsavedChoice { Save, Discard, Cancel };
class EditorUI final {
public:
    EditorDocument Document;
    SceneViewport Viewport;
    void Draw(); // Inside an ImGui frame, outside graphics passes.
    void TickParticlePreview(double delta);
    void ShowParticleEditor() noexcept { particlePanel_ = particleFocusRequested_ = true; }
    void Shutdown() noexcept;
    void InitializeSession(std::filesystem::path preferences,std::filesystem::path sample,std::filesystem::path documentation,bool persist=true);
    void SetFilePicker(FilePicker picker) {picker_=std::move(picker);}
    void ChooseFiles(EditorFileAction action);
    void ResolvePending(UnsavedChoice choice);
    void RequestQuickOpen(const std::filesystem::path& path);
    void DropFiles(const std::vector<std::filesystem::path>& paths,float x,float y);
    void Record(ConsoleLevel level,std::string message,std::filesystem::path source={},ObjectId object=NoObject);
    const std::vector<ConsoleEntry>& ConsoleEntries() const noexcept {return consoleEntries_;}
    void Import(const std::filesystem::path& path, bool place);
    bool HasImportPreview() const noexcept {return !importQueue_.empty();}
    ModelHandle ImportPreviewModel() const noexcept {return importCandidate_;}
    void RefreshImportPreview(const ImportSettings& settings);
    bool ConfirmImportPreview();
    void CancelImportPreview();
    void PollAssetJobs();
    bool ImportPreviewPending() const noexcept {return previewJob_.has_value();}
    void SetHotReload(bool enabled) noexcept {hotReload_=enabled;}
    void RequestOpen(const std::filesystem::path& path);
    void RequestNew();
    void RequestQuit();
    bool SaveUi(const std::filesystem::path& path);
    void RequestLoadUi(const std::filesystem::path& path);
    void RequestClearUi();
    void ResolveUiPending(UnsavedChoice choice);
    bool HasPendingUi() const noexcept {return uiPending_!=UiPending::None;}
    void SetUiPreview(bool enabled) noexcept {uiPreview_=enabled;}
    bool QuitRequested() const noexcept { return quit_; }
    void SetDirectory(std::filesystem::path path) { directory_ = std::move(path); }
    void ShowError(const Error& error,ObjectId object=NoObject);
    void LoadQualityProfile(const std::filesystem::path& path);
    Result<void> ApplyQualityProfile(const QualityProfile& profile);
    const QualityProfile& QualityDraft() const noexcept {return qualityDraft_;}
    void SetQualityDraft(QualityProfile profile) {qualityDraft_=std::move(profile);}
    void ResetQualityDraft() {qualityDraft_=Viewport.Quality().Requested;}
    const std::optional<QualityEstimate>& SelectedQualityEstimate() const noexcept {return selectedEstimate_;}
    void ShowDebuggers() noexcept {renderDebugger_=animationDebugger_=true;}
    std::array<float,4> ViewportArea() const noexcept {return viewportRect_;}
    bool GizmoVisible() const noexcept {return gizmoVisible_;}
    int TransformTool() const noexcept {return transformTool_;}
    // Queue from UI actions after Image commands exist; commit at the next Draw entry.
    void RequestQualityProfile(QualityProfile profile) {pendingQuality_=std::move(profile);}
private:
    bool particlePanel_ = false, particlePlaying_ = true, particleEdit_ = false, particleFocusRequested_ = false;
    float particleSpeed_ = 1;
    ObjectId particleObject_ = NoObject;
    std::size_t selectedEmitter_ = 0;
    std::vector<SceneParticleEvent> particleEvents_;
    std::size_t particleDroppedEvents_ = 0;
    void ParticlePanel();
    void FinishParticleEdit(bool cancel = false);
    void UiDesigner();
    bool UiHasFocus() const;
    void FinishUiEdit();
    void UiUndo(bool redo=false);
    void ApplyUiPending();
    void UiUnsavedDialog();
    bool UiViewportInput(float x,float y,float width,float height,bool hovered,bool modal);
    bool EditUiNumber(UiElement& element);
    UiId uiSelected_=NoUi;
    bool uiDesigner_=true,uiPreview_=false,uiPointerDown_=false,uiNumberFocus_=false;
    UiId uiNumberObject_=NoUi;
    UiNumberInput uiNumberSource_,uiNumberDraft_;
    std::string uiPath_="screen.mvxui.json",uiFontPath_="Font",uiStatus_;
    std::vector<UiFont> uiFonts_;
    enum class UiPending { None,Load,Clear,Quit } uiPending_=UiPending::None;
    std::filesystem::path uiPendingPath_,uiSavedPath_;
    std::string uiSavePath_="screen.mvxui.json";
    bool uiAllowQuit_=false;
    enum class Action { None, New, Open, QuickOpen, Project, Quit } pending_ = Action::None;
    enum class Dialog { None, Import, Open, QuickOpen, Project, Save, PoseFull, PosePartial, PoseLoad, QualitySave, QualityLoad, TelemetryJson, TelemetryCsv } dialog_ = Dialog::None;
    std::filesystem::path directory_ = std::filesystem::current_path(), openPath_;
    std::string error_, status_ = "Open a model to begin.";
    NativeFiles nativeFiles_;
    FilePicker picker_;
    EditorPreferences preferences_;
    std::filesystem::path preferencesPath_,samplePath_,documentationPath_,projectRoot_;
    bool persistPreferences_=false,welcome_=true,console_=true;
    bool consoleInfo_=true,consoleWarnings_=true,consoleErrors_=true;
    std::vector<ConsoleEntry> consoleEntries_;
    std::string consoleFilter_;
    float assetRect_[4]{};
    ObjectId deleteObject_=NoObject;
    std::string filter_;
    std::string hierarchyFilter_;
    std::array<float,4> viewportRect_{};
    int transformTool_=0;
    bool worldTransformTool_=false, surfacePick_=false, surfacePanel_=false;
    std::vector<SurfaceHit> surfaceHits_;
#if MINVEX_EDITOR_PHYSICS
    bool boxPhysics_ = false, rigidBoxes_ = false, physicsOverlay_ = false;
    double gravitySeconds_ = 0.25, rigidSeconds_ = 0.25;
    BoxPreviewOptions boxOptions_;
    std::vector<BoxCollider> boxColliders_;
    std::optional<BoxPreview> boxPreview_;
    void BoxPhysicsPanel();
    void UpdateBoxOverlay();
#endif
    float crossFadeSeconds_=.3f;
    ObjectId equipmentObject_=NoObject, socketParent_=NoObject;
    SkeletonSocket socketDraft_{"Socket", ""};
    std::string attachmentDraft_;
    ObjectId treeObject_=NoObject;
    ModelHandle treeModel_;
    BlendTreeHandle treeDraftSource_;
    BlendTreeDesc treeDraft_;
    ObjectId additiveObject_=NoObject;
    ModelHandle additiveModel_;
    std::size_t additiveClip_=0,additiveReferenceClip_=0;
    int additiveReferenceKind_=0;
    double additiveReferenceTime_=0;
    ObjectId eventObject_=NoObject;
    ModelHandle eventModel_;
    std::size_t eventClip_=0,eventIndex_=0;
    bool collectEventSeek_=false;
    std::vector<SceneAnimationEvent> eventLog_;
    bool grid_=true,gizmoVisible_=false,gizmoBlockInput_=false,gizmoEditing_=false,gizmoSuppressed_=false;
    bool cameraEditing_=false;
    ObjectId gizmoObject_=NoObject;
    AssetId selectedAsset_ = 0;
    bool quit_ = false, resetLayout_ = false, cameraDrag_ = false;
    bool hierarchy_ = true, inspector_ = true, assets_ = true, viewport_ = true;
    bool bones_ = true, animation_ = true;
    bool quality_ = true;
    bool performance_ = true;
    bool assetInspector_=true,copyImport_=false;
    struct LocalityAnalysis {
        std::weak_ptr<const ModelAsset> Model;
        std::size_t Mesh = 0, Primitive = 0;
        MeshLocalityStatistics Stats;
    };
    std::optional<LocalityAnalysis> locality_;
    bool analyzeOverdraw_ = false;
    bool renderDebugger_=false,animationDebugger_=false;
    int inspectedTarget_=0;
    DebugDraw editorDebugDraw_;
    bool terrainPanel_ = false, terrainContours_ = false, terrainStroke_ = false;
    int terrainColumns_ = 65, terrainRows_ = 65, terrainTool_ = 0;
    int terrainSampleColumn_ = 0, terrainSampleRow_ = 0;
    double terrainSampleHeight_ = 0;
    TerrainLayout terrainLayout_;
    MountainPreset mountain_;
    double terrainInterval_ = 2, brushRadius_ = 4, brushStrength_ = 1;
    ObjectId terrainMeasured_ = NoObject, terrainStrokeObject_ = NoObject;
    std::vector<std::array<double,2>> terrainPoints_;
    std::array<double,2> lastBrush_{};
    ModelHandle contourModel_;
    std::array<float,16> contourTransform_{};
    double contourInterval_ = 0;
    bool contoursApplied_ = false;
    std::vector<std::array<double,2>> markersApplied_;
    void TerrainPanel();
    void ImportTerrain(bool png);
    void FinishTerrainStroke(bool cancel);
    bool TerrainInput(float x,float y,ViewportSize size,bool hovered,bool cancel);
    void UpdateTerrainContours();
    struct PendingImport {std::filesystem::path Path;bool Place=false;};
    std::vector<PendingImport> importQueue_;
    ImportSettings importSettings_;
    ModelHandle importCandidate_;
    std::optional<Error> importError_;
    std::unique_ptr<AssetLoader> loader_;
    std::optional<AssetLoadId> previewJob_;
    bool backgroundImport_=true,hotReload_=false;
    AssetHotReload watcher_;
    struct PendingReload {AssetLoadId Job;ModelHandle Before;bool Again=false;};std::vector<PendingReload> reloads_;
    QualityProfile qualityDraft_;
    std::optional<QualityProfile> pendingQuality_;
    QualityProfile analyzedProfile_;
    std::optional<QualityEstimate> selectedEstimate_;
    QualityRecommendation recommendation_;
    std::string estimateError_;
    double lastAnalysis_ = -1;
    TelemetrySnapshot analysisSnapshot_;
    int analysisFrame_=-1,analysisWidth_=0,analysisHeight_=0;
    ObjectId boneObject_ = NoObject, poseObject_ = NoObject;
    JointId bone_ = NoJoint;
    detail::CameraController controls_;
    void ApplyPending();
    void Ask(Action action);
    void FileDialog(Dialog type);
    void PersistPreferences();
    void Remember(const std::filesystem::path& path,RecentKind kind);
    void Welcome();
    void Console();
    bool Save(const std::filesystem::path& path);
    bool SaveDialog(Dialog type, const std::filesystem::path& path);
    void SceneHistory(bool redo = false);
    void FrameScene(bool selected = true);
    void Dockspace();
    void Menu();
    void Hierarchy();
    void Inspector();
    void EquipmentControls(ObjectId object);
    void BoneInspector();
    void AnimationControls();
    void BlendTreeControls(ObjectId object,ModelHandle model);
    void AdditiveControls(ObjectId object,ModelHandle model);
    void EventControls(ObjectId object,ModelHandle model);
    void PollAnimationEvents();
    void QualitySettings();
    void PerformancePanel();
    const TelemetrySnapshot& RefreshQualityAnalysis();
    void QualityOverview();
    void RenderDebugger();
    void AnimationDebugger();
    void AssetBrowser();
    void AssetInspector();
    void ImportPreview();
    void AdvanceImportPreview();
    void ReimportAsset(AssetId asset);
    void CancelPreviewJob();
    void ViewportPanel();
    void SurfacePanel();
    bool SurfaceInput(float x, float y, ViewportSize size, bool hovered, bool blocked);
    void DrawSurfaceMarks(float x, float y, ViewportSize size);
    void TransformTools(float x,float y,float width,float height,bool enabled,bool showGizmo);
    void StopCameraEdit(bool cancel);
    void Dialogs();
    template<typename T> bool Report(const Result<T>& result,ObjectId object=NoObject) {
        if (!result) { ShowError(result.Error(),object); return false; } return true;
    }
};
} // namespace lv::editor
