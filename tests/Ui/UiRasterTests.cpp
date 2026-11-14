#include "TestHarness.h"
#include <minvex/Ui.h>
#include <algorithm>
#include <random>

using namespace lv;
using namespace test;
namespace {
UiElement Panel(float x,float y,float w,float h,UiColor color) {
    UiElement e;e.Kind=UiKind::Panel;e.Layout={x,y,w,h};e.Normal.Fill.fill(color);return e;
}
// RefreshFonts invalidates retained pixels without changing content or input.
// Compare each incremental update with a complete paint of the same state.
void MatchesFull(UiCanvas& ui,const char* message,int width=320,int height=240,float scale=1) {
    const auto incremental=ui.Rasterize(width,height,scale).Value();
    ui.RefreshFonts().Value();
    Check(incremental==ui.Rasterize(width,height,scale).Value(),message);
}
void DamageAndStacking() {
    UiCanvas ui;
    auto under=Panel(0,0,300,220,{.1f,.2f,.4f,.8f});ui.Add(under).Value();
    auto a=Panel(28,35,80,65,{1,.1f,.2f,.7f});a.Normal.Radius={12,4,20,1};
    a.Normal.Shadow={0,0,0,.6f};a.Normal.ShadowBlur=9;a.Normal.ShadowX=-7;a.Normal.ShadowY=13;
    const auto first=ui.Add(a).Value();
    auto b=Panel(65,60,110,70,{.1f,1,.2f,.6f});b.Layout.Rotation=.22f;b.Normal.Opacity=.7f;
    const auto second=ui.Add(b).Value();
    MatchesFull(ui,"first raster matches full paint");
    a.Normal.Fill.fill({.8f,.4f,.1f,.4f});ui.Set(first,a).Value();
    MatchesFull(ui,"changed translucent layer repaints overlapping siblings in order");
    a.Layout.X=180;a.Layout.Y=120;a.Layout.Rotation=-.4f;ui.Set(first,a).Value();
    MatchesFull(ui,"moving and rotating clears old pixels including shadow");
    a.Layout.Z=10;a.Layout.X=70;a.Layout.Y=55;ui.Set(first,a).Value();
    MatchesFull(ui,"z reorder repairs old and new stacking positions");
    b.Visible=false;ui.Set(second,b).Value();MatchesFull(ui,"hiding removes cached pixels");
    b.Visible=true;b.Normal.Opacity=0;ui.Set(second,b).Value();MatchesFull(ui,"transparent element contributes no stale pixels");
    b.Normal.Opacity=.6f;ui.Set(second,b).Value();MatchesFull(ui,"restoring opacity repaints content");
    ui.Remove(first).Value();MatchesFull(ui,"removal restores underlying content");
    ui.Undo().Value();MatchesFull(ui,"undo removal restores the element");
    ui.Redo().Value();MatchesFull(ui,"redo removal removes it again");
    auto exported=ui.Rasterize(320,240).Value();const auto original=exported;exported[0]^=255;
    Check(ui.Rasterize(320,240).Value()==original,"raster export is an independent copy of retained pixels");
    ui.Clear().Value();const auto empty=ui.Rasterize(320,240).Value();
    Check(std::all_of(empty.begin(),empty.end(),[](auto byte){return byte==0;}),"clear releases visible cached content");
}
void AncestorsAndViewport() {
    UiCanvas ui;auto root=Panel(30,30,105,80,{.15f,.2f,.3f,1});
    const auto parent=ui.Add(root).Value();
    auto child=Panel(0,0,180,110,{1,.3f,.2f,.8f});child.Parent=parent;
    child.Disabled=child.Normal;child.Disabled->Fill.fill({0,0,1,1});
    const auto id=ui.Add(child).Value();
    MatchesFull(ui,"overflow-visible child can extend beyond parent damage bounds");
    root.Clip=UiRect{40,40,60,50};ui.Set(parent,root).Value();
    MatchesFull(ui,"ancestor viewport clip invalidates outlying child pixels");
    root.Clip.reset();root.Container.Overflow=UiOverflow::Scroll;root.Container.Padding.fill(7);ui.Set(parent,root).Value();
    MatchesFull(ui,"ancestor content clipping invalidates child raster");
    ui.SetScroll(parent,60,35).Value();MatchesFull(ui,"scroll updates transformed child and ancestor clip");
    root.Layout.Rotation=.3f;root.Layout.ScaleX=1.3f;root.Layout.ScaleY=.8f;ui.Set(parent,root).Value();
    MatchesFull(ui,"ancestor affine and nonuniform clipping repairs child pixels");
    root.Enabled=false;ui.Set(parent,root).Value();MatchesFull(ui,"inherited disabled style changes cached child");
    root.Enabled=true;root.Visible=false;ui.Set(parent,root).Value();MatchesFull(ui,"inherited visibility clears entire subtree");
    root.Visible=true;ui.Set(parent,root).Value();MatchesFull(ui,"subtree becomes visible again");
    child.Layout.AnchorX=child.Layout.PivotX=1;child.Layout.X=-3;ui.Set(id,child).Value();
    MatchesFull(ui,"fractional DPI uses resolved dimensions and transforms",355,263,1.5f);
    MatchesFull(ui,"framebuffer resize replaces retained image",277,187,.75f);
    child.Layout.X=2000;ui.Set(id,child).Value();MatchesFull(ui,"moving offscreen removes former pixels",277,187,.75f);
    child.Layout.X=-3;ui.Set(id,child).Value();MatchesFull(ui,"moving onscreen restores clipped pixels",277,187,.75f);
}
void InputAndText() {
#if defined(_WIN32) || defined(__APPLE__)
    UiCanvas ui;UiElement button;button.Kind=UiKind::Button;button.Layout={15,15,185,65};button.Text="Cached 한글";
    button.Normal.Fill.fill({.1f,.2f,.4f,1});button.Hover=button.Normal;button.Hover->Text={1,0,0,1};
    button.Pressed=button.Normal;button.Pressed->Text={0,1,0,1};button.Focused=button.Normal;button.Focused->Text={0,0,1,1};
    const auto id=ui.Add(button).Value();MatchesFull(ui,"text initial raster");
    ui.ProcessInput({UiInputType::Move,40,40},320,240).Value();MatchesFull(ui,"hover text color invalidates retained pixels");
    ui.ProcessInput({UiInputType::Down,40,40},320,240).Value();MatchesFull(ui,"pressed style invalidates retained pixels");
    ui.ProcessInput({UiInputType::Up,250,200},320,240).Value();MatchesFull(ui,"focused style restores pixels after release outside");
    ui.ProcessInput({UiInputType::Cancel},320,240).Value();MatchesFull(ui,"cancel restores normal pixels");
    button.Text="I";ui.Set(id,button).Value();MatchesFull(ui,"shorter text removes old glyph pixels");
    UiTextRun a;a.Text="Rich ";a.Color=UiColor{1,0,1,1};a.Size=26.0f;
    UiTextRun b;b.Text="type";b.Size=14.0f;button.Runs={a,b};ui.Set(id,button).Value();
    MatchesFull(ui,"rich runs replace plain text after incremental update");
    button.Layout.HeightSizing=UiSizing::Hug;button.Runs[1].Text="wrapping text wrapping text wrapping text";ui.Set(id,button).Value();
    MatchesFull(ui,"intrinsic text size change updates old and new bounds");
    TemporaryDirectory temp;ui.SetFontDirectory(temp.Path/"Font",true).Value();
#if defined(_WIN32)
    const auto font=std::filesystem::path(L"C:/Windows/Fonts/segoeui.ttf");
#else
    const auto font=std::filesystem::path("/System/Library/Fonts/Supplemental/Arial.ttf");
#endif
    if (std::filesystem::exists(font)) {
        std::filesystem::copy_file(font,temp.Path/"Font"/"Local.ttf");
        button.Runs.clear();button.Text="Refresh";button.Typography.FontFile="Local.ttf";ui.Set(id,button).Value();
        ui.Rasterize(320,240).Value();
        Write(temp.Path/"Font"/"Local.ttf","invalid font");ui.RefreshFonts().Value();
        Check(!ui.Rasterize(320,240),"font refresh invalidates cached custom-font pixels and diagnoses new data");
        std::filesystem::copy_file(font,temp.Path/"Font"/"Local.ttf",std::filesystem::copy_options::overwrite_existing);ui.RefreshFonts().Value();
        MatchesFull(ui,"font restored after failed raster repaints successfully");
        ui.SetFontDirectory(temp.Path/"OtherFont",true).Value();Write(temp.Path/"OtherFont"/"Local.ttf","invalid font");
        Check(!ui.Rasterize(320,240),"changing font directory never reuses an old file with the same relative name");
        ui.Undo().Value();MatchesFull(ui,"undo font directory restores its raster after failed replacement");
    }
    button.Layout.HeightSizing=UiSizing::Fixed;button.Layout.X=2000;button.Typography.FontFile="missing.ttf";ui.Set(id,button).Value();
    Check(!ui.Rasterize(320,240),"changed offscreen text still validates missing font");
    button.Typography.FontFile.clear();button.Layout.X=15;ui.Set(id,button).Value();
    MatchesFull(ui,"failed offscreen paint never publishes partial cache state");
    ui.Save(temp.File("ui.json")).Value();button.Text="Before load";ui.Set(id,button).Value();ui.Rasterize(320,240).Value();
    ui.Load(temp.File("ui.json")).Value();MatchesFull(ui,"load remaps IDs and replaces cached content");
    ui.Undo().Value();MatchesFull(ui,"undo load restores previous content and ID space");
    ui.BeginEdit().Value();button.Text="Cancelled text";button.Layout.Y=130;ui.Set(id,button).Value();ui.Rasterize(320,240).Value();
    ui.CancelEdit().Value();MatchesFull(ui,"cancel edit clears intermediate pixels and restores baseline");
#endif
}
void DifferentialUpdates() {
    UiCanvas ui;std::vector<UiId> ids;
    for (int i=0;i<8;++i) {auto e=Panel(float(i*22),float(20+i*11),50,40,{i/8.0f,.4f,.8f,.7f});e.Normal.Radius.fill(9);ids.push_back(ui.Add(e).Value());}
    ui.Rasterize(240,180).Value();std::minstd_rand random(413);
    for (int i=0;i<48;++i) {
        const auto id=ids[random()%ids.size()];auto e=ui.Get(id).Value();
        e.Layout.X=float(int(random()%250)-35);e.Layout.Y=float(int(random()%180)-20);
        e.Layout.Rotation=(int(random()%13)-6)*.12f;e.Layout.ScaleX=.4f+(random()%12)*.1f;e.Layout.ScaleY=.4f+(random()%12)*.1f;
        e.Normal.Opacity=(random()%11)*.1f;e.Layout.Z=int(random()%5);
        if (i%3==0) {e.Normal.Shadow={0,0,0,.5f};e.Normal.ShadowBlur=5;}
        ui.Set(id,e).Value();MatchesFull(ui,"deterministic affine/opacity/stacking update matches complete paint",240,180);
    }
    UiCanvas large;large.Add(Panel(5,5,20,20,{1,0,0,1})).Value();
    auto image=large.Rasterize(3000,3000).Value();
    Check(image[(10*3000+10)*4]==255&&image.back()==0,"image over retained-cache budget still exports correctly");
    MatchesFull(large,"returning from over-budget raster to smaller viewport remains correct",100,100);
}
}
int main() {
    try {DamageAndStacking();AncestorsAndViewport();InputAndText();DifferentialUpdates();}
    catch (const std::exception& error) {Check(false,error.what());}
    return Finish();
}
