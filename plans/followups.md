# Cross-phase follow-ups

Small items left over from phases that otherwise landed. No new
phase home; catalogued here so they don't fall off.

## Phase E — Sequence management polish

- **Sequence Settings → Data Layers tab.** Image-data layers
  authoring. Lowest priority — deferred until someone actually
  uses them on iPad.

## Phase C — Effect Settings Inspector polish

- **G3+ — Moving Head full waypoint path authoring.** Path tab
  currently shows the existing `Path:` value with a Clear action.
  Sketch-style drag waypoint authoring still requires desktop's
  Effect Assist panel.

- **G2-c — Shader dynamic uniform grouping for large `.fs`
  files.** Most shaders declare < 10 uniforms so grouping isn't
  needed; packs with 20+ turn into a flat scroll. Respect
  `GLSL_GROUP:` comment conventions in
  `ShaderConfig::GetDynamicPropertiesJson()` so grouping carries
  across. Deferred until a real shader pack trips the issue. P2.

## Phase H — App Store

- **sACN multicast entitlement.** `com.apple.developer.networking.multicast`
  request submitted to Apple 2026-04-28. Once approved, add the key
  to `macOS/Assets/xLights-iPad/xLights-iPad.entitlements` so iPad
  testers can join `239.255.x.x` for sACN multicast output. Until
  then the toggle's "couldn't reach" alert text steers users to
  ArtNet, DDP, or sACN unicast.

## TestFlight quality (pre-submission)

These are not engineering blockers but are the "any TestFlight
report will need this" items the gap analysis (2026-04-23) flagged.
Each is a near-trivial addition that materially improves the
tester loop. Pull into MVP if there's bandwidth before H-5 wraps.

- **Recent Show Folders list** (L-1b). Recent Sequences is
  scoped per show folder (2026-04-30 refactor — flipping shows
  swaps the picker's recent list cleanly), but the folder-config
  sheet itself only remembers the *current* show folder. Add a
  "Recent Show Folders" section so users can flip between shows
  without re-picking from Files every time.
