# Game Engine
## Structure
The Game Engine has four interacting components: the Mediation Layer (ML),
Physics Simulator (PS), Visualizer (VZ), and the Autonomy Protocol (AP). 

AP maps the quadcopter's state to an intended trajectory. ML mediates the
intended trajectories, altering them if necessary to impose boundaries,
simulate interaction with other objects, etc.  PS forward-simulates the
mediated trajectory, injecting disturbances and applying
proportional-derivative control to track the trajectory, and returns the
quadcopter's state at a subsequent time point. VZ pushes visualization data to
ROS's RVIZ program.

## Installation
### Prerequisites 
1. [Eigen](https://eigen.tuxfamily.org)
2. [ROS](http://www.ros.org)
3. [P4](https://gitlab.com/radionavlab/public/p4)

### Clone
```bash
git clone git@gitlab.com:radionavlab/machine-games/game-engine.git
cd game-engine
git submodule update --init --recursive
```

### Build
```bash
mkdir build 
cd build
cmake ..
make -j4
```

## Running the Game Engine
The Game Engine is composed of several interacting executables. After
building, you must ensure that the following programs are running. You are
encouraged to use a terminal multiplexer like tmux and start each program in a
separate pane.

### ROS Core
```bash
roscore
```

### Load ROS params
The ROS parameters found in `game-engine/run/params.yaml` must be loaded after
roscore has been started or re-loaded if any of the parameters have changed
since loading.

First, modify the file paths in `/game-engine/run/params.yaml` (e.g.,
`map_file_path`) so that they are correct for your system.  Then load the
parameters into the `game_engine` namespace as follows:
```bash
cd game-engine/run
rosparam load params.yaml /game_engine/
```

### ROS Visualizer
The ROS visualizer (RVIZ) manages a 3D visualization environment in which the
arena, obstacles, balloons, and quadcopters are displayed.
```
cd game-engine/run
rosrun rviz rviz -d config.rviz
```

### Mediation Layer
The mediation layer mediates proposed trajectories, ensuring they won't
cause a quadcopter to crash.
```
cd game-engine/bin
./mediation_layer
```

### Physics Simulator
The physics simulator forward-integrates proposed quadcopter trajectories over
a short interval into the future and publishes the resulting state.  It
applies proportional-derivative control to track the trajectories.  It also
introduces disturbance forces, e.g., due to wind.
```
cd game-engine/bin
./physics_simulator
```

### Visualizer
The visualizer sends arena, obstacle, balloon, and quadcopter data
to RVIZ for display.
```
cd game-engine/bin
./visualizer
```

### Autonomy Protocol
The autonomy protocol takes the current quadcopter state and publishes a
proposed trajectory for the quadcopter to follow.
```
cd game-engine/bin
./example_autonomy_protocol
```

### Tests
```bash
cd build/test
./EXECUTABLE_OF_CHOICE
```

## Contributing
### Software Patterns
Please read the [software patterns](doc/software-patterns.md) document to
understand the naming convention and the purpose of components in this system.

