#include "MaintenanceModel.h"
#include <minvex/Viewer.h>

#include <cmath>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <vector>

namespace {
using maintenance::Assembly;
using maintenance::PartIds;
enum PartIndex : std::size_t { Housing, Cover, DriveGear, DrivenGear, BoltFL, BoltFR, BoltBL, BoltBR };

constexpr std::array PartLabels{"하우징", "덮개", "구동 기어", "종동 기어", "나사 좌하", "나사 우하", "나사 좌상", "나사 우상"};
struct Step {
    std::string_view Title, Instruction;
};
constexpr std::array Steps{
    Step{"조립 상태", "하우징과 덮개의 위치를 확인하세요. 다음 단계에서 덮개를 고정하는 나사 네 개를 분리합니다."},
    Step{"나사 분리", "덮개를 고정하는 나사 네 개를 분리합니다. 각 나사의 위치와 이동 방향을 확인하세요."},
    Step{"덮개 분리", "덮개를 들어 올려 분리합니다. 하우징 안쪽에 남아 있는 두 기어의 위치를 확인하세요."},
    Step{"기어 점검", "구동 기어와 종동 기어의 맞물림 상태를 확인하세요. 이전 버튼으로 조립 순서를 다시 살펴볼 수 있습니다."}
};
constexpr double MoveSeconds = .45;
constexpr int InitialWidth = 1280, InitialHeight = 720;
constexpr float Margin = 16;

template<class T> T Require(lv::Result<T> result) {
    if (!result) { throw std::runtime_error(result.Error().Message); }
    return std::move(result).Value();
}

void Check(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

// The authored strings have spaces between words. Protect the UTF-8 scalars
// within each word with U+2060 and let the native layout wrap at spaces.
std::string KeepWords(std::string_view text) {
    std::string result;
    bool insideWord = false;
    for (const char byte : text) {
        if (byte == ' ' || byte == '\n') { insideWord = false; }
        else if ((static_cast<unsigned char>(byte) & 0xC0) != 0x80) {
            if (insideWord) { result += "\xE2\x81\xA0"; }
            insideWord = true;
        }
        result += byte;
    }
    return result;
}

lv::UiElement Label(lv::UiId parent, std::string_view text, float size = 16) {
    lv::UiElement label;
    label.Parent = parent;
    label.Text = KeepWords(text);
    label.Layout.WidthSizing = lv::UiSizing::Fill;
    label.Layout.HeightSizing = lv::UiSizing::Hug;
    label.Typography.Size = size;
    label.Typography.Padding = {6, 4, 6, 4};
    label.Typography.Vertical = lv::UiAlign::Start;
    label.BlocksInput = false;
    return label;
}

lv::UiElement Button(lv::UiId parent, std::string_view text) {
    auto button = Label(parent, text);
    button.Kind = lv::UiKind::Button;
    button.Layout.HeightSizing = lv::UiSizing::Fill;
    button.Typography.Horizontal = button.Typography.Vertical = lv::UiAlign::Center;
    button.BlocksInput = true;
    button.Normal.Fill.fill({.13f, .31f, .45f, 1});
    button.Normal.Radius.fill(6);
    button.Hover = button.Normal;
    button.Hover->Fill.fill({.16f, .40f, .55f, 1});
    button.Focused = button.Normal;
    button.Focused->Border = {.45f, .77f, 1, 1};
    button.Focused->BorderWidth = 2;
    return button;
}

bool SamePosition(lv::Vec3 a, lv::Vec3 b) {
    return a.X == b.X && a.Y == b.Y && a.Z == b.Z;
}

void CheckKoreanWrap() {
    lv::UiCanvas ui;
    constexpr int Size = 18, Viewport = 320;
    constexpr std::string_view Lead = "나사 네 개의 위치를 ", Word = "확인하세요.";
    auto label = Label(lv::NoUi, Lead, Size);
    label.Layout.X = label.Layout.Y = 0;
    label.Layout.WidthSizing = lv::UiSizing::Hug;
    label.Typography.Padding.fill(0);
    const auto id = Require(ui.Add(label));
    const float leadWidth = Require(ui.ResolveLayout(Viewport, Viewport)).front().Width;
    label.Text = KeepWords(Word);
    Require(ui.Set(id, label));
    const float wordWidth = Require(ui.ResolveLayout(Viewport, Viewport)).front().Width;

    label.Layout.WidthSizing = lv::UiSizing::Fixed;
    label.Layout.Width = std::max(leadWidth, wordWidth) + wordWidth * .35f;
    lv::UiTextRun lead, word;
    lead.Text = KeepWords(Lead);
    word.Text = KeepWords(Word);
    word.Color = lv::UiColor{1, 0, 0, 1};
    label.Runs = {lead, word};
    Require(ui.Set(id, label));
    const auto pixels = Require(ui.Rasterize(Viewport, Viewport));
    int first = Viewport, last = -1;
    for (int y = 0; y < Viewport; ++y) {
        for (int x = 0; x < Viewport; ++x) {
            const auto offset = 4 * static_cast<std::size_t>(y * Viewport + x);
            if (pixels[offset] > 100 && pixels[offset + 1] == 0 && pixels[offset + 2] == 0) {
                first = std::min(first, y);
                last = y;
            }
        }
    }
    Check(first >= Size && last >= first && last - first < Size * label.Typography.LineHeight,
          "The Korean word should move intact to the next rendered line.");
}

class Manual {
public:
    Manual(lv::Viewer& viewer, const std::filesystem::path& model, const std::filesystem::path& replacement)
        : viewer_(viewer), models_{model, replacement}, assembly_(maintenance::LoadAssembly(model)) {
        auto& scene = viewer_.GetScene();
        for (std::size_t part = 0; part < PartIds.size(); ++part) {
            objects_[part] = scene.AddModel(assembly_[part].Model, assembly_[part].Assembled);
            scene.SetName(objects_[part], std::string(PartIds[part]));
        }
        auto camera = scene.MainCamera;
        camera.Position = {-1.7f, 1.35f, 5};
        Require(viewer_.SetCamera(camera));
        Require(viewer_.FrameScene());
        BuildUi();
        BeginStep(0, true);
    }

    void Resize(int width, int height) {
        if (width <= 0 || height <= 0 || (width == width_ && height == height_)) { return; }
        width_ = width;
        height_ = height;
        auto& ui = viewer_.GetUi();
        auto panel = Require(ui.Get(panel_));
        panel.Layout.Width = std::max(40.f, std::min({400.f, std::max(210.f, width * .42f), width - 2 * Margin}));
        panel.Layout.Height = std::max(70.f, height - 2 * Margin);
        Require(ui.Set(panel_, panel));
        auto instruction = Require(ui.Get(instruction_));
        instruction.Typography.Size = width >= 950 ? 18.f : 16.f;
        Require(ui.Set(instruction_, instruction));
    }

    void Advance(double seconds) {
        if (elapsed_ == MoveSeconds) { return; }
        elapsed_ = std::min(elapsed_ + seconds, MoveSeconds);
        const auto progress = static_cast<float>(elapsed_ / MoveSeconds);
        const float eased = progress * progress * (3 - 2 * progress);
        auto& scene = viewer_.GetScene();
        for (std::size_t part = 0; part < PartIds.size(); ++part) {
            auto transform = Destination(part);
            const auto from = start_[part];
            const auto to = transform.Position;
            transform.Position = {std::lerp(from.X, to.X, eased), std::lerp(from.Y, to.Y, eased), std::lerp(from.Z, to.Z, eased)};
            scene.SetTransform(objects_[part], transform);
            if (elapsed_ == MoveSeconds) { scene.SetVisible(objects_[part], Visible(part)); }
        }
    }

    bool HandleButtons() {
        auto requested = step_;
        bool replaced = false;
        for (const auto id : Require(viewer_.GetUi().PollActivations())) {
            if (id == next_) { requested = std::min(requested + 1, Steps.size() - 1); }
            if (id == previous_ && requested > 0) { --requested; }
            if (id == replace_) { replaced = ReplaceModel() || replaced; }
        }
        if (requested == step_) { return replaced; }
        BeginStep(requested);
        return true;
    }

    void Exercise(std::uint64_t frame);

private:
    void BuildUi() {
        auto& ui = viewer_.GetUi();
        lv::UiElement panel;
        panel.Kind = lv::UiKind::Panel;
        panel.Layout.X = -Margin;
        panel.Layout.Y = Margin;
        panel.Layout.AnchorX = panel.Layout.PivotX = 1;
        panel.Container.Flow = lv::UiFlow::Column;
        panel.Container.Padding.fill(12);
        panel.Normal.Fill.fill({.066f, .10f, .15f, .97f});
        panel.Normal.Radius.fill(14);
        panel_ = Require(ui.Add(panel));

        lv::UiElement content;
        content.Kind = lv::UiKind::Panel;
        content.Parent = panel_;
        content.Layout.WidthSizing = content.Layout.HeightSizing = lv::UiSizing::Fill;
        content.Container.Flow = lv::UiFlow::Column;
        content.Container.Overflow = lv::UiOverflow::Scroll;
        content_ = Require(ui.Add(content));
        auto title = Label(content_, "기어박스 정비 매뉴얼", 24);
        title.Typography.Weight = 700;
        Require(ui.Add(title));
        stepTitle_ = Require(ui.Add(Label(content_, "", 19)));
        instruction_ = Require(ui.Add(Label(content_, "단계 안내", 18)));
        auto heading = Label(content_, "부품 목록");
        heading.Layout.Order = 1;
        heading.Typography.Weight = 700;
        Require(ui.Add(heading));
        for (std::size_t part = 0; part < PartIds.size(); ++part) {
            auto row = Label(content_, "", 14);
            row.Layout.Order = 2;
            row.Normal.Fill.fill({.10f, .15f, .21f, 1});
            row.Normal.Radius.fill(4);
            rows_[part] = Require(ui.Add(row));
        }
        if (!models_[1].empty()) {
            auto button = Button(content_, "모델 교체");
            button.Layout.Order = 3;
            button.Layout.HeightSizing = lv::UiSizing::Fixed;
            button.Layout.Height = 38;
            replace_ = Require(ui.Add(button));
            auto status = Label(content_, "", 14);
            status.Layout.Order = 4;
            status_ = Require(ui.Add(status));
        }

        lv::UiElement nav;
        nav.Kind = lv::UiKind::Panel;
        nav.Parent = panel_;
        nav.Layout.WidthSizing = lv::UiSizing::Fill;
        nav.Layout.Height = 42;
        nav.Container.Flow = lv::UiFlow::Row;
        const auto navId = Require(ui.Add(nav));
        previous_ = Require(ui.Add(Button(navId, "이전")));
        next_ = Require(ui.Add(Button(navId, "다음")));
        Resize(InitialWidth, InitialHeight);
    }

    bool Visible(std::size_t part) const { return step_ < 2 || (part != Cover && part < BoltFL); }
    bool Highlighted(std::size_t part) const {
        return (step_ == 1 && part >= BoltFL) || (step_ == 2 && part == Housing) ||
               (step_ == 3 && (part == DriveGear || part == DrivenGear));
    }

    lv::Transform Destination(std::size_t part) const {
        auto transform = assembly_[part].Assembled;
        // Offsets are in assembly space. Each stage specifies an absolute
        // destination relative to the imported pose, so reversing cannot drift.
        constexpr std::array<lv::Vec3, 4> boltOffsets{{
            {-.24f, -.18f, .65f}, {.24f, -.18f, .65f}, {-.24f, .18f, .65f}, {.24f, .18f, .65f}
        }};
        auto& p = transform.Position;
        if (part >= BoltFL && step_ >= 1) {
            const auto offset = boltOffsets[part - BoltFL];
            p = {p.X + offset.X, p.Y + offset.Y, p.Z + offset.Z};
        }
        if (part == Cover && step_ >= 2) { p = {p.X - 1.15f, p.Y + .28f, p.Z + 1.15f}; }
        return transform;
    }

    void BeginStep(std::size_t step, bool snap = false) {
        step_ = step;
        elapsed_ = snap ? MoveSeconds : 0;
        auto& scene = viewer_.GetScene();
        auto& ui = viewer_.GetUi();
        for (std::size_t part = 0; part < PartIds.size(); ++part) {
            const auto id = objects_[part];
            const auto& current = scene.GetObject(id);
            // Read the pose already submitted to the scene. Leave it in place
            // when a new click interrupts an in-flight move.
            start_[part] = current.LocalTransform.Position;
            if (snap) { scene.SetTransform(id, Destination(part)); }
            scene.SetVisible(id, Visible(part) || (!snap && current.Visible));
            const auto& materials = assembly_[part].Model->Materials;
            for (std::size_t index = 0; index < materials.size(); ++index) {
                if (Highlighted(part)) {
                    auto material = materials[index];
                    material.BaseColor[0] = 1;
                    material.BaseColor[1] = .54f;
                    material.BaseColor[2] = .09f;
                    scene.SetMaterial(id, index, material);
                } else {
                    scene.ResetMaterial(id, index);
                }
            }
            const char* state = Highlighted(part) ? "강조" : Visible(part) ? "표시" : "분리";
            Require(ui.SetRuntimeText(rows_[part], KeepWords(std::string(PartLabels[part]) + "   ·   " + state)));
        }
        Require(ui.SetRuntimeText(stepTitle_, std::to_string(step + 1) + " / " + std::to_string(Steps.size()) +
                                             "   " + KeepWords(Steps[step].Title)));
        Require(ui.SetRuntimeText(instruction_, KeepWords(Steps[step].Instruction)));
    }

    bool ReplaceModel() {
        const auto next = 1 - activeModel_;
        Assembly candidate;
        try {
            candidate = maintenance::LoadAssembly(models_[next]);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "Cannot replace model: %s\n", error.what());
            Require(viewer_.GetUi().SetRuntimeText(status_, KeepWords("교체 실패. 모델 경로와 부품 ID를 확인하세요.")));
            return false;
        }
        // Resolve all names, geometry and transforms first. Commit the models
        // to their existing objects, then apply the current stage to the new poses.
        for (std::size_t part = 0; part < PartIds.size(); ++part) {
            viewer_.GetScene().SetModel(objects_[part], candidate[part].Model);
        }
        assembly_ = std::move(candidate);
        activeModel_ = next;
        BeginStep(step_, true);
        Require(viewer_.GetUi().SetRuntimeText(status_, KeepWords("모델을 교체했습니다.")));
        return true;
    }

    lv::UiLayoutBox Box(lv::UiId id) const {
        const auto boxes = Require(viewer_.GetUi().ResolveLayout(static_cast<float>(width_), static_cast<float>(height_)));
        const auto found = std::find_if(boxes.begin(), boxes.end(), [id](const auto& box) { return box.Id == id; });
        Check(found != boxes.end(), "Missing UI layout box.");
        return *found;
    }

    void Click(lv::UiId id) {
        const auto box = Box(id);
        lv::ViewerInputEvent event;
        event.Type = lv::ViewerInputType::MouseDown;
        event.Button = lv::ViewerMouseButton::Left;
        event.X = box.Transform[4] + box.Width * .5f;
        event.Y = box.Transform[5] + box.Height * .5f;
        Require(viewer_.ProcessInput(event));
        event.Type = lv::ViewerInputType::MouseUp;
        Require(viewer_.ProcessInput(event));
    }

    void CheckStage(std::size_t expected) const {
        Check(step_ == expected && elapsed_ == MoveSeconds, "Expected a completed stage.");
        const auto& scene = viewer_.GetScene();
        for (std::size_t part = 0; part < PartIds.size(); ++part) {
            const auto& object = scene.GetObject(objects_[part]);
            Check(object.Name == PartIds[part] && object.Model == assembly_[part].Model &&
                  SamePosition(object.LocalTransform.Position, Destination(part).Position) &&
                  object.Visible == Visible(part) && (!object.MaterialOverrides.empty()) == Highlighted(part),
                  "A part did not reach its stage pose, visibility, or material.");
        }
    }

    lv::Viewer& viewer_;
    std::array<std::filesystem::path, 2> models_;
    Assembly assembly_;
    std::array<lv::ObjectId, PartIds.size()> objects_{};
    std::array<lv::Vec3, PartIds.size()> start_{};
    std::array<lv::UiId, PartIds.size()> rows_{};
    lv::UiId panel_{}, content_{}, stepTitle_{}, instruction_{}, previous_{}, next_{}, replace_{}, status_{};
    std::size_t step_{}, activeModel_{};
    double elapsed_ = MoveSeconds;
    int width_{}, height_{};
    std::array<float, 16> cameraBeforeSteps_{};
};

void Manual::Exercise(std::uint64_t frame) {
    auto& scene = viewer_.GetScene();
    if (frame == 0) {
        CheckStage(0);
        for (std::size_t part = BoltFR; part <= BoltBR; ++part) {
            Check(objects_[part] != objects_[BoltFL] && assembly_[part].Model == assembly_[BoltFL].Model,
                  "The exercise GLB should have four independently editable instances of its bolt mesh.");
        }
        const auto camera = scene.MainCamera.ViewProjection(1).Elements;
        lv::ViewerInputEvent event;
        event.Type = lv::ViewerInputType::MouseDown;
        event.Button = lv::ViewerMouseButton::Left;
        event.X = 220;
        event.Y = 330;
        Require(viewer_.ProcessInput(event));
        event.Type = lv::ViewerInputType::MouseMove;
        event.X += 32;
        event.DeltaX = 32;
        Require(viewer_.ProcessInput(event));
        event.Type = lv::ViewerInputType::MouseUp;
        Require(viewer_.ProcessInput(event));
        const auto orbited = scene.MainCamera.ViewProjection(1).Elements;
        event.Type = lv::ViewerInputType::Scroll;
        event.ScrollY = 1;
        Require(viewer_.ProcessInput(event));
        cameraBeforeSteps_ = scene.MainCamera.ViewProjection(1).Elements;
        Check(camera != orbited && orbited != cameraBeforeSteps_, "Orbit and zoom should change the camera.");
        Click(next_);
        HandleButtons();
        Advance(MoveSeconds);
        CheckStage(1);
    } else if (frame == 1) {
        Click(next_);
        HandleButtons();
        Advance(MoveSeconds * .35);
        const auto moving = scene.GetObject(objects_[Cover]).LocalTransform.Position;
        Check(!SamePosition(moving, Destination(Cover).Position) && scene.GetObject(objects_[Cover]).Visible,
              "The cover should be visible and moving toward its removal pose.");
        Click(previous_);
        HandleButtons();
        Check(SamePosition(moving, scene.GetObject(objects_[Cover]).LocalTransform.Position),
              "Reversing mid-move should start at the current pose.");
        Advance(MoveSeconds * .35);
    } else if (frame == 2) {
        const auto moving = scene.GetObject(objects_[Cover]).LocalTransform.Position;
        Click(next_);
        Click(next_);
        Click(next_);
        HandleButtons();
        Check(step_ == 3 && SamePosition(moving, scene.GetObject(objects_[Cover]).LocalTransform.Position),
              "Consecutive clicks should clamp at the last stage without jumping.");
        Advance(MoveSeconds);
        CheckStage(3);
        const auto before = scene.GetObject(objects_[Housing]).Model;
        Click(replace_);
        HandleButtons();
        CheckStage(3);
        Check(activeModel_ == 1 && scene.GetObject(objects_[Housing]).Model != before,
              "Replacement should update the model attached to the same objects.");
    } else if (frame == 3) {
        Click(previous_);
        HandleButtons();
        Advance(MoveSeconds);
        CheckStage(2);
        const auto wide = Box(instruction_);
        Resize(560, 340);
        const auto narrow = Box(instruction_);
        Check(narrow.Width < wide.Width && narrow.Height > wide.Height, "Korean directions should reflow at a narrower width.");
        CheckKoreanWrap();
        auto& ui = viewer_.GetUi();
        Require(ui.SetScroll(content_, 0, 10000));
        Check(Box(content_).ScrollY > 0 && Box(next_).Transform[5] + Box(next_).Height <= height_,
              "The part list should scroll while navigation stays in view.");
        Require(ui.SetScroll(content_, 0, 0));
        Resize(InitialWidth, InitialHeight);
    } else if (frame == 4) {
        Click(previous_);
        HandleButtons();
        Advance(MoveSeconds);
        CheckStage(1);
        Click(previous_);
        Click(previous_);
        HandleButtons();
        Advance(MoveSeconds);
        CheckStage(0);
        Click(replace_);
        HandleButtons();
        CheckStage(0);
        Check(activeModel_ == 0, "The replacement button should restore the first model.");
        const auto debug = Require(viewer_.GetRenderDebugInfo());
        const auto drew = [&](std::string_view name) {
            return std::any_of(debug.Passes.begin(), debug.Passes.end(),
                               [name](const auto& pass) { return pass.Name == name && pass.DrawCalls > 0; });
        };
        Check(debug.RenderedFrames >= 4 && drew("Scene") && drew("UI overlay"),
              "The exercise should render the scene and the Korean UI.");
    }
    Check(scene.MainCamera.ViewProjection(1).Elements == cameraBeforeSteps_, "Stage and model changes should preserve the camera.");
}

int Run(std::span<const std::filesystem::path> args) {
    std::filesystem::path model, replacement;
    bool exercise = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--exercise") { exercise = true; }
        else if (args[i] == "--replacement" && replacement.empty() && i + 1 < args.size()) { replacement = args[++i]; }
        else if (model.empty() && !args[i].native().starts_with(std::filesystem::path("--").native())) { model = args[i]; }
        else { model.clear(); break; }
    }
    if (model.empty() || (exercise && replacement.empty())) {
        std::fputs("Usage: minvex_maintenance gearbox.glb [--replacement gearbox-alt.glb] [--exercise]\n", stderr);
        return 2;
    }

    lv::ViewerOptions options;
    options.Title = "Minvex | 기어박스 정비 매뉴얼";
    options.Width = InitialWidth;
    options.Height = InitialHeight;
    options.AutoFrame = false;
    options.Quality = lv::MakeQualityProfile(lv::QualityPreset::Low);
    lv::Viewer viewer(options);
    Manual manual(viewer, model, replacement);
    std::uint64_t frames = 0;
    lv::ViewerRunOptions run;
    run.FrameLimit = exercise ? 5 : 0;
    run.OnInput = [&](lv::Viewer&, const lv::ViewerInputEvent& input) {
        if (input.Type == lv::ViewerInputType::Resized) { manual.Resize(input.Width, input.Height); }
        return false; // UI first, then the default orbit/pan/zoom outside the panel.
    };
    run.OnFrame = [&](lv::Viewer&, const lv::ViewerFrameInfo& frame) {
        if (exercise) { manual.Exercise(frame.FrameNumber); }
        else if (!manual.HandleButtons()) {
            // The first frame of a retarget keeps the last submitted position.
            manual.Advance(frame.DeltaTimeSeconds);
        }
        ++frames;
    };
    Require(viewer.Run(run));
    if (exercise) {
        Check(frames == run.FrameLimit, "The exercise ended before all verification frames were rendered.");
        std::puts("Maintenance Quick Start exercise passed.");
    }
    return 0;
}
} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
    try {
        const std::vector<std::filesystem::path> args(argv + 1, argv + argc);
        return Run(args);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
