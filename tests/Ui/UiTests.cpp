#include "TestHarness.h"
#include <minvex/Ui.h>
#include <minvex/Viewer.h>
#include <thread>
#include <limits>
using namespace test;
using namespace lv;

void TestNumberInput() {
    Viewer viewer;
    auto& ui=viewer.GetUi();
    UiElement field;
    field.Kind=UiKind::NumberInput;
    field.Number=UiNumberInput{std::nullopt,20,200,"cm"};
    const auto authored=field.Number;
    field.Text="Missing";
    const auto id=ui.Add(field).Value();
    TemporaryDirectory temp;
    const auto document=temp.File("number.mvxui.json");
    ui.Save(document).Value();
    const auto original=Read(document);
    Check(ui.GetNumberInput(id).Value().State==UiNumberState::Empty,"measurement starts empty");
    const auto key=[&](ViewerKey code,bool shortcut=false) {
        ViewerInputEvent event;
        event.Type=ViewerInputType::KeyDown;
        event.Key=code;
        event.Shortcut=shortcut;
        viewer.ProcessInput(event).Value();
    };
    const auto type=[&](std::u32string_view text) {
        for (const auto c:text) {
            ViewerInputEvent event;
            event.Type=ViewerInputType::Character;
            event.Character=c;
            viewer.ProcessInput(event).Value();
        }
    };
    type(U"180.5");
    Check(!ui.GetNumberInput(id).Value().Value,"an unfocused field ignores characters");
    key(ViewerKey::Tab);
    Check(ui.Focused()==id,"Tab focuses a number field");
    type(U"180.5");
    Check(ui.GetNumberInput(id).Value().Value==180.5,"decimal measurement reaches the number field");
#if defined(_WIN32) || defined(__APPLE__)
    const auto withUnit=ui.Rasterize(400,100).Value();
    field.Number->Unit="kg";
    ui.Set(id,field).Value();
    Check(ui.Rasterize(400,100).Value()!=withUnit&&ui.GetNumberInput(id).Value().Value==180.5,
          "the displayed unit is independent of the parsed measurement");
    ui.Undo().Value();
    key(ViewerKey::Tab);
#endif
    key(ViewerKey::A,true);
    type(U".");
    auto input=ui.GetNumberInput(id).Value();
    Check(input.State==UiNumberState::Invalid&&input.Text=="."&&!input.Value,"incomplete decimal differs from an empty measurement");
    key(ViewerKey::Delete);
    type(U"200.1");
    input=ui.GetNumberInput(id).Value();
    Check(input.State==UiNumberState::Invalid&&input.Text=="200.1"&&!input.Value,"out-of-range text remains editable");
    key(ViewerKey::Backspace);
    Check(ui.GetNumberInput(id).Value().Value==200,"backspace reaches the inclusive upper bound");
    key(ViewerKey::Enter);
    Check(ui.PollActivations().Value().empty(),"number fields never enqueue button activations");
    key(ViewerKey::Escape);
    Check(ui.Focused()==NoUi&&ui.GetNumberInput(id).Value().Value==200,"Escape leaves the field without discarding the measurement");
    Check(!ui.SetNumberInputValue(id,201)&&ui.GetNumberInput(id).Value().Value==200,"an invalid programmatic write leaves the field unchanged");
    ui.SetNumberInputValue(id,std::nullopt).Value();
    Check(ui.GetNumberInput(id).Value().State==UiNumberState::Empty&&!ui.History().Value().Dirty,"clearing preserves an empty runtime value without dirtying the document");
    ui.SetNumberInputValue(id,20).Value();
    ui.Save(document).Value();
    Check(Read(document)==original,"saving excludes the edited measurement");
    ui.Load(document).Value();
    const auto restored=ui.Elements().Value().front();
    Check(restored.Number==authored,"document restores authored number settings");
    Check(restored.Id!=id&&ui.GetNumberInput(restored.Id).Value().State==UiNumberState::Empty,"loading restores the authored empty value");
    field.Number=UiNumberInput{};
    const auto unbounded=ui.Add(field).Value();
    ui.SetNumberInputValue(unbounded,0).Value();
    Check(ui.GetNumberInput(unbounded).Value().Value==0,"zero is a valid measurement, distinct from no value");
    for (const double value:{-.125,std::numeric_limits<double>::max(),std::numeric_limits<double>::denorm_min()}) {
        ui.SetNumberInputValue(unbounded,value).Value();
        Check(ui.GetNumberInput(unbounded).Value().Value==value,"finite doubles round trip as decimal input");
    }
}

int main() {
    try {
        TestNumberInput();
        UiCanvas ui;UiElement e;e.Kind=UiKind::Button;e.Text="Minvex 한글";e.Layout={10,10,180,56};e.Normal.Fill.fill({.2f,.4f,.8f,1});e.Normal.Radius.fill(12);
        const auto id=ui.Add(e).Value();Check(id!=0,"ID allocated");e.Layout.Width=-1;Check(!ui.Set(id,e)&&ui.Get(id).Value().Layout.Width==180,"invalid edit is transactional");e=ui.Get(id).Value();
        e.Normal.BorderWidth=2;e.Normal.Border={1,1,1,1};e.Normal.Shadow={0,0,0,.6f};e.Hover=e.Normal;e.Pressed=e.Normal;e.Focused=e.Normal;e.Disabled=e.Normal;ui.Set(id,e).Value();
        e.Text="\xc0\x80";Check(!ui.Set(id,e),"overlong UTF-8 rejected");e.Text="Hello 한글";e.Typography.FontFile="../outside.ttf";Check(!ui.Set(id,e),"font traversal rejected");e.Typography.FontFile.clear();e.Layout.ScaleX=std::numeric_limits<float>::quiet_NaN();Check(!ui.Set(id,e),"NaN rejected");e=ui.Get(id).Value();
        auto input=[&](UiInputType t,float x=30,float y=30,bool shift=false){return ui.ProcessInput({t,x,y,shift},320,200).Value();};
        Check(input(UiInputType::Down),"button captures pointer");Check(input(UiInputType::Up),"release consumed");Check(ui.PollActivations().Value()==std::vector<UiId>{id},"click activation exactly once");Check(ui.PollActivations().Value().empty(),"queue drains");
        input(UiInputType::Down);input(UiInputType::Up,300,180);Check(ui.PollActivations().Value().empty(),"outside release cancels");input(UiInputType::Down);input(UiInputType::Cancel);input(UiInputType::Up);Check(ui.PollActivations().Value().empty(),"focus loss cancels");
        input(UiInputType::Tab);Check(ui.Focused()==id,"Tab focuses");input(UiInputType::ActivateDown);input(UiInputType::ActivateUp);Check(ui.PollActivations().Value()==std::vector<UiId>{id},"keyboard activates");
        auto cover=e;cover.Kind=UiKind::Panel;cover.Layout.Z=2;const auto panel=ui.Add(cover).Value();input(UiInputType::Down);input(UiInputType::Up);Check(ui.PollActivations().Value().empty(),"top panel blocks underlying button");ui.Remove(panel).Value();
        e.Enabled=false;ui.Set(id,e).Value();input(UiInputType::Down);input(UiInputType::Up);Check(ui.PollActivations().Value().empty(),"disabled button blocks without activation");e.Enabled=true;e.Clip=UiRect{100,0,100,100};ui.Set(id,e).Value();Check(!input(UiInputType::Down),"clip affects input");e.Clip.reset();e.Layout.AnchorX=1;e.Layout.PivotX=1;e.Layout.X=-10;ui.Set(id,e).Value();Check(input(UiInputType::Down,280,30),"responsive anchor and pivot hit test");
        e.Layout={10,10,180,56};e.Layout.Rotation=.4f;ui.Set(id,e).Value();Check(input(UiInputType::Down,40,40),"inverse rotated hit test");e.Layout.Rotation=0;ui.Set(id,e).Value();
        bool wrong=false;std::thread worker([&]{const auto r=ui.Clear();wrong=!r&&r.Error().Code==ErrorCode::WrongThread;});worker.join();Check(wrong,"wrong thread rejected");
        TemporaryDirectory temp;ui.SetFontDirectory(temp.Path/"Font",true).Value();Check(std::filesystem::is_directory(temp.Path/"Font"),"Font folder created explicitly");const auto document=temp.File("screen.mvxui.json");ui.Save(document).Value();const auto oldId=id;ui.Load(document).Value();Check(!ui.Get(oldId),"load invalidates runtime IDs");const auto loaded=ui.Elements().Value();Check(loaded.size()==1&&loaded[0].Text==e.Text&&loaded[0].Normal.Fill==e.Normal.Fill,"style and text round trip");Write(temp.File("bad.json"),"{\"version\":99}");Check(!ui.Load(temp.File("bad.json"))&&ui.Elements().Value().size()==1,"failed load preserves canvas");
#if defined(_WIN32) || defined(__APPLE__)
        const auto fonts=ui.Fonts().Value();Check(!fonts.empty(),"installed fonts listed");auto pixels=ui.Rasterize(320,200).Value();const auto at=[&](int x,int y,int c){return pixels[(y*320+x)*4+c];};Check(at(30,30,3)==255&&at(319,199,3)==0,"background fill and transparent outside");std::size_t white=0;for (std::size_t i=0;i<pixels.size();i+=4) {if (pixels[i]>180&&pixels[i+1]>180&&pixels[i+2]>180) {++white;}}Check(white>20,"native Unicode text rasterized");Check(!ui.Rasterize(0,200)&&!ui.Rasterize(320,200,8),"raster bounds validated");
        {UiCanvas top;UiElement label;label.Text="TOP";label.Layout={0,0,100,100};label.Typography.Padding.fill(0);label.Typography.Vertical=UiAlign::Start;top.Add(label).Value();auto image=top.Rasterize(100,100).Value();std::size_t upper=0,lower=0;for (int y=0;y<100;++y) {for (int x=0;x<100;++x) {if (image[(y*100+x)*4+3]>0) {if (y<50) {++upper;}else {++lower;}}}}Check(upper>10&&lower==0,"native text uses top-left bitmap orientation");}
        {UiCanvas design;UiElement box;box.Kind=UiKind::Panel;box.Layout={20,20,100,80};box.Normal.Fill={UiColor{1,0,0,1},UiColor{0,1,0,1},UiColor{0,0,1,1},UiColor{1,1,1,1}};box.Normal.Radius={12,0,0,0};box.Normal.Border={1,1,0,1};box.Normal.BorderWidth=2;box.Normal.Shadow={0,0,0,.8f};box.Normal.ShadowY=10;box.Normal.Opacity=.5f;const auto boxId=design.Add(box).Value();auto image=design.Rasterize(160,140).Value();auto a=[&](int x,int y,int c){return image[(y*160+x)*4+c];};Check(a(30,35,0)>a(30,35,1)&&a(110,35,1)>a(110,35,0),"four corner gradient varies spatially");Check(a(21,21,3)<a(60,60,3),"individual rounded corner clips coverage");Check(a(60,21,0)>50&&a(60,21,1)>50,"border drawn inside bounds");Check(a(60,108,3)>0,"shadow extends beyond bounds");int maxAlpha=0;for (std::size_t i=3;i<image.size();i+=4) {maxAlpha=std::max(maxAlpha,static_cast<int>(image[i]));}Check(maxAlpha<=128,"opacity applies to the complete element group");box.Clip=UiRect{20,20,40,80};design.Set(boxId,box).Value();image=design.Rasterize(160,140).Value();Check(a(70,50,3)==0,"explicit clip affects all layers");}
#if defined(_WIN32)
        const auto font=std::filesystem::path(L"C:/Windows/Fonts/segoeui.ttf");
#else
        const auto font=std::filesystem::path("/System/Library/Fonts/Supplemental/Arial.ttf");
#endif
        if (std::filesystem::exists(font)) {std::filesystem::copy_file(font,temp.Path/"Font"/"Custom.ttf");const auto custom=ui.Fonts(false).Value();Check(!custom.empty()&&custom[0].File=="Custom.ttf","file font family metadata");auto value=loaded[0];value.Typography.FontFile="Custom.ttf";value.Typography.Family=custom[0].Family;ui.Set(value.Id,value).Value();Check(bool(ui.Rasterize(320,200)),"custom font rasterized without global installation");ui.Save(document).Value();std::filesystem::create_directories(temp.Path/"Moved"/"Font");std::filesystem::copy_file(temp.Path/"Font"/"Custom.ttf",temp.Path/"Moved"/"Font"/"Custom.ttf");std::filesystem::copy_file(document,temp.Path/"Moved"/"screen.mvxui.json");UiCanvas moved;moved.Load(temp.Path/"Moved"/"screen.mvxui.json").Value();Check(bool(moved.Rasterize(320,200)),"UI document and Font folder relocate together");value.Typography.FontFile="missing.ttf";ui.Set(value.Id,value).Value();Check(!ui.Rasterize(320,200),"missing font diagnosed");Write(temp.Path/"Font"/"Corrupt.ttf","not a font");Check(!ui.Fonts(false),"corrupt font file diagnosed");}
#endif
        Viewer viewer;UiElement b;b.Kind=UiKind::Button;b.Layout={0,0,180,56};const auto bid=viewer.GetUi().Add(b).Value();ViewerInputEvent down;down.Type=ViewerInputType::MouseDown;down.Button=ViewerMouseButton::Left;down.X=30;down.Y=30;viewer.ProcessInput(down).Value();down.Type=ViewerInputType::MouseUp;viewer.ProcessInput(down).Value();Check(viewer.GetUi().PollActivations().Value()==std::vector<UiId>{bid},"Viewer routes normalized pointer input");
    }catch (const std::exception& e) {Check(false,e.what());}return Finish();
}
