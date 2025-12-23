# Heligoland

A naval combat game where up to 4 ships battle it out. Ships are controlled by gamepads or AI.

## Building

Requires CMake 3.16+ and a C++17 compiler.

```bash
mkdir build
cd build
cmake ..
make
./Heligoland
```

### macOS

Deployment target is set to macOS 13.0.

## Controls

**Gamepad:**
- Left stick: Move and turn ship
- Right stick: Aim turrets
- Right trigger: Fire

Ships without a connected gamepad are controlled by AI.

## Gameplay

- Each ship has 1000 HP
- Shells deal 100 damage per hit
- Ship collisions deal damage based on impact speed
- Last ship standing wins

## Features

- 4-player local multiplayer with gamepad support
- AI opponents for unassigned ships
- 4 turrets per ship with restricted firing arcs (front turrets can't aim backward, rear turrets can't aim forward)
- Bubble wake trails behind moving ships
- Ship-to-ship collision with speed-based damage

## Dependencies

- SDL3 (included as submodule in `modules/SDL`)
