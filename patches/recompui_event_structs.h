#ifndef DT_PATCHES_RECOMPUI_EVENT_STRUCTS_H
#define DT_PATCHES_RECOMPUI_EVENT_STRUCTS_H

// Shared ABI between the host-side UI code (lib/RecompFrontend/recompui) and
// guest-side (MIPS) patch code for UI callback events. recomp_run_ui_callbacks
// (lib/RecompFrontend/recompui/src/api/ui_api_events.cpp) builds one of these
// on the recompiled game's stack and calls into a game-registered callback
// with a pointer to it in $a1.
//
// This is an exact copy of RecompFrontend's own reference copy at
// lib/RecompFrontend/recompui/include/recompui/event_structs.h (which sits
// there as a template for games to copy into their own patches/ directory
// under this exact filename -- lib/RecompFrontend/recompui/src/elements/
// ui_types.h's EventType/DragPhase/MenuAction comment says as much: "must be
// kept in sync with patches/recompui_event_structs.h"). Keep this in sync
// with that file rather than hand-editing independently.

typedef enum {
    UI_EVENT_NONE,
    UI_EVENT_CLICK,
    UI_EVENT_FOCUS,
    UI_EVENT_HOVER,
    UI_EVENT_ENABLE,
    UI_EVENT_DRAG,
    UI_EVENT_RESERVED1, // Would be UI_EVENT_TEXT but text events aren't usable in mods currently
    UI_EVENT_UPDATE,
    UI_EVENT_NAVIGATE,
    UI_EVENT_MOUSE_BUTTON,
    UI_EVENT_MENU_ACTION,
    UI_EVENT_COUNT
} RecompuiEventType;

typedef enum {
    UI_DRAG_NONE,
    UI_DRAG_START,
    UI_DRAG_MOVE,
    UI_DRAG_END
} RecompuiDragPhase;

typedef enum {
    UI_MENU_ACTION_NONE,
    UI_MENU_ACTION_ACCEPT,
    UI_MENU_ACTION_APPLY,
    UI_MENU_ACTION_BACK,
    UI_MENU_ACTION_TOGGLE,
    UI_MENU_ACTION_TAB_LEFT,
    UI_MENU_ACTION_TAB_RIGHT
} RecompuiMenuAction;

typedef struct {
    RecompuiEventType type;
    union {
        struct {
            float x;
            float y;
        } click;

        struct {
            bool active;
        } focus;

        struct {
            bool active;
        } hover;

        struct {
            bool active;
        } enable;

        struct {
            float x;
            float y;
            RecompuiDragPhase phase;
        } drag;

        struct {
            RecompuiMenuAction action;
        } menu_action;
    } data;
} RecompuiEventData;

#endif // DT_PATCHES_RECOMPUI_EVENT_STRUCTS_H
