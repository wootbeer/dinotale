#ifndef DT_PATCHES_UI_FUNCS_H
#define DT_PATCHES_UI_FUNCS_H

// Fills recompui's "// TODO: Forced game includes" hook
// (lib/RecompFrontend/recompui/src/api/ui_api_events.cpp), which needs
// RecompuiEventData (and the enums it's built from) to compile regardless
// of whether this game has any real UI-driving patches yet -- see
// recompui_event_structs.h for the actual definitions and why they live in
// a separate header (shared with the MIPS-side patches/*.c build).
#include "recompui_event_structs.h"

// recomp_run_ui_callbacks (lib/RecompFrontend/recompui/src/api/ui_api_events.cpp)
// pumps recompui's queued per-frame UI callbacks. Declared here (rather than
// in patches.h) so both sides of the ABI -- the host-side definition in
// ui_api_events.cpp (which includes this same header) and the MIPS-side
// patches/*.c call site -- agree on the signature via patch_helpers.h's
// DECLARE_FUNC.
#include "patch_helpers.h"

DECLARE_FUNC(void, recomp_run_ui_callbacks);

#endif // DT_PATCHES_UI_FUNCS_H
