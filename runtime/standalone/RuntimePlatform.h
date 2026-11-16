#pragma once

#if !defined(SOKOL_APP_INCLUDED)
#include <sokol_app.h>
#endif

namespace lv::detail {
bool StandaloneMainThread() noexcept;
bool StandaloneApplicationAvailable() noexcept;
#if defined(MINVEX_FRAME_PROFILING)
// 1: Metal display link; 0: occluded-window fallback timer; -1: unavailable.
int StandalonePresentationState() noexcept;
#endif
void RunStandaloneApplication(const sapp_desc& description);
}
