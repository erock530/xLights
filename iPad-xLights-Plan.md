# xLights iPad — Implementation Plan

This file tracks overall status. Details for each phase live under
[`plans/`](plans/README.md) — one focused sub-plan per phase, listing
only the work still to do. Completed phases keep a short residual
file documenting deferrals + caveats; landed implementation prose
lives in git history.

---

## Where we are (2026-04-30)

The iPad app builds, ships through Xcode Cloud, runs on TestFlight
external testers, and exercises the full desktop rendering / effect /
sequence pipeline through the same `src-core/` it shares with the
Mac. Phases A, B-Metal, C, D, E, F, G are complete. Phase B P0+P1
parity work is done (only 2 named P2s + 3 deferred remain). Phase H
is one organizational push (H-5) from submission. Phase I shipped
the headline `.xsq`/`.xsqz` import flow on 2026-04-29; format
expansions (`.sup`, `.lms`/`.las`) are the remaining feature gap.

### Code layout

iPad code lives at `src-iPad/` at the repo root (peer to `src-ui-wx/`).
It is not under `macOS/`: code in `macOS/` cannot depend on anything
outside `macOS/`, so the iPad UI sits alongside the other UI layer
instead.

```
src-iPad/
  App/            SwiftUI views + view model
  Bridge/         ObjC++ bridges (XLSequenceDocument, iPadRenderContext,
                  XLiPadInit, CoreGraphicsTextDrawingContext,
                  XLValueCurve)
  Metal/          xlStandaloneMetalCanvas, iPadModelPreview,
                  XLMetalBridge, iPadGridPreview, XLGridMetalBridge
  Metadata/       EffectMetadata.swift (JSON model for effectmetadata/*.json)
```

Shared core that iPad consumes:

- `src-core/` — wx-free C++ core (renderers, models, outputs, render
  engine, effect manager, sequence file/elements, audio manager,
  import_export).
- `macOS/src-apple-core/` — Apple shared code (Metal device manager,
  external hooks, Apple utilities in Swift and ObjC++).

### Xcode targets

| Target | Purpose | Multi-platform |
|---|---|---|
| `xLights-core` | wx-free core (`src-core/`) | macOS + iOS |
| `xLights-Apple-core` | Apple shared code (`macOS/src-apple-core/`) | macOS + iOS |
| `xLights-macOSLib-UI` | macOS-only UI (`macOS/src-mac-ui/`) | macOS only |
| `xLights` | Desktop app | macOS only |
| `EffectComputeFunctions` | Metal shaders | macOS + iOS |
| `UIMetalShaders` | UI Metal shaders | macOS + iOS |
| `ISPCEffectComputeFunctions` | SIMD kernels | macOS + iOS |
| `xLights-iPadLib` | iPad bridge code (`src-iPad/`) | iOS |
| `xLights-iPad` | SwiftUI iPad app | iOS 26+ |

iOS dependencies live at `/opt/xLights-macOS-dependencies/lib-ios/`:
`libcurl.a`, `libEGL.xcframework`, `libGLESv2.xcframework` (ANGLE),
`libliquidfun.a`, `liblua.a`, `libxlsxwriter.a`, `libzstd.a`. Debug
variants in `libdbg-ios/`.

---

## Phase status

| Phase | Title | Status | Sub-plan |
|---|---|---|---|
| A | Core-path hardening | ✓ complete | — |
| B | Effects grid parity | ✓ P0 + P1 closed; 2 P2 + 3 deferred remain | [`phase-b-grid-parity.md`](plans/phase-b-grid-parity.md) |
| B-Metal | Grid render pipeline (CG → Metal) | ✓ complete | — |
| C | Effect settings inspector | ✓ complete | 3 small follow-ups in [`followups.md`](plans/followups.md) |
| D | Model Preview + preview polish | ✓ complete | [`phase-d-preview.md`](plans/phase-d-preview.md) (residual) |
| E | Sequence management | ✓ complete | 1 follow-up (Data Layers tab) in [`followups.md`](plans/followups.md) |
| F | Window system + Display Elements | ✓ complete 2026-04-21 | [`phase-f-window-system.md`](plans/phase-f-window-system.md) (residual) |
| G | Document / iCloud polish | ✓ complete 2026-04-22 | [`phase-g-document.md`](plans/phase-g-document.md) (residual) |
| H | App Store readiness | H-0..H-4 ✓; **H-5 metadata + screenshots remaining** | [`phase-h-app-store.md`](plans/phase-h-app-store.md) |
| I | Import Effects | I-1 + I-2 (`.xsq`/`.xsqz`) ✓ 2026-04-29; **I-3 vendor regression, I-4 `.sup`, I-5 `.lms`/`.las` remaining** | [`phase-i-import-effects.md`](plans/phase-i-import-effects.md) |

---

## What's left for MVP

In priority order:

1. **Phase H-5 — App Store submission metadata.** Organizational, not
   engineering. Screenshots (12.9"/13" + 11", landscape + portrait,
   3-5 frames per orientation), description, keywords, support URL,
   privacy policy URL, age rating, category, copyright, "What's
   New". Pin a TestFlight build, submit. Detailed checklist in
   [`phase-h-app-store.md`](plans/phase-h-app-store.md).

2. **Phase I-3 — Manual vendor-sequence regression of `.xsq`/`.xsqz`
   import.** Load a real Holiday Coro / Wally Wally World pack with
   user-supplied `.xmaphint` files, run Auto Map, count matched
   models against desktop baseline. Tune Auto Map UX in response
   (scroll-to-first-unmapped, "X of Y mapped" counter). Bug-fix
   anything the regression turns up.

3. **Phase B P2 — B77 (MIDI Import Notes) and B79 (AI Speech 2
   Lyrics).** Each needs a new bridge surface (MIDI parser /
   `SFSpeechRecognizer`). Both are P2 — desktop has them, iPad
   doesn't, but neither is on the critical authoring path.

The 3 deferred Phase B items (B16 drag-from-palette ghost, B24 Find
Possible Source Effects, B56 Convert-to-Effect) are explicitly
parked — substantial new work, not blocking submission.

The follow-ups in [`followups.md`](plans/followups.md) (Data Layers
tab, MH waypoint authoring, shader uniform grouping, video compat
badge, sACN multicast entitlement) are quality-of-life and unblock
independently.

## Could pull into MVP during testing

If H-5 prep + I-3 testing leaves spare cycles, these are the high-
ROI items that would **measurably improve** what testers experience.
Detailed in [`plans/followups.md`](plans/followups.md) "TestFlight
quality" section unless noted otherwise.

**P0 / P1 — TestFlight loop quality** (gap-analysis flagged):

- ✓ **Log export** — Tools → Package Logs zips `xLights.log` +
  rotated siblings, MetricKit JSON, show-folder XML, the open
  sequence, threads + device-info sidecars, and hands the result
  to `UIActivityViewController`. Logs moved from `Documents/` to
  `Library/Logs/` so they no longer eat iCloud quota; one-time
  migration in `XLiPadInit` brings legacy logs across.
- ✓ **Crash telemetry** — `XLMetricKit` (shared with desktop, in
  `macOS/src-apple-core/osxUtils/`) subscribes at launch and writes
  metric / diagnostic payloads to `Library/Logs/Diagnostics/`. iPad
  ships them via Package Logs; desktop folds them into the next
  `wxDebugReportCompress` zip under `MetricKit/`. Hang / CPU /
  disk-write diagnostics are the headline win — they don't trigger
  the wx signal-handler-based crash path on Mac and have no
  equivalent on iPad today.
- ✓ **Auto-upload of crash zips** — `XLDiagnosticUploader.swift`
  posts staged zips from `Library/Logs/PendingUpload/` to the same
  `crashUpload/index.php` endpoint the desktop app uses, so iPad
  reports land in the existing triage bin. Triggered three ways:
  (a) MetricKit notification → stage + upload, (b) prior-session
  sentinel left behind by a crash → stage + upload at next launch,
  (c) every `scenePhase = .active`. Auto-upload uses a smaller
  payload than user-initiated Package Logs (no show folder XML, no
  open sequence) to keep size + PII small. Opt-in toggle in iOS
  Settings → xLights → "Send Crash Reports" (default on).
  `PrivacyInfo.xcprivacy` updated with `CrashData /
  PerformanceData / OtherDiagnosticData` entries.
- ✓ **About + Help menu** — `Help` group in the menu bar with
  About xLights… (sheet showing icon, version, build, GPL legal
  text from the shared `XLIGHTS_LICENSE`, Privacy Policy + EULA
  links) plus seven external link entries (Manual, Tutorial
  Videos, Release Notes, Forum, Facebook, Issue Tracker,
  xLights.org), each routed through `XLOpenURL` to the system
  browser to match desktop's `wxLaunchDefaultBrowser` behaviour.
- ✓ **Check Sequence runner** — Tools → Check Sequence runs the
  full desktop check suite via the shared
  `src-core/diagnostics/SequenceChecker` (~1175 lines, wx-free
  port of the bulk of `xLightsFrame::CheckSequence`). Covers
  controllers (inactive, IP collisions, ZCPP/managed checks,
  duplicate universes, model controller-connection validation),
  models (start-channel chain loops, overlaps, missing matrix
  faces, single-line orientation, model-group consistency,
  submodel sanity), and the per-effect / per-element walk
  (transitions, Per-Model + sub-buffer combos, old value curves,
  canvas mode, video codec, render-disabled summaries, faces /
  states / viewpoints summaries). `CheckSequenceReport` was
  also lifted to core with a structured `ReportIssue` carrying
  optional `(modelName, effectName, startTimeMS, layerIndex)` so
  both clients can offer tap-to-jump — iPad's sheet uses it now;
  desktop's HTML report ignores it (the data is there for a
  future in-app results panel). Desktop's
  `xLightsFrame::CheckSequence` collapsed from ~2k lines to a
  ~180-line wrapper that keeps only the wx-only chunks (network
  socket probe, OS / preferences, HTML output, OpenGL core-
  profile shader guard, BadDriveAccess) and delegates the rest
  through `DesktopCheckCallbacks`.
- ✓ **Incompatible-video warning at sequence load** —
  `mediaInventoryInSequence` now runs the AVFoundation probe via
  `MediaCompatibility::CheckVideoFile` on every video entry whose
  file exists, rolling codec-incompatible videos (VP9, AV1,
  ProRes-RAW, …) into `isBroken` alongside missing files. The
  red "X media files have issues" banner across the top fires for
  both. Media Manager inventory chip says "Missing" or
  "Unsupported" depending on the failure mode.
- ✓ **Re-prompt on failed `ObtainAccessToURL`** —
  `SequencerViewModel.loadShowFolder` pre-checks every show /
  media folder path with `XLSequenceDocument.obtainAccess(toPath:)`
  before calling into C++. Stale paths queue an
  `AccessRepromptRequest`; the `AccessRepromptSheet` presents a
  `UIDocumentPickerViewController` so the user re-grants access,
  and the C++ load only runs once the queue drains. Folders that
  still can't be accessed are dropped (matches the prior silent-drop
  behaviour, but now the user actually sees what happened).

**P2 — feature additions worth a tester sprint**:

- **I-4 SuperStar `.sup` import.** Same UI as I-2; just hoist the
  parser from `SuperStarImportDialog.cpp` to
  `src-core/import_export/SuperStarImporter.{h,cpp}`. Vendor
  relevance ≈ `.xsq` for a meaningful slice of users.
- **B77 MIDI import.** Concrete user request; iOS-native via
  AVFoundation `MIDIFile`.
- **Audio onset → timing track** (A-1, P1, M). Replaces 5–10 minutes
  of manual mark-tapping with one button.
- **Live-output controller list (read-only)** (gap-analysis O-3,
  P1, M). Even just *seeing* what's configured (without setup UI)
  is a tester clarity win.
- **Recent Show Folders list** (L-1b). Recent Sequences exists;
  Recent Show Folders does not.
- **Video compat badge in Media Manager.** Per-effect probe
  already exists; one-day fix, big diagnostic clarity win.

Items **not** worth pulling in even with spare cycles:

- I-5 (`.lms`/`.las`) — distant third format, low vendor traffic
  today. Land after I-4 stabilises.
- MH full waypoint authoring — Sketch-style drag UI is a real
  design exercise, not a quick port.
- Shader uniform grouping (G2-c) — only matters for shader packs
  with 20+ uniforms, which are vanishingly rare.
- Anything from [`plans/future-layout-editing.md`](plans/future-layout-editing.md),
  [`plans/future-custom-models.md`](plans/future-custom-models.md),
  or [`plans/future-controllers-tab.md`](plans/future-controllers-tab.md) —
  all multi-month efforts.

**Catalogue of full post-MVP scope.** The 2026-04-23 gap analysis
(in the sibling working tree at
`xLights/plans/gap-analysis-2026-04-23.md`) inventoried the full
desktop surface and recommended ~12 phases beyond MVP totalling
20–30 person-months. Each major domain is now tracked in a
`plans/future-*.md` file — see [`plans/README.md`](plans/README.md).

## Preview scope

Phase D is preview *viewing and appearance*: camera, overlays,
background, labels, transport, export. Desktop `ModelPreview` also
hosts the layout editor (drag-to-move, resize handles, polyline
editing, align/distribute, property grid) — that behaviour stays
desktop-only and is parked in
[`future-layout-editing.md`](plans/future-layout-editing.md).
Per-view model visibility / layout-group management is Phase F-6
(Display Elements editor).

## Controller output

Lightbulb toggle in the `SequencerView` toolbar between play
controls and render-all (`lightbulb` / `lightbulb.fill`, yellow
tint when on). Tap-while-off routes through
`SequencerViewModel.toggleOutput()`; on failure it returns one of
two user-facing alerts: "no controllers configured" (the common
external-tester case — there's no iPad controller-setup UI, so
testers must configure controllers in desktop xLights and copy the
show folder over) or "couldn't reach configured controllers"
(network / multicast issue).

**sACN multicast caveat.** iOS gates `239.255.x.x` joins behind
`com.apple.developer.networking.multicast`, which Apple approves
manually. Entitlement request submitted 2026-04-28; tracked in
[`followups.md`](plans/followups.md). Until approved, ArtNet, DDP,
and sACN unicast all work today.

## Deferred / explicitly out of MVP

- **JobPool requeue redesign** — desktop-scope refactor to replace
  block-on-other-model-frame with re-enqueue. Tracked separately;
  the iPad workaround is "more threads."
- **Layout editor, controller setup** — stays desktop-only.
- **Disk-persistent effect presets** — in-session ship is enough
  for now; on-disk store + preset-tree UI parked in
  [`future-effect-presets.md`](plans/future-effect-presets.md).

---

## Risks

- **JobPool deadlock on complex sequences** — mitigated by raising
  the thread count; the underlying "workers block on peers"
  pattern is still there.
- **Memory on mid-tier iPads** — Phase A memory-pressure handling
  is in place but under-tested. External tester reports will be
  the first stress signal.
- **AVFoundation codec coverage** — video effects with unusual
  codecs fail; no FFmpeg fallback on iPad.
- **Auto Map false-positives on similar model names** — desktop
  users live with this; iPad testers have a smaller screen for
  review. UX mitigation (scroll-to-first-unmapped) tracked in I-3.

---

## Open questions

_(none open.)_
