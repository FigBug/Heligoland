# Heligoland

A 4-player naval combat game built with SDL3 and C++.

![Build macOS](https://github.com/FigBug/Heligoland/actions/workflows/build_macos.yml/badge.svg)
![Build Windows](https://github.com/FigBug/Heligoland/actions/workflows/build_windows.yml/badge.svg)
![Build Linux](https://github.com/FigBug/Heligoland/actions/workflows/build_linux.yml/badge.svg)

## About

Heligoland is a local multiplayer battleship game where up to 4 players command warships in naval combat. Players without controllers are controlled by AI.

## Features

- 4-player local multiplayer with gamepad support
- AI opponents for empty player slots
- Realistic ship physics with throttle and rudder controls
- 4 turrets per ship with independent aiming and firing arcs
- Wind system affecting smoke and shell trajectories
- Damage system with visual smoke effects
- Explosion and water splash effects
- Bubble wake trails behind moving ships

## Controls

Each player uses a gamepad:

- **Left Stick Y** - Throttle (forward/reverse)
- **Left/Right Stick X or Triggers** - Rudder (turn left/right)
- **Right Stick** - Aim crosshair
- **Any Button** - Fire

Ships without a connected gamepad are controlled by AI.

## Gameplay

- Each ship has 1000 HP
- Shells deal 100 damage per hit
- Ship collisions deal damage based on impact speed
- Damaged ships lose up to 20% speed and turning ability
- Damaged ships emit smoke (more damage = more smoke)
- Last ship standing wins

## Building

### Requirements

- CMake 3.16 or higher
- C++17 compatible compiler
- SDL3 (included as submodule)

### Clone

```bash
git clone --recursive https://github.com/FigBug/Heligoland.git
cd Heligoland
```

### Build on macOS

```bash
cmake -B build
cmake --build build
open build/Heligoland.app
```

### Build on Windows

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\Release\Heligoland.exe
```

### Build on Linux

```bash
sudo apt-get install libgl1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/Heligoland
```

## Dependencies

- SDL3 (included as submodule in `modules/SDL`)

## License

MIT License
