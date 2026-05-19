# Black Hole Gravitational Lensing Simulator

A real-time black hole simulation built from scratch in C++20 with GLSL shaders.

![Black Hole Simulation](demo.gif)

## Features
- GLSL gravitational lensing shader (Einstein ring, photon sphere, starfield warping)
- Relativistic light ray deflection at configurable speeds
- Relativistic plasma jets shooting from the black hole poles
- Live physics stats (deflection angle, closest approach, captured/escaped rays)
- Preset scenarios: Default, Einstein Ring, Photon Orbit, Light Speed
- Right-click anywhere to reposition the black hole
- Adjustable ray count (1-20) and speed (Slow to Speed of Light)

---

## The Physics Explained

### Event Horizon
The point of no return. Any light or matter that crosses it is trapped forever.
The black circle in the simulation is this boundary, defined by the Schwarzschild
radius — named after Karl Schwarzschild who solved Einstein's field equations in 1916.

### Gravitational Lensing
Massive objects warp spacetime itself. Light follows the curve of spacetime,
so it bends around massive objects. The GLSL shader simulates this — stars behind
the black hole appear warped and distorted because their light bends around it
on the way to the observer.

### Einstein Ring
When a light source, black hole, and observer are perfectly aligned, lensing
bends light from all directions equally — creating a perfect glowing ring around
the black hole. Einstein predicted this in 1936. Use the "E. Ring" preset to see it.

### Photon Sphere
At exactly 1.5x the Schwarzschild radius, light can orbit the black hole in a
circle — like a planet orbiting the sun, but for photons. The "Photon Orbit"
preset shoots a single ray at just the right angle to achieve this. In reality
it is unstable — any tiny perturbation and the photon either escapes or falls in.

### Relativistic Jets
When black holes consume matter, magnetic fields accelerate some of it outward
from the poles at near light speed. The cyan and magenta particles in the
simulation represent this. Real jets can stretch for thousands of light years.

### Schwarzschild Metric
The mathematical foundation of the simulation. Einstein's field equations describe
how mass curves spacetime. Schwarzschild found the exact solution for a non-rotating
black hole. The ray bending formula used here (grav = mass / dist^2) is a Newtonian
approximation of this — accurate enough for real-time visualization.

---

## Controls
| Input | Action |
|-------|--------|
| Speed slider | Adjust ray speed (Slow to Speed of Light) |
| Rays slider | Set number of light rays (1-20) |
| Right-click | Move black hole anywhere on screen |
| Preset buttons | Load named physics scenario |
| ESC | Quit |

## Presets
| Preset | Description |
|--------|-------------|
| Default | Balanced view of lensing and ray deflection |
| E. Ring | Maximum lensing — Einstein ring clearly visible |
| Photon Orbit | Single ray orbiting the photon sphere |
| Light Speed | All rays at maximum speed |

---

## Build Instructions
Requires CMake 3.16+, C++20 compiler, Git (SFML 3.0 downloads automatically)

```bash
cmake -B build -G "Ninja"
cmake --build build -j4
./BlackHoleSim.exe
```

## Tech Stack
- C++20 — core simulation and physics
- SFML 3.0 — windowing, rendering, input
- GLSL — fragment shader for gravitational lensing
- CMake + FetchContent — build system

---

## Developed by Sankalp S. Kulkarni
University of Florida — Computer Engineering
