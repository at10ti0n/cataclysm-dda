# iOS/iPadOS touch UI architecture

This branch treats touch as a semantic input source, not as an on-screen keyboard.

## Input pipeline

`SDL/UIKit touch -> gesture_router -> semantic_input queue -> input_context/game action`

The gesture router starts touches in `pending`. It deliberately emits no gameplay action until ambiguity is resolved. A second finger can therefore promote a pending one-finger gesture to pan/pinch without leaving a ghost click behind.

## Gesture vocabulary

- one-finger long press: contextual radial
- one-finger drag from movement region: discrete 8-way movement
- two-finger drag: viewport pan
- pinch: zoom
- release inside radial dead-zone: cancel
- lifecycle interruption/backgrounding: cancel all fingers, held controls and queued semantic input

Radial slots are semantic families (`Interact`, `Items`, `Combat`, `Reload`, `Movement`, `Wait`, `Craft`, `More`) and resolve against the active CDDA input context. Icons are stable presentation identifiers and should remain in stable spatial positions even when the underlying action changes.

## Adaptive layout

Touch control positions are stored as normalized 0..1 coordinates. Phone defaults favor transient thumb controls. Tablet defaults are larger and may keep contextual HUD information visible. Safe-area/platform adapters should convert normalized positions to drawable coordinates after accounting for insets.

## Input-device handoff

The last meaningful input device owns the HUD. Touch shows touch chrome; physical controller or keyboard/mouse hides it. Touching the screen restores it. This is presentation-only: all devices ultimately produce the same semantic/game actions.

## Haptics

The platform-neutral layer exposes sector-change and action-commit hooks. Apple builds should map them to light selection feedback and a slightly stronger commit feedback. Non-Apple builds may no-op.

## Diagnostics

The touch layer retains a bounded diagnostic ring containing gesture transitions and cancellations. The iOS shell should expose a Share Diagnostics command and prepend build commit, iOS version, device model, drawable size, render scale and current input context. Do not include save contents or absolute user/container paths.

## Lifecycle contract

Before suspending, changing drawable ownership, or handling an interruption:

1. cancel active gestures;
2. release virtual movement;
3. close transient radial UI;
4. clear queued semantic inputs;
5. suspend rendering;
6. on resume, revalidate drawable/renderer before accepting touch.

## Device acceptance gate

A build is not considered mobile-verified merely because it compiles or runs in Simulator.

### iPhone
- [ ] install and launch on physical device
- [ ] new/load game
- [ ] 8-way movement and diagonals
- [ ] long-press radial; center cancellation; sector haptics
- [ ] inventory/context changes preserve semantic slot positions
- [ ] two-finger pan does not cause ghost action
- [ ] pinch zoom
- [ ] keyboard/text entry
- [ ] physical controller hides touch HUD and touch restores it
- [ ] background/resume during radial and movement
- [ ] rotate / safe-area handling
- [ ] save/load after resume
- [ ] Share Diagnostics redacts paths

### iPad
- [ ] repeat iPhone suite
- [ ] split/full-screen size changes
- [ ] tablet layout and larger controls
- [ ] external keyboard + controller handoff
- [ ] Stage Manager / drawable resize if supported

Record tested device, OS version, commit SHA and date for every physical-device pass.
