// MagelightUI_API.h — the public API header other SKSE plugins compile
// against (SeverActions is the first consumer).
//
// Pattern: versioned structs of C function pointers served by the exported
// factory `Magelight_RequestApi(version)` — resolved at runtime via
// GetModuleHandle/GetProcAddress, so consumers need no import library and a
// missing Magelight.dll degrades to a null API (soft no-op). Versions are
// additive: MagelightApi2 is
// MagelightApi1's members followed by the v2 additions, so a consumer that
// needs the newer surface requests 2 and falls back to 1.
//
// ABI rules (published): C ABI only; every struct starts with its members in
// declaration order and only ever APPENDS; enums never renumber; an unknown
// version returns null; v-N stays served for at least two minor host
// versions after v-N+1 ships; a breaking change means a new DLL name and a
// new export, never an in-place change.
//
// Threading contract (identical to the host's internal one):
//  - Every function is callable from any thread; view/image mutations marshal
//    internally (render thread owns Ultralight; EnterUIMode/ExitUIMode post a
//    task to the game thread).
//  - JS listener callbacks fire on the RENDER thread mid-frame — snapshot
//    what you need, then post the game-state work with PostGameTask (0.30.1+).
//    The render thread is the game's main thread inside Present (VR, hosts
//    before 1.7.0, or Magelight.json "pageThread": false), or, by default on
//    host 1.7.0+, flat only, Magelight's own page thread, running with the game
//    thread (QueryCapability("pageThread") == 1): never assume either, the
//    player can change it between sessions. Never touch RE:: state or the
//    game's D3D11 immediate context from a render-thread callback (render
//    texture images on the game thread, then InvalidateImage), and guard any
//    state such a callback shares with your game-thread code.
//  - Never call SKSE's AddTask or AddUITask from a callback the host makes
//    inside a frame: JS listeners, DOM ready, VR callbacks, and for a
//    RenderThread mod its events and EvalJS results (and onLog for a call made
//    from one of these). They run inside the game's Present (or on Magelight's
//    page thread, where the same advice holds), and on Skyrim VR
//    SKSE also drains its task queues on game job threads, holding a queue's
//    lock until it is empty, so the post can wait forever on a job that waits
//    for the frame. PostGameTask hands the post to a host thread instead (task
//    lane only). On a host older than 0.30.1, post from a thread of your own.
//    The same holds for your OWN main-thread entry points (an input sink, an
//    engine menu, a Present hook, a Papyrus native): PostGameTask is hang-safe
//    only inside the host's callbacks, so hand those posts to a thread of your
//    own.
//  - GameThread events keep their order within one source, not across: a
//    page event (DOM ready, console, resize, destroy, render death) and a
//    UI-mode event can arrive in either order.
//  - "The game thread" below means wherever SKSE runs its tasks: the main
//    thread on SE/AE, possibly a game job thread on VR. RE:: state is safe
//    there as in any SKSE task.
//  - The UI-mode exit callback fires on the GAME thread, on every exit path
//    (programmatic, Escape, toggle key, load boundary, render-death).
//    Host 0.9.1+: registrations are MULTICAST — every plugin that calls
//    SetUIModeExitCallback hears every exit with the exiting ViewId and
//    filters by the views it owns (0.9.0 kept only the last registration).
//    There is no unregister in v1-v3; register a static function once.
//  - v4 mod events (ModDesc.onEvent): GameThread (default) = delivered as
//    an SKSE task, safe to touch game state, may still arrive shortly after
//    UnregisterMod returns (the host drops them, but keep `user` alive until
//    the next game-thread tick). RenderThread = called INLINE ON THE EMITTING
//    THREAD, not always the render thread: page/console/resize events come
//    from the render thread, UIModeEntered/Exited/Refused from the game
//    thread, FocusDenied from the requesting mod's thread, RenderDead from
//    whichever present thread saw the kill switch. With "pageThread" the
//    page thread is the render thread, so the handler can be entered from two
//    threads at once, and a page event can still enter it after UnregisterMod
//    has returned on another thread: keep `user` and the handler alive for
//    the rest of the process. Pick RenderThread only if your handler is
//    thread-agnostic and reentrant. A faulting handler disables that mod's
//    sink, never the overlay.
//  - ModDesc.onLog fires synchronously on the thread of the failing API
//    call, outside the host's locks (calling back into the API is allowed).
//  - const char* arguments are valid only for the duration of the call.
//
// Geometry (v1-v3): x/y are pixels from the swapchain's top-left; negative
// x/y anchor from the right/bottom edge. w == 0 && h == 0 = fullscreen
// (tracks the backbuffer). htmlPath is relative to Magelight's runtime dir,
// or an absolute Windows path (drive-letter form). v4's ViewDesc carries an
// explicit Anchor instead of the sign convention.
// Every path argument is UTF-8 (1.6.1; text that is not valid UTF-8 is read
// in the ANSI code page, which is how earlier hosts read all of them).
//
// Texture images (v2): render into a D3D11 texture you own on the GAME's
// device, register its SRV under a name, place ImageUrl(id) in an <img> or
// CSS url() — the page treats it as a normal image, and you InvalidateImage
// after each update. sRGB-encoded bytes behind a UNORM SRV, premultiplied
// alpha; bilinear/no-mip sampling (keep within ~2x the CSS box). GPU path
// only — IsGpuAccelerated() false means everything returns 0 / no-ops.
//
// Known engine limit (Ultralight 1.4 GPU path, both drivers): a ROUNDED box
// whose border sides differ (accent stripe, coloured top bar, top-only
// separator) draws its sides as filled wedges when the box is not fully
// opaque (translucent side colour, translucent/transparent background, or a
// translucent gradient layer). Keep such boxes opaque, or drop the radius.

#pragma once

#include <cstdint>

#ifndef MAGELIGHT_API_NO_LOADER
    #include <windows.h>
#endif

struct ID3D11ShaderResourceView;

namespace MAGELIGHT_API {

    using ViewId = std::uint64_t;  // 0 = invalid
    using ImageId = std::uint32_t; // 0 = invalid
    using JsListenerFn = void (*)(const char* argument);
    using DomReadyFn = void (*)(ViewId view);
    using UIModeExitFn = void (*)(ViewId view);

    inline constexpr std::uint32_t kApiVersion1 = 1;
    inline constexpr std::uint32_t kApiVersion2 = 2;
    inline constexpr std::uint32_t kApiVersion3 = 3;
    inline constexpr std::uint32_t kApiVersion4 = 4;

    struct MagelightApi1 {
        std::uint32_t apiVersion;   // = kApiVersion1
        const char* hostVersion;    // e.g. "0.8.0" (static storage)

        ViewId (*CreateView)(const char* htmlPath, int x, int y, int w, int h,
                             DomReadyFn onDomReady, bool clickThrough, bool startVisible);
        bool (*IsViewValid)(ViewId view);
        void (*ShowView)(ViewId view, bool show);
        void (*SetViewBounds)(ViewId view, int x, int y, int w, int h);
        void (*RegisterJSListener)(ViewId view, const char* name, JsListenerFn callback);
        void (*InteropCall)(ViewId view, const char* functionName, const char* argument);
        void (*InvokeJS)(ViewId view, const char* script);

        void (*EnterUIMode)(ViewId view);   // show + focus + cursor + control suspension
        void (*ExitUIMode)();               // drops UI mode; deliberately does NOT hide
        bool (*IsUIModeActive)();           // cross-surface mutex / hotkey gate
        void (*SetUIModeExitCallback)(UIModeExitFn cb);  // every exit path, game thread
    };

    // v1 members first (same layout), then the texture-image surface.
    struct MagelightApi2 {
        std::uint32_t apiVersion;   // = kApiVersion2
        const char* hostVersion;

        ViewId (*CreateView)(const char* htmlPath, int x, int y, int w, int h,
                             DomReadyFn onDomReady, bool clickThrough, bool startVisible);
        bool (*IsViewValid)(ViewId view);
        void (*ShowView)(ViewId view, bool show);
        void (*SetViewBounds)(ViewId view, int x, int y, int w, int h);
        void (*RegisterJSListener)(ViewId view, const char* name, JsListenerFn callback);
        void (*InteropCall)(ViewId view, const char* functionName, const char* argument);
        void (*InvokeJS)(ViewId view, const char* script);
        void (*EnterUIMode)(ViewId view);
        void (*ExitUIMode)();
        bool (*IsUIModeActive)();
        void (*SetUIModeExitCallback)(UIModeExitFn cb);

        bool (*IsGpuAccelerated)();
        ImageId (*RegisterTextureImage)(const char* name, ID3D11ShaderResourceView* srv,
                                        std::uint32_t width, std::uint32_t height);
        void (*UpdateTextureImage)(ImageId image, ID3D11ShaderResourceView* srv);  // re-point (resize)
        void (*InvalidateImage)(ImageId image);   // after every content change
        void (*UnregisterImage)(ImageId image);   // only once no page references it
        const char* (*ImageUrl)(ImageId image);   // file:/// URL of the .imgsrc; "" if invalid
    };

    // v2 members first (same layout), then: pausing UI mode. The host pushes
    // its own engine menu on UI-mode entry; pauseGame adds kPausesGame to it
    // (the world freezes, Papyrus keeps running — vanilla menu semantics).
    struct MagelightApi3 {
        std::uint32_t apiVersion;   // = kApiVersion3
        const char* hostVersion;

        ViewId (*CreateView)(const char* htmlPath, int x, int y, int w, int h,
                             DomReadyFn onDomReady, bool clickThrough, bool startVisible);
        bool (*IsViewValid)(ViewId view);
        void (*ShowView)(ViewId view, bool show);
        void (*SetViewBounds)(ViewId view, int x, int y, int w, int h);
        void (*RegisterJSListener)(ViewId view, const char* name, JsListenerFn callback);
        void (*InteropCall)(ViewId view, const char* functionName, const char* argument);
        void (*InvokeJS)(ViewId view, const char* script);
        void (*EnterUIMode)(ViewId view);
        void (*ExitUIMode)();
        bool (*IsUIModeActive)();
        void (*SetUIModeExitCallback)(UIModeExitFn cb);
        bool (*IsGpuAccelerated)();
        ImageId (*RegisterTextureImage)(const char* name, ID3D11ShaderResourceView* srv,
                                        std::uint32_t width, std::uint32_t height);
        void (*UpdateTextureImage)(ImageId image, ID3D11ShaderResourceView* srv);
        void (*InvalidateImage)(ImageId image);
        void (*UnregisterImage)(ImageId image);
        const char* (*ImageUrl)(ImageId image);

        void (*EnterUIModeEx)(ViewId view, bool pauseGame);
    };

    // ── v4: mod identity, result codes, events, view lifecycle ─────────────
    //
    // A consumer REGISTERS ITSELF once (RegisterMod) and receives a ModId that
    // scopes everything it creates: views, listeners, texture images, its
    // UI-mode ownership, its error string and its event sink. Two plugins can
    // no longer overwrite each other's hooks, and every failure names itself
    // (Result + GetLastErrorMessage) instead of returning 0.
    //
    // Version gate: ModDesc.minHostVersion is a packed MAJOR*10000 +
    // MINOR*100 + PATCH; an older host returns HostTooOld from RegisterMod
    // (the consumer then no-ops — never fail hard). hostVersionNumber at the
    // end of the struct is the running host's packed version.

    using ModId = std::uint32_t;   // 0 = invalid

    enum class Result : std::int32_t {
        Ok = 0,
        HostAbsent = 1,       // (consumer-side only: RequestApi returned null)
        HostTooOld = 2,       // ModDesc.minHostVersion > host
        NotReady = 3,         // before the renderer exists (world not ready)
        RenderDead = 4,       // the overlay disabled itself after a render fault
        InvalidView = 5,
        InvalidMod = 6,
        FileNotFound = 7,
        Busy = 8,             // UI mode is owned by another mod (see FocusDenied)
        Denied = 9,           // policy: e.g. a non-file:// URL, releasing UI mode you don't own
        Unsupported = 10,     // capability missing on this host/path
        InvalidArgument = 11,
        Internal = 12,
    };

    enum class CallbackThread : std::uint8_t {
        GameThread = 0,       // default: events as SKSE tasks — safe for game state
        RenderThread = 1,     // inline on the EMITTING thread (see the threading contract) — no RE:: access
    };

    // z-order tiers, low to high. Within a tier: creation order, RaiseView
    // moves to the top of its tier. Hud and click-through views are refused
    // UI mode (RequestUIMode → Denied).
    enum class Layer : std::uint8_t { Hud = 0, Panel = 1, Popup = 2, System = 3 };

    enum class Anchor : std::uint8_t { TopLeft = 0, TopRight = 1, BottomLeft = 2, BottomRight = 3 };
    // 0.28.2: what a mod's pages may reach over the network (SetNetworkPolicy).
    // File reads come from the Data\Magelight tree, the registered page folders
    // and the host runtime dir (no per-mod isolation). FileOnly (the default
    // since 0.30.0): nothing over the network. LoopbackOnly: also http(s) and
    // ws(s) to this machine (localhost, 127.0.0.1 - since 0.31.5 the policy is
    // a Content-Security-Policy written into each page, which cannot name
    // [::1] or other 127.x addresses); it was the default before 0.30.0, so a
    // mod that talks to a local server now opts in. Any: any host, any scheme —
    // the reach a page has in a browser. A manifest or a script may pick
    // FileOnly or LoopbackOnly; Any is a plugin's call. Each change is logged
    // and applies to a page on its next load.
    enum class NetworkPolicy : std::uint8_t {
        LoopbackOnly = 0,
        Any = 1,
        FileOnly = 2,   // 0.30.0 — gate on hostVersionNumber >= 3000 (an older host reads it as LoopbackOnly)
    };

    enum class Event : std::uint32_t {
        ViewDomReady = 0,     // page scripts ran; safe to InteropCall
        ViewLoadFailed = 1,   // detail = "<url>: <description> (<domain>:<code>)"
        ViewReloaded = 2,     // the page was replaced: a ReloadView/Navigate landed, a RebuildViewAtScale, or (1.8.0)
                              // the next load of a view the low-memory guard released (DOM ready again; re-send state)
        ViewDestroyed = 3,    // DestroyView completed on the render thread
        UIModeEntered = 4,    // view = the UI-mode view; detail "switched" when you retargeted an active mode
        UIModeExited = 5,     // every exit path (programmatic, Escape, key, load boundary, render-death)
        FocusDenied = 6,      // another mod asked for UI mode while you own it; detail = its modId
        DisplayResized = 7,   // x = width, y = height (backbuffer pixels)
        ConsoleMessage = 8,   // detail = every console argument stringified and space-joined (objects as
                              // JSON, an Error as its text, the JSON of its own fields when it has any, and its
                              // stack); x = level (0 log, 1 warning, 2 error);
                              // y = source line. Every message, whatever Magelight.log records of it
                              // (QueryCapability("consoleLog"), 0.31.0). Also an exception InvokeJS or an
                              // InteropCall handler left uncaught: x = 2, detail = its text and stack
                              // (QueryCapability("scriptErrors"), 1.5.0)
        RenderDead = 9,       // the overlay disabled itself for the session (also sent at RegisterMod if already dead)
        HostShutdown = 10,    // reserved
        UIModeRefused = 11,   // your RequestUIMode was not entered (a lost race, or the host refused it on the game
                              // thread); detail = why ("low memory" since 1.8.0, see docs/CPP.md)
        KeyFocusGained = 12,  // (1.9.0, SetViewFocusSharing) your view has the keyboard now, UI mode going on: a
                              // click on it, or it holds UI mode and the keyboard came back from a view it was shared
                              // with. x = the view that had it; detail = that view's modId ("" for none)
        KeyFocusLost = 13,    // (1.9.0) your view no longer has the keyboard (a click elsewhere took it, or it had it
                              // by a share that ended). x = the view that has it now, 0 when UI mode ended; detail =
                              // that view's modId ("" for none)
    };

    struct EventData {
        Event type;
        ViewId view;          // 0 when not view-specific
        std::int32_t x, y;    // event-specific integers (see Event)
        const char* detail;   // event-specific text ("" when none); valid for the call only
    };

    using EventFn = void (*)(const EventData* ev, void* user);
    using LogFn = void (*)(int level, const char* message, void* user);   // 0 info, 1 warning, 2 error
    using JsListenerFn4 = void (*)(ViewId view, const char* argument, void* user);
    // EvalJS completion: result = String(completion value) ("undefined" for undefined, "" for a value
    // with no string form such as a Symbol); exception = the thrown value's string, "" when the script
    // succeeded (or threw a value with no string form).
    using JsResultFn = void (*)(ViewId view, const char* result, const char* exception, void* user);
    // PostGameTask (0.30.1): runs once, on the game thread, with the user pointer it was posted with.
    using GameTaskFn = void (*)(void* user);

    struct ModDesc {
        std::uint32_t size;              // = sizeof(ModDesc) — forward-compat
        const char* modId;               // slug, [A-Za-z0-9_.-], 1..32 chars, unique per process (case-insensitive)
        const char* displayName;         // for user-facing messages; nullptr = modId
        std::uint32_t modVersion;        // packed, for logs
        std::uint32_t minHostVersion;    // packed; host refuses (HostTooOld) if older
        CallbackThread callbackThread;   // where onEvent runs (default GameThread)
        EventFn onEvent;                 // optional
        LogFn onLog;                     // optional: host log lines about THIS mod, mirrored
        void* user;                      // handed back to onEvent/onLog
        const char* sessionName;         // storage isolation for this mod's views: nullptr/"isolated" = own
                                         // persistent jar named by modId (localStorage/IndexedDB/cookies
                                         // never collide between mods); "default" = the shared session
                                         // v1-v3 views use; any other name = a jar shared by every mod
                                         // that names it and by a mod whose modId is that name (scripts
                                         // cannot drive a mod whose jar a plugin uses). Decided at
                                         // RegisterMod; a manifest mod's choice wins when a plugin adopts it
    };

    struct ViewDesc {
        std::uint32_t size;              // = sizeof(ViewDesc)
        const char* name;                // unique within the mod (for logs / GetViewInfo)
        const char* htmlPath;            // relative to Magelight's runtime dir, or absolute
        Anchor anchor;                   // which display corner x/y offset from
        std::int32_t x, y, w, h;         // offset from the anchor corner; w/h in pixels
        bool fullscreen;                 // tracks the backbuffer; x/y/w/h ignored
        bool clickThrough;               // HUD: never receives input, never focused
        bool startVisible;
        Layer layer;
        float uiScale;                   // Ultralight DEVICE scale (0 = 1.0): the page lays out and
                                         // rasterizes at this scale — real DPI, sharp — instead of a
                                         // CSS transform that resamples a 1x raster. Change at runtime
                                         // with SetViewScale (0.26.9). window.devicePixelRatio mirrors it.
                                         // Clamped 1.0..3.0 since 0.30.3: to shrink a page, use a CSS transform
        DomReadyFn onDomReady;           // optional; the ViewDomReady event carries the same
    };

    // Flags for RequestUIMode.
    inline constexpr std::uint32_t kUIModeFlagNone = 0;
    inline constexpr std::uint32_t kUIModeFlagPause = 1;   // = EnterUIModeEx(view, true)
    inline constexpr std::uint32_t kUIModeFlagQueue = 2;   // held by someone else → wait your turn instead of
                                                            // Busy: Ok now, UIModeEntered when the holder releases
                                                            // (FIFO; ReleaseUIMode withdraws your queued requests)
    inline constexpr std::uint32_t kUIModeFlagNoTextEntry = 4;   // (0.26.6) do NOT raise the engine's text-entry
                                                            // gate for this UI mode. For pages with no text
                                                            // fields (a radial, a yes/no card): on Skyrim VR the
                                                            // gate starts the engine's virtual keyboard device,
                                                            // which OpenComposite hooks to raise ITS keyboard.
    inline constexpr std::uint32_t kUIModeFlagReclaim = 8;  // (1.1.0) your own ReleaseUIMode has not run yet:
                                                            // withdraw it and keep the mode (retargeted to this
                                                            // view and pause) instead of losing it to that exit.
                                                            // No UIModeExited for the withdrawn release, and a
                                                            // UIModeEntered ("switched") confirms the reclaim,
                                                            // the same view included. Re-show a view you hid with
                                                            // the release. Ignored before 1.1.0 (gate on
                                                            // "uiModeCancel").
    inline constexpr std::uint32_t kUIModeFlagPassPolledKeys = 16;   // (1.8.0) while this UI mode lasts, other
                                                            // plugins' GetAsyncKeyState / GetKeyState read the
                                                            // keyboard as it is instead of all keys up (see
                                                            // PolledKeysConfig for a narrower rule). A switch to
                                                            // another of your views takes the new request's flag.
                                                            // Ignored before 1.8.0 (gate on "polledKeys").

    // Flags for SetViewFocusSharing (1.9.0).
    inline constexpr std::uint32_t kFocusShareGive = 1;   // while this view holds UI mode, a click on a view that
                                                            // takes moves the keyboard there
    inline constexpr std::uint32_t kFocusShareTake = 2;   // a click on this view takes the keyboard from a view
                                                            // that holds UI mode and gives

    // ── VR (0.18.0) ────────────────────────────────────────────────────────
    // Per-view placement in the headset. mode: 0 HeadLocked (glued to the head
    // — HUD widgets), 1 WorldLocked (placed once, never follows), 3 LazyFollow
    // (the default for panels: placed level in front, stays put until the gaze
    // drifts ~30 deg or the head moves ~0.5 m, then glides back). Zero for
    // distanceMeters/widthMeters keeps the host default for the view's layer.
    // Ignored on a flat runtime.
    struct VRPlacementDesc {
        std::uint32_t size;              // = sizeof(VRPlacementDesc)
        std::uint8_t  mode;
        float distanceMeters;            // metres in front of the head
        float widthMeters;               // panel width; height follows the texture aspect
        float heightOffset;              // metres above/below eye height
        float curvature;                 // reserved (IVROverlay_016 has no curvature)
        float autoCloseMeters;           // reserved
        std::uint32_t flags;             // kVRPlacement* (1.6.1); an older host accepts the larger struct and ignores it
    };
    // Place the view where the player is LOOKING, pitch included, tilted to face them (pitch held within 60 degrees
    // of level), instead of level at eye height. Applies when the view is placed (each show, a recentre); best with
    // WorldLocked, since a LazyFollow glide places it level again. QueryCapability("vrGaze").
    inline constexpr std::uint32_t kVRPlacementAtGaze = 1;

    // Bind a view to a CONTROLLER button (VR only). Polled from the runtime
    // every frame whether or not UI mode is on — that is how a view opens with
    // no keyboard — then gated on the game thread exactly like a key binding
    // (no engine menu, no console, no text entry; while a page holds focus only
    // ITS OWN binding fires, to close). A bare button also fires during
    // gameplay, so prefer a modifier. button = 0 clears the binding.
    inline constexpr std::uint32_t kVRButtonNone       = 0;
    inline constexpr std::uint32_t kVRButtonMenu       = 1;    // B / Y
    inline constexpr std::uint32_t kVRButtonGrip       = 2;
    inline constexpr std::uint32_t kVRButtonA          = 7;    // A / X
    inline constexpr std::uint32_t kVRButtonStickClick = 32;
    inline constexpr std::uint32_t kVRButtonTrigger    = 33;
    inline constexpr std::uint32_t kVRButtonTouchpad   = 35;
    // Callback form (BindVRHotkeyCallback). Runs on the PRESENT thread the
    // moment the button edge is seen — post game work with PostGameTask.
    using VRHotkeyFn = void (*)(ViewId view, void* user);
    // SetVRButtonListener (0.26.0). PRESENT thread (post with PostGameTask).
    using VRButtonEdgeFn = void (*)(std::uint32_t button, std::uint32_t heldOther,
                                    std::uint8_t hand, void* user);
    struct VRHotkeyDesc {
        std::uint32_t size;              // = sizeof(VRHotkeyDesc)
        std::uint32_t button;            // kVRButton*
        std::uint32_t modifier;          // kVRButton* held alongside; kVRButtonNone = bare
        std::uint32_t action;            // kHotkeyAction*
        std::uint8_t  hand;              // 0 either, 1 left, 2 right
    };

    // ── Per-view cursors (0.31.0) ──────────────────────────────────────────
    // SetViewCursor: the image the flat cursor shows over this view, one per
    // page state. The state follows the page's CSS cursor: kCursorPointer over
    // pointer/grab/grabbing, kCursorText over text, kCursorArrow everywhere else
    // (including cursor: url(...)). A missing pointer image falls back to the
    // arrow image; a missing text image keeps the host's I-beam.
    inline constexpr std::uint32_t kCursorArrow   = 0;
    inline constexpr std::uint32_t kCursorPointer = 1;
    inline constexpr std::uint32_t kCursorText    = 2;
    struct CursorDesc {
        std::uint32_t size;              // = sizeof(CursorDesc)
        std::uint32_t state;             // kCursorArrow / kCursorPointer / kCursorText
        const char* imagePath;           // PNG or DDS (anything Windows' image decoder reads), at most 256x256:
                                         // relative to the mod folder (Data/Magelight/<Mod>/ for a page there,
                                         // else the page's own folder; no ".."), or absolute. nullptr or ""
                                         // clears this state. "none" (kCursorArrow only): the page draws its own
                                         // pointer, so the host draws nothing over this view (flat; VR keeps
                                         // its pointer)
        float hotspotX, hotspotY;        // the pointer pixel, in IMAGE pixels from the top-left (CSS url() x y)
        float height;                    // drawn height in px at 1080p, scaled with the resolution (8-256);
                                         // 0 = the image's own height
        bool pressShrink;                // shrink about the hotspot while the button is down, like the host arrow
    };

    // ── Cursor tint (0.31.1) ───────────────────────────────────────────────
    // SetViewCursorTint recolours the host's DRAWN cursor over this view (the
    // arrow, its hover glow and the I-beam keep their shape, sharpness and the
    // player's cursorHeight) and the VR laser dot (lit core, ink rim). Each
    // colour is 0xAARRGGBB: alpha 0 keeps the host's colour, any other alpha
    // uses the colour, drawn opaque.
    struct CursorTint {
        std::uint32_t size;              // = sizeof(CursorTint)
        std::uint32_t lit;               // the arrow's lit facet (host: steel 0xFFD8CCB0); the VR dot's core
        std::uint32_t shade;             // the arrow's shaded facet (host: 0xFF7D6E52)
        std::uint32_t ink;               // the outline (host: 0xFF120E0A); the VR dot's rim
        std::uint32_t glow;              // the hover glow over a clickable element (host: brass 0xFFECC983)
        std::uint32_t ibeam;             // the I-beam over text (host: vellum 0xFFF0E4C9)
    };

    // ── Keyboard theme (0.31.9) ────────────────────────────────────────────
    // SetViewKeyboardTheme colours Magelight's own VR keyboard (one page shared
    // by every mod) while this view holds UI mode. Each colour is 0xAARRGGBB
    // like CursorTint: alpha 0 = not given, any other alpha uses the colour,
    // drawn opaque. panel through accent are required. The host fills in the
    // rest: action = accent when it has 3:1 contrast (WCAG) on key and on
    // keyHover, else text; danger = a red with 3:1 on key and keyHover, else
    // text; pressed = accent 20% over key; pressedText = text, or black or
    // white when text has under 4.5:1 on pressed; barHover = panel and key
    // half and half. A given action or danger under 3:1 on key or keyHover,
    // or a given pressedText under 4.5:1 on pressed, is replaced by its
    // fill-in (logged). Refused: text under 3:1 on key, keyHover or panel. An
    // accent under 3:1 on key is kept, with a warning in the log.
    struct KeyboardTheme {
        std::uint32_t size;              // = sizeof(KeyboardTheme); the 0.31.9 size or more (later fields append)
        std::uint32_t panel;             // the keyboard's background (host: 0xFF1E1913)
        std::uint32_t panelBorder;       // its border, the drag grip, a key under the laser's border (host: 0xFF5A4A2E)
        std::uint32_t key;               // a key (host: 0xFF292118)
        std::uint32_t keyBorder;         // a key's border; the title bar's border under the laser (host: 0xFF3B3221)
        std::uint32_t keyHover;          // a key under the laser (host: 0xFF35291C)
        std::uint32_t text;              // key labels (host: 0xFFE8DCC4)
        std::uint32_t muted;             // the title bar's hints (host: 0xFF8A7A5A)
        std::uint32_t accent;            // the title, Shift and Caps, a pressed key's border (host: 0xFFCBA560)
        std::uint32_t action;            // Bksp, Tab, Enter, Esc and the arrows (host: 0xFFE0925A)
        std::uint32_t danger;            // Close (host: 0xFFA3472B)
        std::uint32_t pressed;           // a key held down, Shift or Caps on, the bar while dragged (host: 0xFF4A3A24)
        std::uint32_t pressedText;       // the label of a key held down, and of Shift or Caps while on (host: 0xFFFFF3D8)
        std::uint32_t barHover;          // the title bar under the laser (host: 0xFF251D14)
    };

    // ── Gamepad layer (1.6.0) ──────────────────────────────────────────────
    // SetViewGamepad opts a view in: while it holds UI mode on flat, the pad is
    // its page's and kept from the game. kGamepadStyleKeys types the pad as
    // keys (south/A Enter, east/B Escape, d-pad and left stick the arrows, the
    // right stick a mouse wheel under the pointer), so a
    // page that works with a keyboard works with a pad; kGamepadStyleNavigation
    // hands the page actions, which window.magelight.gamepad turns into focus
    // moves between the controls of the scopes the page attaches (docs/SDK.md).
    // B follows Escape's rule either way: it closes the view unless the page
    // took Escape (SetEscapeCapture / '__escapecapture'), and then the page
    // gets it (Escape, or the cancel action).
    inline constexpr std::uint32_t kGamepadStyleKeys = 0;
    inline constexpr std::uint32_t kGamepadStyleNavigation = 1;
    inline constexpr std::uint32_t kGamepadModeNavigation = 0;
    inline constexpr std::uint32_t kGamepadModeCursor = 1;     // the left stick moves the cursor, south/A clicks
    inline constexpr std::uint32_t kGamepadGlyphsXbox = 0;
    inline constexpr std::uint32_t kGamepadGlyphsPlayStation = 1;
    inline constexpr std::uint32_t kGamepadGlyphsGeneric = 2;
    // Controls, for GamepadConfig::bindings: 1 dpadUp, 2 dpadDown, 3 dpadLeft,
    // 4 dpadRight, 5 start, 6 back, 7 leftThumb, 8 rightThumb, 9 leftShoulder,
    // 10 rightShoulder, 11 south (A), 12 east (B), 13 west (X), 14 north (Y).
    struct GamepadConfig {
        std::uint32_t size;              // = sizeof(GamepadConfig)
        bool enabled;                    // false = the pad is the game's again (as without a config)
        std::uint32_t style;             // kGamepadStyle*
        bool allowCursor;                // right-stick click toggles cursor mode
        bool startInCursorMode;          // needs allowCursor
        // One control per action, 0 = the default; actions in this order: up, down, left, right (default the
        // d-pad), accept (south), cancel (east), previousTab (left shoulder), nextTab (right shoulder), secondary
        // (west), tertiary (north), toggleCursor (right-stick click). Each control on one action at most, and a
        // slot left 0 keeps its default control, which counts: moving a control to another action means
        // rebinding the action that had it. No action can be left unbound.
        std::uint8_t bindings[11];
        std::uint8_t reserved[3];        // must be 0
        // 1.6.0 ends here (size 28); fields a later host adds start at offset 28.
    };
    struct GamepadState {
        std::uint32_t size;              // = sizeof(GamepadState), set by the caller
        bool enabled;                    // the view opted in and the layer can run (flat, Magelight.json allows it)
        bool connected;                  // a gamepad is connected and enabled in the game
        bool capturing;                  // the view holds the pad right now
        std::uint32_t mode;              // kGamepadMode*
        std::uint32_t glyphs;            // kGamepadGlyphs*: the prompt labels the player chose
        std::uint64_t generation;        // changes whenever the pad changes hands or reconnects
    };

    // ── Polled keys (1.8.0) ───────────────────────────────────────────────
    // While a page holds UI mode, another SKSE plugin that reads keys with
    // GetAsyncKeyState / GetKeyState gets every keyboard key as up, so typing
    // into the page cannot fire its hotkeys (your own DLL and the mouse
    // buttons are never hidden). SetViewPolledKeys lets reads through while
    // this view holds UI mode: those of the plugins named in modules, of the
    // keys named in keys, or of both at once (a read passes when its plugin
    // AND its key are listed; an empty list matches all, so both empty lets
    // every read through). A side-neutral VK_SHIFT, VK_CONTROL or VK_MENU also
    // covers its left and right keys.
    struct PolledKeysConfig {
        std::uint32_t size;              // = sizeof(PolledKeysConfig)
        std::uint32_t moduleCount;       // at most 32; 0 = every plugin
        const char* const* modules;      // DLL file names, UTF-8, no path, case-insensitive ("VoiceChat.dll")
        std::uint32_t keyCount;          // at most 64; 0 = every key
        std::uint32_t reserved;          // must be 0
        const std::uint16_t* keys;       // Windows virtual-key codes (VK_*, 1-254), not DirectInput scancodes
    };

    // Actions for BindHotkey (one binding per DirectInput scancode, process-wide).
    inline constexpr std::uint32_t kHotkeyActionUnbind = 0;             // scancode 0 = every binding of the view
    inline constexpr std::uint32_t kHotkeyActionToggleUIMode = 1;       // RequestUIMode / ReleaseUIMode + hide
    inline constexpr std::uint32_t kHotkeyActionToggleUIModePaused = 2; // same, with kUIModeFlagPause
    inline constexpr std::uint32_t kHotkeyActionToggleVisible = 3;      // ShowView on/off (the only action a Hud view may bind)

    // Trust model: this ABI carries no caller identity. Any consumer that
    // knows a ViewId can act on it — ShowView/Navigate/DestroyView/EvalJS and
    // listener registration — including ids owned by another mod (views are
    // addressable, not owned; the cooperative model is deliberate). What the
    // host DOES scope per mod: file reads, storage sessions, network policy,
    // hotkeys. Since 0.30.0 the Papyrus tier cannot touch a mod registered
    // through this API (its slug or its views) or a script-tier mod whose
    // storage jar a plugin uses, and a page's listener calls act only as that
    // page's own view. See docs/CPP.md "Trust model".
    struct MagelightApi4 {
        std::uint32_t apiVersion;   // = kApiVersion4
        const char* hostVersion;

        // v3 surface, identical layout
        ViewId (*CreateView)(const char* htmlPath, int x, int y, int w, int h,
                             DomReadyFn onDomReady, bool clickThrough, bool startVisible);
        bool (*IsViewValid)(ViewId view);
        void (*ShowView)(ViewId view, bool show);
        void (*SetViewBounds)(ViewId view, int x, int y, int w, int h);
        void (*RegisterJSListener)(ViewId view, const char* name, JsListenerFn callback);
        void (*InteropCall)(ViewId view, const char* functionName, const char* argument);
        void (*InvokeJS)(ViewId view, const char* script);
        void (*EnterUIMode)(ViewId view);
        void (*ExitUIMode)();
        bool (*IsUIModeActive)();
        void (*SetUIModeExitCallback)(UIModeExitFn cb);
        bool (*IsGpuAccelerated)();
        ImageId (*RegisterTextureImage)(const char* name, ID3D11ShaderResourceView* srv,
                                        std::uint32_t width, std::uint32_t height);
        void (*UpdateTextureImage)(ImageId image, ID3D11ShaderResourceView* srv);
        void (*InvalidateImage)(ImageId image);
        void (*UnregisterImage)(ImageId image);
        const char* (*ImageUrl)(ImageId image);
        void (*EnterUIModeEx)(ViewId view, bool pauseGame);

        // v4 additions
        Result (*RegisterMod)(const ModDesc* desc, ModId* outMod);  // same modId as a manifest mod = ADOPT it
        void   (*UnregisterMod)(ModId mod);                 // releases UI mode, then destroys its views (game-thread
                                                            // tasks; no ViewDestroyed events reach the gone mod)
        Result (*CreateViewEx)(ModId mod, const ViewDesc* desc, ViewId* outView);
        Result (*DestroyView)(ViewId view);                 // async; Busy if it is the UI-mode view
        Result (*ReloadView)(ViewId view);                  // ViewReloaded follows DOM ready
        Result (*Navigate)(ViewId view, const char* url);   // file:/// (or a path); http = Denied. The page's
                                                            // own URL with only its #fragment added or changed
                                                            // keeps the page: no DOM ready, no ViewReloaded,
                                                            // calls keep flowing (1.5.0)
        Result (*RegisterJSListenerEx)(ViewId view, const char* name, JsListenerFn4 cb, void* user);
                                                            // "magelight" and names starting "__" belong to
                                                            // the host's page script: InvalidArgument (0.30.0;
                                                            // RegisterJSListener drops them the same way)
        Result (*RequestUIMode)(ViewId view, std::uint32_t flags);   // Ok = entry queued: UIModeEntered confirms,
                                                            // UIModeRefused says it was not entered. Busy → the
                                                            // holder hears FocusDenied. Hud/click-through → Denied.
                                                            // While YOU hold UI mode, another of your views
                                                            // switches in place (UIModeEntered, detail "switched")
        Result (*ReleaseUIMode)(ModId mod);                 // only the owner may release
        ModId  (*GetUIModeOwner)();                         // 0 = none
        Result (*RaiseView)(ViewId view);                   // to the top of its layer
        Result (*GetViewInfo)(ViewId view, ViewDesc* out);  // v4-owned views; out->size set by caller; anchor
                                                            // reconstructed so the desc round-trips; startVisible
                                                            // = current visibility; strings live until DestroyView;
                                                            // htmlPath is UTF-8 (1.6.1; earlier hosts gave ANSI
                                                            // for manifest and Papyrus views)
        void   (*GetDisplaySize)(std::int32_t* w, std::int32_t* h);   // 0,0 before the first frame
        std::int32_t (*QueryCapability)(const char* name);  // 1/0. Case-insensitive here. The authoritative
                                                            // name list is QueryCapability() in the host; as of
                                                            // 1.1.0: "gpu" "textureImage" "clipPathHole"
                                                            // "pause" "events" "clipboard" "networkDeny"
                                                            // "sessions" "manifest" "http" "vr" "hotkeys"
                                                            // "evaljs" "pagebridge" "cutout" "hibernate"
                                                            // "inspector" "ime" (0.27.0) "loopback"
                                                            // "escapeCapture" "viewOrder" "scrollStep" (0.28.0)
                                                            // "networkPolicy" (0.28.2) "sound" (0.29.0)
                                                            // "freezeWorld" "consoleLog" (0.31.0: not 1/0 but
                                                            // which page console messages Magelight.log
                                                            // records, 0 none, 1 errors, 2 warnings and
                                                            // errors, 3 all) "loadStagger" "loadOnShow"
                                                            // "cursor" (0.31.0) "cursorTint" (0.31.1) "csp"
                                                            // (0.31.5) "rebuildScale" (0.31.7)
                                                            // "keyboardTheme" (0.31.9), "prepaint" (0.31.11),
                                                            // "uiModeCancel" "hostThemeReply" (1.1.0),
                                                            // "tooltips" (1.2.0), "scriptErrors"
                                                            // "pageVisibility" "keyboardCode"
                                                            // "auxButtons" "imeCandidates" (1.5.0;
                                                            // "ime" is 0 in VR from 1.5.0), "gamepad"
                                                            // (1.6.0; 0 in VR), "vrGaze" (1.6.1), "pageThread" (1.7.0: 1 when
                                                            // render-thread callbacks run, or once the renderer
                                                            // starts will run, on Magelight's page thread; it can
                                                            // turn 0 for the session when the renderer starts,
                                                            // never back; see the threading contract; 0 in VR),
                                                            // "mainMenu" (1.7.1: follows "pageThread"),
                                                            // "polledKeys" (1.8.0), "focusShare" (1.9.0). The
                                                            // page-injected window.__MAGELIGHT__.capabilities
                                                            // (SDK: host.can) and the SDK mock carry this same
                                                            // set under the camelCase spellings shown (the page
                                                            // side is a plain key lookup).
        const char* (*GetLastErrorMessage)(ModId mod);      // per-mod storage, valid until the mod's next failing
                                                            // call or UnregisterMod — copy it; "" if none. Read it
                                                            // on the thread that made the failing call, right
                                                            // after it: with "pageThread" a failing call made from
                                                            // a render-thread callback can replace it meanwhile
                                                            // (ModDesc.onLog hands each message over on the
                                                            // failing call's own thread)
        Result (*BindHotkey)(ViewId view, std::uint32_t dxScancode, std::uint32_t action);  // kHotkeyAction*; Busy names
                                                            // the mod holding the key, Denied = the host's toggle key.
                                                            // Fires on key-down outside engine menus/console/text entry;
                                                            // while a page has key focus only ITS OWN key fires (close),
                                                            // and not even that while it has shared the keyboard
                                                            // (1.9.0); a ToggleVisible binding fires either way
        Result (*RegisterTextureImageEx)(ModId mod, const char* name, ID3D11ShaderResourceView* srv,
                                         std::uint32_t width, std::uint32_t height, ImageId* out);

        std::uint32_t hostVersionNumber;                    // packed MAJOR*10000 + MINOR*100 + PATCH

        // ── appended in 0.14.0 — gate on hostVersionNumber >= 1400 ──
        Result (*EvalJS)(ViewId view, const char* script, JsResultFn cb, void* user);   // runs on the page's
                                                            // JS thread next frame; cb on your callback thread.
                                                            // The synchronous twin of InvokeJS: read state back,
                                                            // catch exceptions (InvokeJS only logs them)

        // ── appended in 0.16.0 — gate on hostVersionNumber >= 1600 ──
        Result (*SetViewCutout)(ViewId view, std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h);
                                                            // the compositor DISCARDS this rect of the view (view
                                                            // pixels) so the game shows through; w or h <= 0 clears.
                                                            // Works on the GPU and CPU paths alike (no clip-path)
        Result (*SetViewHibernate)(ViewId view, std::uint32_t idleMs);   // hidden this long -> the View and its
                                                            // texture are released; ShowView(true) reloads the page,
                                                            // calls sent meanwhile land after DOM ready, and your
                                                            // onDomReady/ViewDomReady fire again. 0 = never, but
                                                            // while the game is low on memory (1.8.0) every hidden
                                                            // view is released whatever idleMs is (docs/CPP.md)

        // ── appended in 0.18.0 (VR-4) — gate on hostVersionNumber >= 1800 ──
        // No-ops on a flat runtime; QueryCapability("vr") says which you got.
        Result (*SetViewVRPlacement)(ViewId view, const VRPlacementDesc* desc);   // nullptr = host defaults
        Result (*GetViewVRPlacement)(ViewId view, VRPlacementDesc* out);          // out->size set by the caller
        Result (*RecenterVRView)(ViewId view);                                    // re-place in front of the head now
        Result (*BindVRHotkey)(ViewId view, const VRHotkeyDesc* desc);            // nullptr/button 0 clears

        // ── appended in 0.21.0 — gate on hostVersionNumber >= 2100 ──
        // The same binding, but it calls YOU instead of running desc->action.
        // Needed by any mod whose open/close does more than show a view: SA
        // gathers page data, snapshots the pause setting and raises a text-
        // entry gate, none of which the host's built-in toggle knows about,
        // so a built-in binding would desync its bookkeeping. desc->action is
        // ignored. cb == nullptr (or button 0) clears the binding.
        Result (*BindVRHotkeyCallback)(ViewId view, const VRHotkeyDesc* desc,
                                       VRHotkeyFn cb, void* user);

        // ── appended in 0.26.0 — gate on hostVersionNumber >= 2600 ──
        // Raw controller button EDGES, so a mod can let the user pick a chord
        // by pressing it (the keyboard "click, then press a key" flow, for
        // controllers). While a listener is set, each button-down on either
        // hand is reported once with `heldOther` = one other kVRButton* held
        // on that hand at that moment (0 = none) — so "hold Grip, press B"
        // arrives as button=kVRButtonMenu, heldOther=kVRButtonGrip. All view
        // hotkey bindings are SUPPRESSED while a listener is set, so the
        // press being captured cannot also toggle a menu. Runs on the
        // PRESENT thread. cb == nullptr clears it.
        Result (*SetVRButtonListener)(VRButtonEdgeFn cb, void* user);

        // ── appended in 0.26.8 — gate on hostVersionNumber >= 2608 ──
        // Resolve a view NAME to its ViewId within your mod (or a manifest mod
        // your DLL adopted with the same modId). The C++ twin of the Papyrus
        // FindView — needed to drive a manifest-declared view from C++, since
        // every other call takes a ViewId. Ok + *out set on a hit; InvalidView
        // if the name is not one of your mod's views. *out is 0 on any failure.
        Result (*FindView)(ModId mod, const char* name, ViewId* out);

        // ── appended in 0.26.9 — gate on hostVersionNumber >= 2609 ──
        // Set the view's Ultralight device scale at runtime (see ViewDesc::uiScale).
        // The page lays out again at the next frame and keeps its state; its
        // devicePixelRatio becomes `scale`. Before 0.28.5 the host made the change
        // on the caller's thread, where it crashed the game for a consumer, and it
        // has not run in game since: on a page that has loaded, prefer
        // RebuildViewAtScale (0.31.7) until it has. Offline it draws like a page
        // loaded at `scale`, except from 1.5.0 (Ultralight 2.0) where the view's
        // width or height divided by `scale` is not whole: a View resized or
        // rescaled after it was created (this call, SetViewBounds, a fullscreen
        // view's new size) then has its CSS size rounded up where a View created
        // at that size and scale rounds down, so the two lay out a pixel apart
        // (docs/CPP.md). From 1.5.0 the scale the view already has changes
        // nothing. Before the first load, and on a hibernated view, it only
        // stores the scale for the next load. Cutout and
        // image rects are in VIEW PIXELS = CSS px * scale. Clamped 1.0..3.0 (0.5..3.0 before
        // 0.30.3; below 1 Ultralight clipped the page): shrink a page with a CSS transform.
        Result (*SetViewScale)(ViewId view, float scale);
        // ── 0.27.0 (no new fields): native IME. Text fields in the UI-mode view take
        //    CJK composition: the window's IME context attaches on field focus (the
        //    host follows the page's focus — pages need NO code), the pre-edit is drawn
        //    inline in the field, and keys the IME consumed never reach the page. No OS
        //    IME window shows (none draws over the game): from 1.5.0 the page core draws
        //    the candidate list in the page at the composition (opt out with
        //    data-ml-ime="off" or by cancelling the 'magelight:ime' window event, which
        //    carries {state, text} and from 1.5.0 the whole state: docs/CPP.md "IME").
        //    QueryCapability("ime") == 1 and, from 1.5.0, ("imeCandidates") == 1. Flat
        //    only (VR: the runtime keyboard owns text; both are 0 there from 1.5.0).
        // ── 0.26.11 (no new fields): RequestUIMode on the view that ALREADY holds
        //    UI mode retargets the PAUSE flag (kUIModeFlagPause) IN PLACE — the live
        //    focus menu's kPausesGame and the engine's pause counter are adjusted;
        //    no menu is hidden or re-shown, so your close detection never fires.
        //    (0.26.10 did it by hide + show, which tripped exactly that — withdrawn.)
        //    Before 2610 a same-view re-request returned Ok and kept the old pause.
        //    Gate on hostVersionNumber >= 2611 if you rely on it.
        // ── appended in 0.28.0 — gate on hostVersionNumber >= 2800 ──
        // The page owns Escape: while set, Escape is delivered to the page as a
        // key (keydown/keyup) and does NOT leave UI mode — for a modal or an
        // editing control that closes itself. Clear it to hand Escape back.
        // A page can do the same without C++: magelight.send('__escapecapture','1'|'0').
        // From 1.9.0 the keyboard's Escape asks the view that has the keyboard
        // (SetViewFocusSharing); the gamepad's B asks the view holding UI mode.
        Result (*SetEscapeCapture)(ViewId view, bool capture);
        // Host the Web Inspector for a page (needs the runtime's inspector assets).
        Result (*ShowInspector)(ViewId view, bool show);
        Result (*IsInspectorVisible)(ViewId view, bool* outVisible);
        // Explicit z-order within the view's layer (higher draws later / on top);
        // RaiseView is "above everything so far". GetViewOrder reads it back.
        Result (*SetViewOrder)(ViewId view, std::uint64_t order);
        Result (*GetViewOrder)(ViewId view, std::uint64_t* outOrder);
        // Pixels scrolled per wheel notch for this view (default 40; 120 is a
        // common alternative).
        Result (*SetScrollStep)(ViewId view, int px);
        // ── appended in 0.28.2 — gate on hostVersionNumber >= 2802 ──
        // Network reach for EVERY page of this mod, existing views included.
        // Default FileOnly (0.30.0 — gate on hostVersionNumber >= 3000; before
        // it the default was LoopbackOnly). A mod whose page talks to a server
        // on this machine asks for LoopbackOnly; one whose dashboard talks to
        // the internet (model catalogs, plugin indexes, a user-configured
        // provider) asks for Any. Call it once after RegisterMod; other mods
        // keep the sandbox untouched. An unknown value is InvalidArgument.
        Result (*SetNetworkPolicy)(ModId mod, NetworkPolicy policy);
        // ── appended in 0.29.0 — gate on hostVersionNumber >= 2900 ──
        // UI sounds. Ultralight has no media stack (<audio> and Web Audio do
        // nothing), so the host plays through the game's own audio, which also
        // respects the player's UI volume. `name`: a built-in — "ok"/"click",
        // "cancel", "prevnext", "focus"/"hover", "open", "close", "inactive"
        // (the vanilla UIMenu* descriptors), ANY vanilla UI*/ITM* descriptor by
        // its EditorID ("UIJournalOpen", "ITMGoldUpSD" — 134, case-insensitive),
        // "none" for explicit silence — or "Plugin.esp|0xFormID" for any
        // SNDR a mod ships. Unknown names log once and stay silent. A page can
        // do the same without C++: magelight.sound('click'), or the markup
        // convention data-ml-sound="click" / data-ml-sound-hover="focus" the
        // injected bridge wires up (hover throttled host-side; page-driven
        // sounds play only while the view is visible). QueryCapability("sound").
        Result (*PlayUISound)(ViewId view, const char* name);
        // Played by the host when this view enters / leaves UI mode; "" or
        // nullptr clears either side. Manifest: "sounds": { "open": "open", "close": "close" }.
        Result (*SetViewSounds)(ViewId view, const char* open, const char* close);
        // ── appended in 0.30.1 — gate on hostVersionNumber >= 3001 ──
        // Run fn(user) once on the game thread as an SKSE task. Any thread may
        // call. From a callback inside a frame the post goes to a host thread
        // that hands it to SKSE, so the caller never waits on SKSE's task lock
        // (see the threading contract), and such posts run in call order
        // together with the host's own work from the same frames (UI-mode calls
        // made there included). Anywhere else it goes straight to SKSE, in
        // order with your own AddTask calls there; such a post can run before
        // an earlier in-frame one. fn is a plain function pointer: box what it
        // needs on the heap, pass it as user (handed back untouched, no cancel)
        // and free it in fn. Needs no registered mod. Ok = queued; a post SKSE
        // refuses is logged and fn never runs; a fault in fn is logged with its
        // module and skipped. Null fn: InvalidArgument; before SKSE's task
        // interface exists: NotReady.
        Result (*PostGameTask)(GameTaskFn fn, void* user);
        // ── appended in 0.31.0 — gate SetViewFreezeWorld on hostVersionNumber >= 3100. Unreleased 0.31.0 test
        // builds lack SetViewLoadOnShow and SetViewCursor: gate those two on >= 3101, or on >= 3100 together with
        // QueryCapability("loadonshow") / ("cursor") == 1 ──
        // The game skips its 3D world render behind this view (a frozen frame
        // shows instead) while the view holds UI mode with kUIModeFlagPause. Any
        // thread; a per-view preference kept until changed or the view is
        // destroyed, applied and dropped by the host as pause and focus move.
        // Flat only: QueryCapability("freezeworld") == 0 and Unsupported on VR
        // or when the player set Magelight.json "freezeWorld": false.
        Result (*SetViewFreezeWorld)(ViewId view, bool freeze);
        // The view's FIRST page load waits until it is shown (ShowView(true), or
        // UI-mode entry, which shows it); DOM ready, and every call queued for
        // the page, follow that load. For panels that are rarely opened: no
        // load cost at all until then. Decides only a load that has not
        // started: call it right after CreateViewEx, before the world loads
        // (a view created mid-session may already be loading; Ok either way,
        // a loaded page stays loaded). Never wait for DOM ready before showing
        // such a view. Any thread. Manifest key "loadOnShow".
        Result (*SetViewLoadOnShow)(ViewId view, bool onShow);
        // Your own cursor over this view (see CursorDesc): one call per state,
        // kept until changed or the view is destroyed. desc == nullptr clears
        // every state, back to the host cursor (or the mod's manifest default,
        // which a view without a cursor of its own uses). The image decodes on
        // a worker the first time it is drawn; until then, and if it fails
        // (logged once), the host cursor shows. The player can override it
        // (Magelight.json "cursorForce" / "modCursors"). InvalidArgument for a
        // bad size or state, a relative path that leaves the mod folder, or
        // a missing file. Any thread. QueryCapability("cursor"). Manifest key
        // "cursor".
        Result (*SetViewCursor)(ViewId view, const CursorDesc* desc);
        // ── appended in 0.31.1 — gate on hostVersionNumber >= 3101 ──
        // The host's drawn cursor over this view in your colours (see
        // CursorTint), kept until changed or the view is destroyed; nullptr
        // gives the host's colours back. The view's own images (SetViewCursor,
        // the manifest's "cursor") still win over the tint; on flat the
        // player's cursorFile replaces the drawn cursor, and "cursorForce" and
        // "modCursors": false drop the tint. The VR dot takes the new colours
        // at the panel's next redraw. Any thread. InvalidArgument for a bad
        // size. QueryCapability("cursortint").
        Result (*SetViewCursorTint)(ViewId view, const CursorTint* tint);
        // ── appended in 0.31.7 — gate on hostVersionNumber >= 3107 ──
        // Change a loaded page's device scale by REBUILDING it: at its next
        // frame the host releases the view's Ultralight View and loads the
        // page again in a new one created at `scale` (clamped 1.0..3.0, as
        // SetViewScale), so it lays out and rasterizes at that DPI from its
        // first paint. The page reloads: its script state is gone (keep what
        // it must survive in localStorage), calls you make meanwhile wait for
        // its DOM ready, and your onDomReady, ViewDomReady and ViewReloaded
        // fire again; send whatever page state you set only at open again on
        // one of those. What the host keeps per view stays: bounds,
        // visibility, UI mode (and its focus; a focused text field's keyboard
        // claims are dropped with the old page), listeners, cursor, sounds,
        // session, VR placement (a placed panel keeps its pose), and an escape
        // capture the old page set (a page that set one clears it before
        // asking). The cutout stays in VIEW pixels, which are CSS px x scale:
        // send it again at the new scale on ViewReloaded. A hidden view
        // reloads at once, hidden, unless it hibernates (SetViewHibernate) or
        // loads on show (SetViewLoadOnShow): then it reloads when next shown.
        // A view that has no page yet takes the scale at its first load (no
        // ViewReloaded); the scale its View already has is a no-op and
        // withdraws a pending rebuild. A page hosting a Web Inspector is not
        // rebuilt and keeps its scale (logged). Any thread.
        // QueryCapability("rebuildscale").
        Result (*RebuildViewAtScale)(ViewId view, float scale);
        // ── appended in 0.31.9 — gate on hostVersionNumber >= 3109 and QueryCapability("keyboardtheme") ──
        // Magelight's own VR keyboard in your colours (see KeyboardTheme)
        // whenever it is raised while this view has the keyboard (it holds UI
        // mode, or from 1.9.0 the keyboard was shared with it); another view
        // shows its own theme or the host's look. Kept until
        // changed or the view is destroyed (hibernation and RebuildViewAtScale
        // keep it); nullptr gives the host's look back. A page can set the same
        // slot with magelight.hostTheme(). The keyboard shows only on VR
        // (Magelight.json vr.keyboard); elsewhere the theme is kept, unused.
        // Any thread; *theme is copied during the call. InvalidView for a view
        // that is not a v4 one; InvalidArgument for a size below the 0.31.9
        // struct, a missing required colour or text under 3:1
        // (GetLastErrorMessage says which). Nothing is stored on a failure.
        Result (*SetViewKeyboardTheme)(ViewId view, const KeyboardTheme* theme);
        // ── appended in 0.31.11 — gate on hostVersionNumber >= 3111 and QueryCapability("prepaint") ──
        // Paint this view while it is hidden, about 1.5 s after its page
        // reaches DOM ready (again after a reload or resize), so its first show
        // composites a finished texture instead of painting the whole page in
        // that frame. From 1.5.0 it is painted once more when its page settles
        // (network and layout quiet): Ultralight 2.0 decodes a page's images
        // only once the page is painted, so the first paint has none. One view
        // per frame, never on a frame already over the load budget. The
        // texture stays allocated while the view lives, unless it hibernates or
        // the game runs low on memory (1.8.0) (for a 2560x1440 view
        // about 74 MB with MSAA 4x before 1.5.0, about 15 MB plus the page's
        // composited layer tiles from 1.5.0): opt in for a view the player
        // opens often. A page that hides its content while hidden (display:
        // none) gets nothing from it. Each paint is still one long frame,
        // during play; never in VR (accepted there, does nothing). A page can
        // set the same flag with magelight.send('__prepaint', '1'). Any thread;
        // InvalidView for a view that is gone or an inspector.
        Result (*SetViewPrepaint)(ViewId view, bool on);

        // ── appended in 1.1.0 — gate on hostVersionNumber >= 10100 and QueryCapability("uiModeCancel") ──
        // Withdraw your RequestUIMode whose entry the host has not run yet
        // (RequestUIMode only queues it), and your kUIModeFlagQueue requests:
        // Ok = withdrawn, and no UIModeEntered or UIModeRefused follows for
        // it, so a close right after an open needs no give-back. Denied = no
        // entry of yours can be withdrawn: it already ran and its
        // UIModeEntered or UIModeRefused is on its way (leave UI mode with
        // ReleaseUIMode once it is entered), or there was none. Call it BEFORE
        // ReleaseUIMode: a release while the entry waits forgets it (as before
        // 1.1.0, the entry still runs) and this then answers Denied. Any thread.
        Result (*CancelUIModeRequest)(ModId mod);

        // ── appended in 1.6.0 — gate on hostVersionNumber >= 10600 and QueryCapability("gamepad") ──
        // Opt a view in to the gamepad layer (see GamepadConfig), or out with
        // nullptr or enabled false. Kept across reloads and hibernation, ended by
        // DestroyView. The view holds the pad only while it holds UI mode, the
        // page core of its current document has started, the game window has
        // focus and no engine menu that takes menu input is open above it. Flat only: QueryCapability("gamepad")
        // is 0 on VR (the laser) and when the player switched the layer off.
        // A mod with its own gamepad bridge steps it aside where the capability
        // is 1: opt the view in with kGamepadStyleKeys and stop the bridge for
        // that view; where it is 0, keep the bridge. Any thread; *config is
        // copied. InvalidView for a view that is gone or an inspector;
        // InvalidArgument for a short size, an unknown style or bindings that
        // reuse a control or name a stick or trigger (GetLastErrorMessage says
        // which). Nothing changes on a failure.
        Result (*SetViewGamepad)(ViewId view, const GamepadConfig* config);
        // The view's opt-in and the pad as the host last saw it (*out filled;
        // out->size set by the caller). Any thread. InvalidArgument for a null
        // out or a size below the 1.6.0 struct; InvalidView for a view that is
        // gone or an inspector.
        Result (*GetViewGamepadState)(ViewId view, GamepadState* out);

        // ── appended in 1.7.1 — gate on hostVersionNumber >= 10701 and QueryCapability("mainMenu") ──
        // Asking for UI mode at the main menu is enough to open a view there (flat, with the page thread): the view
        // becomes a main-menu page and Ultralight starts if it has not. So never ask there unless your page works
        // without a world: no player, no cell. A request in the first seconds of the main menu, before the game's main
        // thread presents it, is refused as before. This call makes a view one ahead of that: its page loads at the
        // main menu, a shown view draws there, and a manifest or v4 hotkey opens it there (manifest "mainMenu": true
        // does the same). A view stays one for the session; false undoes it. Any other view waits for the first
        // in-game frame and is not drawn over the main menu. On VR or with Magelight.json "pageThread": false the call
        // succeeds and does nothing (QueryCapability("mainMenu") is 0 there). UI mode closes when a load starts, and if
        // the page thread cannot start; a page shown without UI mode is yours to hide when the main menu closes. Any
        // thread. InvalidView for a view that is gone or an inspector.
        Result (*SetViewMainMenu)(ViewId view, bool on);

        // ── appended in 1.8.0 — gate on hostVersionNumber >= 10800 and QueryCapability("polledKeys") ──
        // Which other plugins' key reads get through while this view holds UI
        // mode (see PolledKeysConfig); nullptr puts back the default, every
        // keyboard key up. Kept across reloads and hibernation, ended by
        // DestroyView; a change to the view holding UI mode applies at once.
        // kUIModeFlagPassPolledKeys on a request lets every read through
        // whatever this says. Any thread; *config and its strings are copied.
        // InvalidView for a view that is gone or an inspector; InvalidArgument
        // for a short size, a nonzero reserved, too many entries, a name that
        // is empty, a path or not a .dll, or a key outside 1-254
        // (GetLastErrorMessage says which). Nothing changes on a failure.
        Result (*SetViewPolledKeys)(ViewId view, const PolledKeysConfig* config);

        // ── appended in 1.9.0 — gate on hostVersionNumber >= 10900 and QueryCapability("focusShare") ──
        // The keyboard shared between views while one holds UI mode, both opting in. kFocusShareGive on the view
        // that holds UI mode lets a left click (or a VR trigger) on a view that set kFocusShareTake, any mod's, move
        // the keyboard there: keys, typed text, the IME, the keyboard's Escape and the page's focus go to that view,
        // while UI mode itself (the cursor, the pause, the gamepad and its keys, which plugins may read keys, who owns
        // it) stays the holder's. A click on the holder's view, or on another view that takes, moves the keyboard
        // again. It goes back to the holder's view when the holder stops giving, or the view that has it stops
        // taking, is hidden, destroyed or rebuilt (RebuildViewAtScale), and the share ends when UI mode starts, moves
        // or ends. Escape still closes UI mode unless the page that has the keyboard captured it (SetEscapeCapture).
        // While the keyboard is shared no UI-mode hotkey fires, the holder's close included (a VR binding still
        // does); a ToggleVisible binding fires either way. Each move sends KeyFocusGained and KeyFocusLost to the
        // views' mods. Kept across reloads, hibernation and RebuildViewAtScale, ended by DestroyView; manifest
        // "shareFocus" and "acceptFocus" set it too. 0 turns both off (the default). Any thread. InvalidView for a
        // view that is gone or an inspector; InvalidArgument for another bit.
        Result (*SetViewFocusSharing)(ViewId view, std::uint32_t flags);
    };

    inline constexpr std::uint32_t PackVersion(std::uint32_t major, std::uint32_t minor, std::uint32_t patch)
    {
        return major * 10000u + minor * 100u + patch;
    }

#ifndef MAGELIGHT_API_NO_LOADER
    // Resolve an API struct from an already-loaded Magelight.dll. Returns
    // nullptr when Magelight is absent or doesn't serve that version — treat
    // it as "host not installed" and no-op, never fail hard.
    inline const void* RequestApiRaw(std::uint32_t version)
    {
        const HMODULE mod = GetModuleHandleW(L"Magelight.dll");
        if (!mod) return nullptr;
        using RequestFn = void* (*)(std::uint32_t);
        const auto request =
            reinterpret_cast<RequestFn>(GetProcAddress(mod, "Magelight_RequestApi"));
        if (!request) return nullptr;
        return request(version);
    }
    inline const MagelightApi1* RequestApi(std::uint32_t version = kApiVersion1)
    {
        return static_cast<const MagelightApi1*>(RequestApiRaw(version));
    }
    inline const MagelightApi2* RequestApi2()
    {
        return static_cast<const MagelightApi2*>(RequestApiRaw(kApiVersion2));
    }
    inline const MagelightApi3* RequestApi3()
    {
        return static_cast<const MagelightApi3*>(RequestApiRaw(kApiVersion3));
    }
    inline const MagelightApi4* RequestApi4()
    {
        return static_cast<const MagelightApi4*>(RequestApiRaw(kApiVersion4));
    }
#endif

}  // namespace MAGELIGHT_API
