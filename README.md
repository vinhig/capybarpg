# CapybaRPG

Some kind of 2D game engine that aims at providing methods and data structures necessary to develop a Rim World/Dwarf Fortress clone. It focuses on high-parallelization of the logic and the management of assets.

## Build

Make sure you have SDL2 installed, the vulkan SDK installed, a Vulkan 1.3 GPU , and a QuakeC compiler. At the time of writing, only the linux build is tested.

```
sudo pacman -S 
```

```
meson setup build_debug --buildtype=debug
cd build_debug
ninja
```

```
meson setup build_release --buildtype=release
cd build_debug
ninja
```

## Run

CapybaRPG is a only a set of tools in an executable. Your game logic, and assets have to be declared in a separate folder called `base`. An minimal example will be given in the near future.

```
cd build_debug
./maidenless --base /path/to/your/game/base
```
