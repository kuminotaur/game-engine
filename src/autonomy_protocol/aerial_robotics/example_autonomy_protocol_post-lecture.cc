#include "example_autonomy_protocol.h"

#include <chrono>

#include "graph.h"
#include "occupancy_grid3d.h"
#include "student_game_engine_visualizer.h"

// The length of one side of a cube in meters in the occupancy grid
constexpr double DISCRETE_LENGTH = 0.2;
// How big the "inflation" bubble around obstacles will be, in meters
constexpr double SAFETY_BOUNDS = 0.34;

namespace game_engine {
std::chrono::milliseconds dt_chrono = std::chrono::milliseconds(40);

// UpdateTrajectories creates and returns a proposed trajectory.  The proposed
// trajectory gets submitted to the mediation_layer (ML), which responds by
// setting the data member trajectoryCodeMap_.  See the header file
// game-engine/src/util/trajectory_code.h for a list of possible codes.
//
// Any code other than MediationLayer::Success indicates that the ML has
// rejected the submitted trajectory.
//
// trajectoryCodeMap_ is initialized with MediationLayerCode::Success, so this
// will be its value the first time this function is called (before any
// trajectories have been submitted).  Thereafter, trajectoryCodeMap_ will
// indicate the MediationLayerCode for the most recently submitted trajectory.
std::unordered_map<std::string, Trajectory>
ExampleAutonomyProtocol::UpdateTrajectories() {
  // Set the duration of the example trajectory
  constexpr int duration_sec = 30;
  const std::chrono::milliseconds T_chrono = std::chrono::seconds(duration_sec);

  // 'static' variables act like Matlab persistent variables, maintaining their
  // value between function calls. Their initializer is only called once, on the
  // first pass through the code.  If you prefer not to include static
  // variables, you could instead make these data members of your
  // StudentAutonomyProtocol class, which is a more standard C++ code design
  // pattern.
  static OccupancyGrid3D occupancy_grid;
  static Graph3D graph_of_arena;
  // Student_game_engine_visualizer is a class that supports visualizing paths,
  // curves, points, and whole trajectories in the RVIZ display of the arena to
  // aid in your algorithm development.
  static Student_game_engine_visualizer visualizer;
  static bool first_time = true;
  static bool halt = false;
  static Eigen::Vector3d start_pos;
  static const std::chrono::time_point<std::chrono::system_clock>
      start_chrono_time = std::chrono::system_clock::now();
  static const std::chrono::time_point<std::chrono::system_clock>
      end_chrono_time = start_chrono_time + T_chrono;

  // Load the current quad position
  const std::string& quad_name = friendly_names_[0];
  Eigen::Vector3d current_pos;
  snapshot_->Position(quad_name, current_pos);

  // Set some static variables the first time this function is called
  if (first_time) {
    first_time = false;
    occupancy_grid.LoadFromMap(map3d_, DISCRETE_LENGTH, SAFETY_BOUNDS);
    // You can run A* on graph_of_arena once you created a 3D version of A*
    graph_of_arena = occupancy_grid.AsGraph();
    visualizer.startVisualizing("/game_engine/environment");
    start_pos = current_pos;
  }

  // Obtain current balloon positions and popped states
  const Eigen::Vector3d red_balloon_pos = *red_balloon_position_;
  const Eigen::Vector3d blue_balloon_pos = *blue_balloon_position_;
  const bool red_balloon_popped = red_balloon_status_->popped;
  const bool blue_balloon_popped = blue_balloon_status_->popped;

  // Condition actions or parameters on wind intensity
  switch (wind_intensity_) {
    case WindIntensity::Zero:
      // Do something zero-ish
      break;
    case WindIntensity::Mild:
      // Do something mild
      break;
    case WindIntensity::Stiff:
      // Do something stiff
      break;
    case WindIntensity::Intense:
      // Do something intense
      break;
    case WindIntensity::Ludicrous:
      // Do something ludicrous
      break;
    default:
      std::cerr << "Unrecognized WindIntensity value." << std::endl;
      std::exit(EXIT_FAILURE);
  }

  // You can fill out this switch statement with case statements tailored to
  // each MediationLayerCode.
  switch (trajectoryCodeMap_[quad_name].code) {
    case MediationLayerCode::Success:
      // You probably won't need to do anything in response to Success.
      break;
    case MediationLayerCode::TimeBetweenPointsExceedsMaxTime: {
      // Suppose your AP initially submits a trajectory with a time that exceeds
      // the maximum allowed time between points. You could fix the problem as
      // shown below.
      std::cout << "Replanning trajectory: "
                   "Shortening time between trajectory points."
                << std::endl;
      std::cout << "Value: " << trajectoryCodeMap_[quad_name].value
                << std::endl;
      dt_chrono = dt_chrono - std::chrono::milliseconds(15);
      break;
    }
    default:
      // If you want to see a numerical MediationLayerCode value, you can cast
      // and print the code as shown below.
      std::cout << "MediationLayerCode: "
                << static_cast<int>(trajectoryCodeMap_[quad_name].code)
                << std::endl;
      std::cout << "Value: " << trajectoryCodeMap_[quad_name].value
                << std::endl;
      std::cout << "Index: " << trajectoryCodeMap_[quad_name].index
                << std::endl;
  }

  // Always use the chrono::system_clock for time. Trajectories require time
  // points measured in floating point seconds from the Unix epoch.
  const double dt =
      std::chrono::duration_cast<std::chrono::duration<double>>(dt_chrono)
          .count();
  const std::chrono::time_point<std::chrono::system_clock> current_chrono_time =
      std::chrono::system_clock::now();
  const std::chrono::duration<double> remaining_chrono_time =
      end_chrono_time - current_chrono_time;

  // The following code generates and returns a new trajectory each time it
  // runs.  The new trajectory starts at the location on the original circle
  // that is closest to the current location of the quad and it creates a
  // trajectory of N position, velocity, and acceleration (PVA) points spaced by
  // dt seconds.  Thus, the code below responds to the actual position of the
  // quad and adjusts the newly-generated trajectory accordingly.  But note that
  // its strategy is not optimal for covering the greatest distance in the
  // allotted time in the presence of disturbance accelerations.

  // Number of samples in new trajectory
  const size_t N = remaining_chrono_time / dt_chrono;

  // Create an empty quad-to-trajectory map.  This map object associates a quad
  // name (expressed as a std::string) with the corresponding Trajectory object.
  std::unordered_map<std::string, Trajectory> quad_to_trajectory_map;

  // If the end time has passed, or if there are too few samples for a valid
  // trajectory, return an empty quad_to_trajectory_map
  constexpr size_t min_required_samples_in_trajectory = 2;
  if (current_chrono_time >= end_chrono_time ||
      N < min_required_samples_in_trajectory) {
    return quad_to_trajectory_map;
  }

  // Halt at goal position when close enough
  constexpr double goal_arrival_threshold_meters = 0.3;
  const Eigen::Vector3d dv = current_pos - goal_position_;
  // TrajectoryVector3D is an std::vector object defined in trajectory.h
  TrajectoryVector3D trajectory_vector;
  if (halt ||
      (remaining_chrono_time < std::chrono::seconds(duration_sec - 10) &&
       dv.norm() < goal_arrival_threshold_meters)) {
    halt = true;
    for (size_t idx = 0; idx < 20; ++idx) {
      const std::chrono::duration<double> flight_chrono_time =
          current_chrono_time.time_since_epoch() + idx * dt_chrono;
      const double flight_time = flight_chrono_time.count();
      trajectory_vector.push_back(
          (Eigen::Matrix<double, 11, 1>() << goal_position_.x(),
           goal_position_.y(), goal_position_.z(), 0, 0, 0, 0, 0, 0, 0,
           flight_time)
              .finished());
    }
    Trajectory trajectory(trajectory_vector);
    visualizer.drawTrajectory(trajectory);
    quad_to_trajectory_map[quad_name] = trajectory;
    return quad_to_trajectory_map;
  }

  // Create circular trajectory
  // Set the radius of the circular example trajectory in meters
  constexpr double radius = 0.75;

  // Set the angular rate of the circular example trajectory in radians/s
  constexpr double omega = 2 * M_PI / duration_sec;

  // Place the center of the circle 1 radius in the y-direction from the
  // goal position
  const Eigen::Vector3d circle_center =
      goal_position_ + Eigen::Vector3d(0, radius, 0);

  // Transform the current position into an angle
  const Eigen::Vector3d r = current_pos - circle_center;
  const double theta_start = std::atan2(r.y(), r.x());

  // Generate the remaining circular trajectory
  for (size_t idx = 0; idx < N; ++idx) {
    // chrono::duration<double> maintains high-precision floating point time in
    // seconds use the count function to cast into floating point
    const std::chrono::duration<double> flight_chrono_time =
        current_chrono_time.time_since_epoch() + idx * dt_chrono;

    // Calculate angle in radians
    const double theta = theta_start + omega * idx * dt;

    // Calculate circular path points
    const double x = circle_center.x() + radius * std::cos(theta);
    const double y = circle_center.y() + radius * std::sin(theta);
    const double z = circle_center.z();

    // Generate velocities via chain rule
    const double vx = -radius * std::sin(theta) * omega;
    const double vy = radius * std::cos(theta) * omega;
    const double vz = 0.0;

    // Generate accelerations via chain rule
    const double ax = -radius * std::cos(theta) * omega * omega;
    const double ay = -radius * std::sin(theta) * omega * omega;
    const double az = 0.0;

    const double yaw = 0.0;

    // Time must be specified as a floating point number that measures the
    // number of seconds since the Unix epoch.
    const double flight_time = flight_chrono_time.count();

    // Push an a matrix packed with a trajectory point onto the trajectory
    // vector
    trajectory_vector.push_back((Eigen::Matrix<double, 11, 1>() << x, y, z, vx,
                                 vy, vz, ax, ay, az, yaw, flight_time)
                                    .finished());
  }
  // Construct a trajectory from the trajectory vector
  Trajectory trajectory(trajectory_vector);

  // Before submitting the trajectory, you can use this prevetting interface to
  // determine if the trajectory violates any mediation layer constraints. This
  // interface can be found in presubmission_trajectory_vetter.h. The PreVet
  // function returns type TrajectoryCode which contains three values: (1) The
  // mediation layer code that specifies success or the failure; (2) The failure
  // value (e.g., if the velocity limit is 4.0 m/s and you submit 5.0 m/s, the
  // value is returned as 5.0); (3) The failure index. This is the trajectory
  // sample index that caused the mediation layer to kick back an error code.
  // The index is the sampled index (specified from the P4 sampling process).
  // You can figure out the waypoint index by a simple math transformation.
  TrajectoryCode prevetter_response =
      prevetter_->PreVet(quad_name, trajectory, map3d_);
  switch (prevetter_response.code) {
    case MediationLayerCode::Success:
      // You probably won't need to do anything in response to Success.
      break;
    case MediationLayerCode::TimeBetweenPointsExceedsMaxTime: {
      // Suppose your AP intends to submit a trajectory with a time that exceeds
      // the maximum allowed time between points. The prevetter would catch this
      // before you submit to the mediation layer.
      std::cout << "Prevet: Shorten time between trajectory points."
                << std::endl;
      std::cout << "Prevet: Time violation: " << prevetter_response.value
                << std::endl;
      break;
    }
    default:
      std::cout << "Prevet code: " << static_cast<int>(prevetter_response.code)
                << std::endl;
      std::cout << "Prevet value: " << prevetter_response.value << std::endl;
      std::cout << "Prevet index: " << prevetter_response.index << std::endl;
  }

  // Invoke the visualizer to see the proposed trajectory, which will be
  // displayed in violet. See student_game_engine_visualizer.h for other
  // visualization options: you can visualize a short path, a single point, etc.
  // It will be helpful to get such visual feedback on candidate trajectories.
  // Note that there is a built-in visualizer called "ViewManager" implemented
  // elsewhere in the game-engine code, but you don't have full control over
  // what it displays like you do with the Student_game_engine_visualizer
  // invoked below.
  visualizer.drawTrajectory(trajectory);
  quad_to_trajectory_map[quad_name] = trajectory;
  return quad_to_trajectory_map;
}
}  // namespace game_engine
