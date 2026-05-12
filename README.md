# GLStation

![GLStation](https://i.imgur.com/ihoSDv7.png)

GLStation is a power grid simulator and builder. It also has utilities for procedural generation of power grids (currently based on population using generalised averages for consumption).

It features generation and consumption profiles based on real world power sources such as Thermal, Solar, Wind, etc and consumption based on Residential and Commercial Zones. GLStation uses iterative non-linear calculations for stable voltage estimation using an implementation of the [Newton-Raphson method](https://www.ijert.org/research/load-flow-solution-u-sing-simplified-newton-raphson-method-IJERTV2IS121281.pdf), within [PowerSolver](https://github.com/jazz-club/GLStation/blob/main/src/sim/PowerSolver.cpp).

The runtime binary has a builder mode that can be accessed using the `build` command. The `help` command can be used in both modes to view a list of currently applicable commands.

GLStation has built-in state serialisation using a save/load system that exports using a normalised CSV format.

![Status](https://i.imgur.com/iJrGImW.png)

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

Downloads of the binary are available in the [Releases](https://github.com/jazz-club/GLStation/releases) tab.

You can learn how to navigate the shells and view available commands using the `help` command.

## Experimental Procedural Generation

Build mode currently features an experimental grid generator for testing purposes, using the `generate` command this can cause misconfigurations and invalid topologies, causing Ybus calculations to fail crashing the program due to the invalid matrix. This is partly expected, use the `validate` command and sanity check procedural grids using the `tree` command. This still does not guarantee that a randomly generated grid will be valid, improvements to this feature will come eventually.

## Grid Validation

As mentioned, the `validate` command can be used to test the integrity of the generated or manually created grids. In addition to this, please make sure to manually delete the `grid.csv` and `log.csv` files while troubleshooting, an automated check for this does exist within GLStation but can be manually circumvented leading to misconfigured grids and simulations due to human error.

## State Serialisation & Export

GLStation creates two files in the binary directory, `grid.csv` and `log.csv`, these files contain the current grid schema and logs of all simulation events, additional logging can be easily configured using the `Logger` utilities.

You export/load a compatible data schema file using the `save` and `import` commands.

## UI

The namesake of the project, a OpenGL frontend, is currently entirely unimplemented, this is due to aforementioned resource and time constraints, GLStation currently works as a standalone terminal app, the addition of the full graphical user interface is a planned future extension of the project.

## 3-Phase Calculations

GLStation currently employs a balanced single phase per unit calculation. This approach assumes the phases begin perfectly symmetrical meaning phase shift is a consequence of parameter mismatches. The 3nx3n Jacobian matrix in PowerSolver, using the NR method, would need to scale 3 times to accurately calculate every single phase, while this is not impossible to implement, this requires much more testing and calculating for power flow and stability. Consequently, our current implementation represents phase shift caused by the physical reactance of transmission lines, transformers, nodes etc.

Entirely realistic 3P solving is a planned future improvement, it was simply out of scope given the resources and time available to us.

You can view the current [PowerSolver](https://github.com/jazz-club/GLStation/blob/main/src/sim/PowerSolver.cpp) implementation for context.
