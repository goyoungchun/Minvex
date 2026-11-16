#include <minvex/Viewer.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <numeric>

int main(int argc, char** argv) {
    try {
        if (argc != 5 && argc != 6) { throw std::runtime_error("minvex_ui_frame_probe model-or-cube none|static|dynamic|dense-static|dense-dynamic frames output.json [--animate]"); }
        const bool animate=argc==6;
        if(animate && std::string(argv[5])!="--animate") {throw std::runtime_error("unknown probe option");}
        const std::string mode=argv[2];
        std::size_t parsed=0;const int count=std::stoi(argv[3],&parsed),warmup=60;
        if(argv[3][parsed]!='\0') {throw std::runtime_error("invalid frame count");}
        if(count<1 || count>18000 || (mode!="none" && mode!="static" && mode!="dynamic" && mode!="dense-static" && mode!="dense-dynamic")) { throw std::runtime_error("invalid probe arguments"); }
        lv::ViewerOptions options;
        options.Title="Minvex QA frame probe";options.Width=1280;options.Height=720;
        options.VSync=false;options.HighDpi=false;options.EnableDefaultInput=false;
        options.Quality=lv::MakeQualityProfile(lv::QualityPreset::Low);
        options.Quality.Overrides.TargetFps=0;options.Quality.Overrides.OnDemand=lv::OnDemandMode::Off;
        lv::Viewer viewer(options);
        if(std::string(argv[1])=="cube") {
            if(animate) {throw std::runtime_error("cube has no animation");}
            auto model=std::make_shared<lv::ModelAsset>();
            model->Meshes.push_back({"Cube",{{lv::MakeCube(),0,false}}});model->Parts.push_back({0});model->Materials.push_back({});
            model->ModelBounds={{-.5f,-.5f,-.5f},{.5f,.5f,.5f}};
            viewer.GetScene().AddModel(model);
        } else {const auto object=viewer.AddModel(std::filesystem::path(argv[1])).Value();if(animate) {viewer.PlayAnimation(object,0).Value();}}
        viewer.FrameScene().Value();
        auto& ui=viewer.GetUi();lv::UiId label=0;
        if(mode!="none") {
            ui.BeginEdit().Value();
            const int elements=mode.starts_with("dense-") ? 64:1;
            for(int i=0;i<elements;++i) {
                lv::UiElement element;element.Text="Status "+std::to_string(i);
                element.Layout=elements==1 ? lv::UiLayout{20,20,360,60}:lv::UiLayout{float(20+(i%8)*155),float(20+(i/8)*80),140,60};
                element.Normal.Fill.fill({.1f,.15f,.25f,1});const auto added=ui.Add(element).Value();if(i==0){label=added;}
            }
            ui.CommitEdit().Value();ui.ClearHistory().Value();
        }
        using Clock=std::chrono::steady_clock;
        std::optional<Clock::time_point> previous,started;
        std::optional<std::uint64_t> previousFrame;
        bool measuring=false;
        std::vector<double> samples;std::vector<std::uint64_t> rss,logical;
        struct Work {double Callback=0,Telemetry=0,UiUpdate=0;};
        struct Row {std::uint64_t Frame;Work Previous;std::size_t InputInterruptions;};
        Work work;std::vector<Row> rows;std::vector<std::pair<double,int>> events;
        std::size_t duplicates=0,interruptions=0;
        samples.reserve(count);rows.reserve(count);rss.reserve(count/60+2);logical.reserve(count/60+2);
        events.reserve(64);
        const auto ms=[](auto duration) {return std::chrono::duration<double,std::milli>(duration).count();};
        lv::ViewerRunOptions run;
        run.OnInput=[&](lv::Viewer&,const lv::ViewerInputEvent& event) {
            if(measuring && (event.Type==lv::ViewerInputType::FocusLost || event.Type==lv::ViewerInputType::Resized)) {
                ++interruptions;
                if(events.size()<1024) {events.emplace_back(ms(Clock::now()-*started),static_cast<int>(event.Type));}
            }
            return false;
        };
        run.OnFrame=[&](lv::Viewer& current,const lv::ViewerFrameInfo& frame) {
            const auto now=Clock::now();
            if(!started) {started=now;}
            if(frame.FramebufferWidth!=1280 || frame.FramebufferHeight!=720) { throw std::runtime_error("framebuffer changed"); }
            // OnFrame runs before submission and can be retried with the same
            // frame number. Keep retry time in the next complete interval.
            if(previousFrame && frame.FrameNumber==*previousFrame) {++duplicates;return;}
            if((!previousFrame && frame.FrameNumber!=0) || (previousFrame && frame.FrameNumber!=*previousFrame+1)) {throw std::runtime_error("incomplete frame sequence");}
            // Fast scenes can submit sixty frames before the window is visible.
            // Warm up by elapsed time as well as frame count before comparing UI.
            if(previous && measuring) {samples.push_back(ms(now-*previous));rows.push_back({*previousFrame,work,interruptions});}
            if(!measuring && frame.FrameNumber>=std::uint64_t(warmup) && now-*started>=std::chrono::seconds(2)) {measuring=true;}
            previous=now;previousFrame=frame.FrameNumber;work={};
            if(measuring && (samples.empty() || samples.size()%60==0 || samples.size()==std::size_t(count))) {
                const auto before=Clock::now();
                const auto telemetry=current.GetTelemetry().Value();
                if(telemetry.System.ProcessResidentMemory.Value) {rss.push_back(*telemetry.System.ProcessResidentMemory.Value);}
                logical.push_back(telemetry.Resident.TotalBytes);
                work.Telemetry=ms(Clock::now()-before);
            }
            if(samples.size()==std::size_t(count)) {
                lv::SaveTelemetry(current.GetTelemetry().Value(),std::string(argv[4])+".telemetry.json").Value();
                current.RequestClose();return;
            }
            if(mode=="dynamic" || mode=="dense-dynamic") {
                const auto before=Clock::now();
                auto element=ui.Get(label).Value();element.Text="Frame "+std::to_string(frame.FrameNumber)+" - Minvex QA";
                ui.Set(label,element).Value();
                // Runtime status updates are not edits a host needs to undo.
                ui.ClearHistory().Value();
                work.UiUpdate=ms(Clock::now()-before);
            }
            work.Callback=ms(Clock::now()-now);
        };
        viewer.Run(run).Value();
        if(samples.size()!=std::size_t(count)) { throw std::runtime_error("incomplete frame capture"); }
        auto sorted=samples;std::sort(sorted.begin(),sorted.end());
        const double mean=std::accumulate(samples.begin(),samples.end(),0.0)/samples.size();
        const double p95=sorted[std::size_t(std::ceil(samples.size()*.95))-1];
        const auto over=std::count_if(samples.begin(),samples.end(),[](double ms){return ms>33.333333;});
        std::ofstream out(argv[4]);out<<std::setprecision(12)<<"{\"ui\":\""<<mode<<"\",\"width\":1280,\"height\":720,\"frames\":"<<samples.size()<<",\"meanMs\":"<<mean<<",\"p95Ms\":"<<p95<<",\"maxMs\":"<<sorted.back()<<",\"fps\":"<<1000/mean<<",\"over33ms\":"<<over;
        if(!rss.empty()) {out<<",\"rssStart\":"<<rss.front()<<",\"rssEnd\":"<<rss.back()<<",\"rssMax\":"<<*std::max_element(rss.begin(),rss.end());}
        out<<",\"logicalStart\":"<<logical.front()<<",\"logicalEnd\":"<<logical.back()<<",\"logicalMax\":"<<*std::max_element(logical.begin(),logical.end());
        out<<",\"animation\":"<<(animate ? "true":"false")<<",\"duplicateCallbacks\":"<<duplicates<<",\"inputInterruptions\":"<<interruptions<<",\"events\":[";
        for(std::size_t i=0;i<events.size();++i) {if(i){out<<',';}out<<"{\"elapsedMs\":"<<events[i].first<<",\"type\":"<<events[i].second<<'}';}
        out<<"],\"eventNote\":\"FocusLost also includes mouse leave; events require review and never erase over-budget frames\"}\n";out.close();
        std::ofstream csv(std::string(argv[4])+".csv");csv<<"frameMs,frame,previousCallbackMs,previousTelemetryMs,previousUiUpdateMs,outsideCallbackMs,inputInterruptions\n"<<std::setprecision(12);
        for(std::size_t i=0;i<samples.size();++i){const auto& r=rows[i];csv<<samples[i]<<','<<r.Frame<<','<<r.Previous.Callback<<','<<r.Previous.Telemetry<<','<<r.Previous.UiUpdate<<','<<std::max(0.0,samples[i]-r.Previous.Callback)<<','<<r.InputInterruptions<<'\n';}
        if(!out||!csv){throw std::runtime_error("cannot save probe output");}
        std::printf("%s: %.2f FPS, mean %.3f ms, p95 %.3f ms, max %.3f ms, >33.3 ms %zu/%zu\n",mode.c_str(),1000/mean,mean,p95,sorted.back(),std::size_t(over),samples.size());
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
