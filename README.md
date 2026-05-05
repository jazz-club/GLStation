# GLStation

![GLStation](https://i.imgur.com/jNFGttm.png)

GLStation is a power grid simulator and builder. It has utilities for procedural generation of power grids (currently based on population using generalised averages for consumption).

It features generation and consumption profiles based on real world power sources such as Thermal, Solar, Wind, etc and consumption based on Residential and Commercial Zones. It has built-in auto balancing using a custom implementation of PowerSolver.

The runtime binary has a builder mode that can be accessed using the `build` command. The `help` command can be used in both modes to view a list of currently applicable commands.

GLStation has built-in state serialisation using a save/load system that exports using a normalised CSV format.

## Build Instructions

GLStation (currently) has no external dependencies and requires C++ and a modern compiler.

You can build the project using GNU make using the `make` commands

### Prerequisites

- GCC (C++ 20)
- GNU Make

### Recommended for Windows

- MinGW/MSYS2

### Build

```bash
git clone https://github.com/jazz-club/GLStation.git
cd GLStation/
make
```

Build artifacts can be cleared using the `make clean` command. The binary compiles to the `/build` folder as `glstation.exe` on Windows and as `glstation` on Linux.

The binary can be automatically run using the `make run` command.

## Binary

Downloads of the binary will be added to the releases tab of this repository eventually.

You can learn how to navigate the shells and view available commands using the `help` command.

## State Serialisation & Export

GLStation creates a grid.csv file in the same directory as the binary that has logs for all simulation events and telemetry and includes a saved state of the grid in the same file.

You export/load a compatible data schema file using the `save` and `import` commands.
