# Changelog

## 0.1.5

- Expand: water reflections (4-2), goo (2-3) and the page-turn transition
  cover the whole wide screen.
- The Graphics API setting (Auto, D3D12, Vulkan) is shown in the graphics
  options. If the game crashes in the graphics driver on Windows, try Vulkan.
- Expand: no more flashing strip at the right edge after a teleport (and
  at a level's start): the backgrounds are filled all the way across.
- Message boxes and clouds (images drawn in strips) have no lines at the
  joins, and their edges no longer wiggle as they move.
- Fixed lines through the clouds on the continue / game over screen.
- Expand: things no longer vanish at the edge of the 4:3 area: enemies like
  5-4's bob-ombs stay until they leave the wider screen, and the stilt Shy
  Guys' stilts show across it.

## 0.1.4

- Windows: larger thread stacks, for random crashes in NVIDIA's graphics
  driver (`nvwgf2umx.dll`).
- Expand in a window wider than 16:9: black bars at the sides instead of
  smeared, flickering edges.
- Fixed lines through rotated sprites (3-3's tilting platforms, 4-3's balls,
  6-1's cogwheels).

## 0.1.3

- Expand: Yoshi walks smoothly to the edge of the wider screen instead of
  jumping out from the 4:3 edge, and can no longer walk off the left end of
  a level at its start.

## 0.1.2

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
