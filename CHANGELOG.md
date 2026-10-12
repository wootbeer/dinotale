# Changelog

## Unreleased

- Fixed blocks of wrong background tiles in 4-1 (and anywhere the game
  draws background tiles with the RDP), in every aspect ratio.
- Expand: the zoomed-out treehouse in 4-1 fills the whole wider screen.

## 0.1.1

- The game can be picked inside a `.zip` or `.7z` archive.
- Expand: Yoshi can walk all the way to the edge of the screen where the
  camera stops (an invisible wall used to stop him short, e.g. in 3-2).
- Hybrid and Expand are marked experimental in the settings.
- The Resolution setting's description says what each option renders at.
- Screen filters (Settings > Graphics > Screen Filter): CRT (flat, no
  curve), Scanlines, LCD, and the xBR, xBRZ and HQ2x pixel-art upscalers.
- Hybrid Dimming slider (Settings > Graphics): how dark Hybrid's stretched
  sides are, from 0% (not dimmed) up; the default is the 0.1.0 look.
- crash_log.txt names the module and thread a crash happened in.

## 0.1.0

First release.

- Yoshi's Story (USA) runs natively on PC, recompiled with N64Recomp
  and rendered with RT64. Bring your own copy of the game.
- Aspect ratios: Original, Stretch, Hybrid and Expand (true widescreen up
  to 16:9, showing more of each level).
- HUD Placement setting for Expand.
- Saves, keyboard and controller input, audio.
- Windows and Linux builds. The Linux build is new; Steam Deck reports welcome.

Still being tested: later pages and boss fights, especially in Expand.
