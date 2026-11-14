#pragma once
// Opt-in diagnostics only. No clocks, allocation or file IO in normal builds.
namespace lv::detail {
enum class FramePhase {Callback,Render,Prepare,UiRaster,UiUpload,UiDraw,Commit,NativeLayout,NativeTarget,NativeDraw,NativeCopy,Count};
}
#if defined(MINVEX_FRAME_PROFILING)
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#endif
namespace lv::detail {
inline std::uint64_t FrameThreadCycles() {
#if defined(_WIN32)
    ULONG64 cycles=0;QueryThreadCycleTime(GetCurrentThread(),&cycles);return cycles;
#else
    return 0;
#endif
}
struct FrameProfile {
    using Clock=std::chrono::steady_clock;
    static constexpr std::size_t Phases=static_cast<std::size_t>(FramePhase::Count);
    struct Row {
        std::uint64_t Frame=0,StartCycles=0,WorkCycles=0;
        double StartMs=0,WorkMs=0;
        std::array<double,Phases> Wall{};
        std::array<std::uint64_t,Phases> Cycles{};
    };
    inline static thread_local FrameProfile* Active=nullptr;
    Clock::time_point Start=Clock::now();
    std::vector<Row> Rows;
    Row* Current=nullptr;
    std::string Path;
    FrameProfile() {if (const auto path=std::getenv("MINVEX_FRAME_TRACE");path&&*path) {Path=path;Rows.reserve(24000);Active=this;}}
    ~FrameProfile() {
        Active=nullptr;
        if (Path.empty()) {return;}
        try {
            std::ofstream file(Path);file<<std::setprecision(12);
            file<<"frame,startMs,workMs,startCycles,workCycles";
            constexpr const char* names[]={"callback","render","prepare","uiRaster","uiUpload","uiDraw","commit","nativeLayout","nativeTarget","nativeDraw","nativeCopy"};
            for (auto name:names) {file<<','<<name<<"Ms,"<<name<<"Cycles";}file<<'\n';
            for (const auto& row:Rows) {file<<row.Frame<<','<<row.StartMs<<','<<row.WorkMs<<','<<row.StartCycles<<','<<row.WorkCycles;for (std::size_t i=0;i<Phases;++i) {file<<','<<row.Wall[i]<<','<<row.Cycles[i];}file<<'\n';}
            file.close();if (!file) {std::fprintf(stderr,"Cannot write frame trace.\n");}
        }catch (...) {std::fprintf(stderr,"Cannot write frame trace.\n");}
    }
};
struct ProfileFrame {
    FrameProfile* Profile=FrameProfile::Active;
    FrameProfile::Clock::time_point Begin;
    explicit ProfileFrame(std::uint64_t frame) {
        if (!Profile||Profile->Rows.size()==Profile->Rows.capacity()) {Profile=nullptr;return;}
        Begin=FrameProfile::Clock::now();Profile->Rows.emplace_back();Profile->Current=&Profile->Rows.back();
        auto& row=*Profile->Current;row.Frame=frame;row.StartMs=std::chrono::duration<double,std::milli>(Begin-Profile->Start).count();row.StartCycles=FrameThreadCycles();
    }
    ~ProfileFrame() {if (Profile) {auto& row=*Profile->Current;row.WorkMs=std::chrono::duration<double,std::milli>(FrameProfile::Clock::now()-Begin).count();row.WorkCycles=FrameThreadCycles()-row.StartCycles;Profile->Current=nullptr;}}
};
struct ProfileScope {
    FrameProfile::Row* Row=FrameProfile::Active ? FrameProfile::Active->Current:nullptr;
    FrameProfile::Clock::time_point Begin;
    std::uint64_t Cycles=0;
    FramePhase Phase;
    explicit ProfileScope(FramePhase phase):Phase(phase) {if (Row) {Begin=FrameProfile::Clock::now();Cycles=FrameThreadCycles();}}
    ~ProfileScope() {if (Row) {const auto index=static_cast<std::size_t>(Phase);Row->Wall[index]+=std::chrono::duration<double,std::milli>(FrameProfile::Clock::now()-Begin).count();Row->Cycles[index]+=FrameThreadCycles()-Cycles;}}
};
}
#else
#include <cstdint>
namespace lv::detail {
struct FrameProfile {~FrameProfile() {}};
struct ProfileFrame {explicit ProfileFrame(std::uint64_t) {} ~ProfileFrame() {}};
struct ProfileScope {explicit ProfileScope(FramePhase) {} ~ProfileScope() {}};
}
#endif
