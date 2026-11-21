#include <minvex/Viewer.h>
#include <minvex/Version.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <locale>
#include <sstream>

namespace {
constexpr float ChestY=1.05f;
constexpr int WindowWidth=1280, WindowHeight=720;
constexpr lv::UiColor NormalRow{.13f,.18f,.25f,1}, SelectedRow{.12f,.36f,.50f,1};

template<class T> T Require(lv::Result<T> result) {
    if (!result) {throw std::runtime_error(result.Error().Message);}
    return std::move(result).Value();
}
void Check(bool condition,const char* message) {
    if (!condition) {throw std::runtime_error(message);}
}

lv::ModelHandle Box(float width,float height,float depth,lv::UiColor color) {
    auto mesh=lv::MakeCube();
    for (auto& vertex:mesh.Vertices) {
        vertex.Position[0]*=width;
        vertex.Position[1]*=height;
        vertex.Position[2]*=depth;
    }
    auto model=std::make_shared<lv::ModelAsset>();
    model->Meshes.push_back({"Box",{{std::move(mesh),0,false}}});
    model->Parts.push_back({0});
    model->Materials.push_back({});
    model->Materials.front().BaseColor=color;
    model->ModelBounds={{-width/2,-height/2,-depth/2},{width/2,height/2,depth/2}};
    lv::ValidateModel(*model);
    return model;
}

lv::UiElement Control(lv::UiKind kind,float y,std::string text,lv::UiColor fill) {
    lv::UiElement element;
    element.Kind=kind;
    element.Text=std::move(text);
    element.Layout={24,y,280,44};
    element.Normal.Fill.fill(fill);
    element.Normal.Radius.fill(6);
    return element;
}

class Fitting {
public:
    explicit Fitting(lv::Viewer& viewer) {
        auto& scene=viewer.GetScene();
        constexpr lv::UiColor mannequin{.68f,.65f,.59f,1};
        lv::Transform chest;
        chest.Position.Y=ChestY;
        scene.AddModel(Box(.45f,.82f,.30f,mannequin),chest);
        lv::Transform head;
        head.Position.Y=1.58f;
        scene.AddModel(Box(.23f,.26f,.24f,mannequin),head);
        garment_=scene.AddModel(tee_,chest);

        auto camera=scene.MainCamera;
        camera.Position={0,ChestY,2.25f};
        camera.Target={0,ChestY,0};
        Require(viewer.SetCamera(camera));

        auto& ui=viewer.GetUi();
        auto button=Control(lv::UiKind::Button,24,"T-shirt | Switch garment",{.18f,.34f,.49f,1});
        button.Focused=button.Normal;
        button.Focused->Border={.48f,.78f,1,1};
        button.Focused->BorderWidth=2;
        button_=Require(ui.Add(button));

        auto label=Control(lv::UiKind::Text,80,"Measured chest width",{0,0,0,0});
        label.BlocksInput=false;
        Require(ui.Add(label));
        auto input=Control(lv::UiKind::NumberInput,124,"Enter width",NormalRow);
        input.Number=lv::UiNumberInput{std::nullopt,20,90,"cm"};
        input.Focused=input.Normal;
        input.Focused->Border={.48f,.78f,1,1};
        input.Focused->BorderWidth=2;
        input_=Require(ui.Add(input));

        row_=Require(ui.Add(Control(lv::UiKind::Text,180,"",NormalRow)));
        UpdateDimensions(viewer);
        UpdateRow(ui);
    }

    bool OnInput(lv::Viewer& viewer,const lv::ViewerInputEvent& event) {
        // Viewer calls this after UI handling. Let the default controls orbit
        // after movement exceeds four logical pixels.
        if (event.Type==lv::ViewerInputType::FocusLost) {press_.reset();}
        if (event.Type==lv::ViewerInputType::MouseDown) {
            press_.reset();
            if (event.Button==lv::ViewerMouseButton::Left) {press_=Pointer{event.X,event.Y};}
            dragging_=false;
        }
        if (!press_) {return false;}
        if (event.Type==lv::ViewerInputType::MouseMove) {
            dragging_=dragging_||Moved(event);
            return !dragging_;
        }
        if (event.Type==lv::ViewerInputType::MouseUp&&event.Button==lv::ViewerMouseButton::Left) {
            const bool click=!dragging_&&!Moved(event);
            press_.reset();
            if (click) {
                selected_=Require(viewer.PickObject(event.X,event.Y));
                return true;
            }
        }
        return false;
    }

    void OnFrame(lv::Viewer& viewer,const lv::ViewerFrameInfo& frame,bool exercise) {
        if (exercise&&frame.FrameNumber==0) {ExerciseInput(viewer);}
        if (exercise&&frame.FrameNumber==1) {
            BeforeSwap_=viewer.GetScene().MainCamera.ViewProjection(1).Elements;
            PointerEvent(viewer,lv::ViewerInputType::MouseDown,100,45);
            PointerEvent(viewer,lv::ViewerInputType::MouseUp,100,45);
        }
        auto& ui=viewer.GetUi();
        for (const auto id:Require(ui.PollActivations())) {
            if (id!=button_) {continue;}
            // Replace the model in place. Object ID, selection and camera stay live.
            usingJacket_=!usingJacket_;
            viewer.GetScene().SetModel(garment_,usingJacket_ ? jacket_ : tee_);
            Require(ui.SetRuntimeText(button_,usingJacket_ ? "Jacket | Switch garment" : "T-shirt | Switch garment"));
            UpdateDimensions(viewer);
        }
        UpdateRow(ui);
        if (exercise&&frame.FrameNumber==1) {
            Check(viewer.GetScene().GetObject(garment_).Model==jacket_&&LineWidth_==.60f,"Garment swap did not update the model and its dimension line.");
            Check(viewer.GetScene().MainCamera.ViewProjection(1).Elements==BeforeSwap_,"The garment button changed the camera.");
            Check(selected_==garment_&&Highlight_,"The UI click changed the 3D selection.");
        }
        if (exercise&&frame.FrameNumber==2) {CheckRender(viewer);}
    }

private:
    struct Pointer {float X,Y;};
    bool Moved(const lv::ViewerInputEvent& event) const {
        return std::hypot(event.X-press_->X,event.Y-press_->Y)>4;
    }
    void UpdateDimensions(lv::Viewer& viewer) {
        const auto bounds=viewer.GetScene().GetModelBounds(garment_);
        const float z=bounds.Max.Z+.025f;
        constexpr lv::DebugColor color{1,.77f,.25f,1};
        lv::DebugDraw lines;
        lines.DrawLine({bounds.Min.X,ChestY,z},{bounds.Max.X,ChestY,z},color);
        for (const float x:{bounds.Min.X,bounds.Max.X}) {
            lines.DrawLine({x,ChestY-.025f,z},{x,ChestY+.025f,z},color);
        }
        LineWidth_=bounds.Max.X-bounds.Min.X;
        Require(viewer.SetDebugDraw(lines));
    }
    void UpdateRow(lv::UiCanvas& ui) {
        const bool highlight=selected_==garment_;
        if (highlight!=Highlight_) {
            auto row=Require(ui.Get(row_));
            row.Normal.Fill.fill(highlight ? SelectedRow : NormalRow);
            Require(ui.Set(row_,row));
            Highlight_=highlight;
        }
        const auto measurement=Require(ui.GetNumberInput(input_));
        std::ostringstream text;
        text.imbue(std::locale::classic());
        if (measurement.State==lv::UiNumberState::Empty) {text<<"Chest clearance: missing";}
        else if (!measurement.Value) {text<<"Width must be 20-90 cm";}
        else {text<<"Chest clearance: "<<std::showpos<<std::fixed<<std::setprecision(1)<<(LineWidth_*100.0-*measurement.Value)<<" cm";}
        Require(ui.SetRuntimeText(row_,text.str()));
    }
    static void PointerEvent(lv::Viewer& viewer,lv::ViewerInputType type,float x,float y,float dx=0) {
        lv::ViewerInputEvent event;
        event.Type=type;
        event.Button=type==lv::ViewerInputType::MouseMove ? lv::ViewerMouseButton::None : lv::ViewerMouseButton::Left;
        event.X=x;
        event.Y=y;
        event.DeltaX=dx;
        Require(viewer.ProcessInput(event));
    }
    void ExerciseInput(lv::Viewer& viewer) {
        const auto& ui=viewer.GetUi();
        Check(Require(ui.GetNumberInput(input_)).State==lv::UiNumberState::Empty,"The measurement should start empty.");
        PointerEvent(viewer,lv::ViewerInputType::MouseDown,100,140);
        PointerEvent(viewer,lv::ViewerInputType::MouseUp,100,140);
        for (const auto c:std::u32string_view(U"48.5")) {
            lv::ViewerInputEvent event;
            event.Type=lv::ViewerInputType::Character;
            event.Character=c;
            Require(viewer.ProcessInput(event));
        }
        Check(Require(ui.GetNumberInput(input_)).Value==48.5,"The decimal measurement was not entered.");
        PointerEvent(viewer,lv::ViewerInputType::MouseDown,640,360);
        PointerEvent(viewer,lv::ViewerInputType::MouseUp,640,360);
        Check(selected_==garment_,"The garment should be the nearest bounds hit.");

        const auto original=viewer.GetScene().MainCamera.ViewProjection(1).Elements;
        PointerEvent(viewer,lv::ViewerInputType::MouseDown,950,360);
        PointerEvent(viewer,lv::ViewerInputType::MouseMove,980,360,30);
        PointerEvent(viewer,lv::ViewerInputType::MouseUp,980,360);
        Check(viewer.GetScene().MainCamera.ViewProjection(1).Elements!=original&&selected_==garment_,
              "Dragging should orbit without changing the selection.");
    }
    static void CheckRender(lv::Viewer& viewer) {
        const auto debug=Require(viewer.GetRenderDebugInfo());
        const auto quality=Require(viewer.GetQuality());
        const auto drew=[&](std::string_view name) {
            return std::any_of(debug.Passes.begin(),debug.Passes.end(),[&](const auto& pass) {
                return pass.Name==name&&pass.DrawCalls>0;
            });
        };
        Check(debug.RenderedFrames>=2&&debug.LineCount==3&&drew("Scene")&&drew("Debug overlay")&&drew("UI overlay"),
              "Expected a rendered scene, dimension lines and UI overlay.");
        Check(quality.Applied&&quality.Requested.Base==lv::QualityPreset::Low&&
              quality.RenderWidth==WindowWidth&&quality.RenderHeight==WindowHeight,"Expected applied Low at 1280x720.");
    }

    lv::ModelHandle tee_=Box(.54f,.54f,.36f,{.23f,.67f,.78f,1});
    lv::ModelHandle jacket_=Box(.60f,.67f,.43f,{.59f,.35f,.70f,1});
    lv::ObjectId garment_=lv::NoObject, selected_=lv::NoObject;
    lv::UiId button_=lv::NoUi, input_=lv::NoUi, row_=lv::NoUi;
    std::optional<Pointer> press_;
    std::array<float,16> BeforeSwap_{};
    bool dragging_=false, usingJacket_=false, Highlight_=false;
    float LineWidth_=0;
};
}

int main(int argc,char* argv[]) {
    try {
        lv::ViewerRunOptions run;
        bool exercise=false;
        for (int i=1;i<argc;++i) {
            const std::string_view arg(argv[i]);
            if (arg=="--exercise") {exercise=true;}
            else if (arg=="--frames"&&i+1<argc) {
                const std::string_view count(argv[++i]);
                const auto [end,error]=std::from_chars(count.data(),count.data()+count.size(),run.FrameLimit);
                if (error!=std::errc{}||end!=count.data()+count.size()) {return 2;}
            } else {
                std::fputs("Usage: minvex_fitting [--frames N] [--exercise]\n",stderr);
                return 2;
            }
        }
        if (exercise) {run.FrameLimit=std::max<std::uint64_t>(3,run.FrameLimit);}

        lv::ViewerOptions options;
        options.Title="Minvex Fitting Quick Start";
        options.Width=WindowWidth;
        options.Height=WindowHeight;
        options.AutoFrame=false;
        options.Quality=lv::MakeQualityProfile(lv::QualityPreset::Low);
        options.Quality.Overrides.Resolution=lv::ResolutionMode::Fixed;
        options.Quality.Overrides.FixedWidth=WindowWidth;
        options.Quality.Overrides.FixedHeight=WindowHeight;
        lv::Viewer viewer(options);
        Fitting app(viewer);
        std::uint64_t frames=0;
        run.OnInput=[&](lv::Viewer& active,const lv::ViewerInputEvent& input) {return app.OnInput(active,input);};
        run.OnFrame=[&](lv::Viewer& active,const lv::ViewerFrameInfo& frame) {
            app.OnFrame(active,frame,exercise);
            frames=frame.FrameNumber+1;
        };
        Require(viewer.Run(run));
        const auto telemetry=Require(viewer.GetTelemetry());
        if (exercise) {
            Check(frames==run.FrameLimit,"The exercise ended before rendering all requested frames.");
        }
        const auto configuration=lv::GetBuildInfo().Configuration;
        std::printf("Rendered %llu frames | %.*s | %s | %dx%d\n",
                    static_cast<unsigned long long>(frames),
                    static_cast<int>(configuration.size()),configuration.data(),
                    telemetry.Quality.Requested.Name.c_str(),telemetry.Quality.RenderWidth,telemetry.Quality.RenderHeight);
    } catch (const std::exception& error) {
        std::fprintf(stderr,"%s\n",error.what());
        return 1;
    }
}
