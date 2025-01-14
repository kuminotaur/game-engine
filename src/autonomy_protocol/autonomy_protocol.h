#pragma once

#include <Eigen/Dense>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "balloon_status.h"
#include "game_snapshot.h"
#include "map3d.h"
#include "presubmission_trajectory_vetter.h"
#include "trajectory_client.h"
#include "trajectory_code.h"
#include "warden.h"
#include "wind_intensity.h"

namespace game_engine {
// AutonomyProtocol interfaces with MediationLayer the and enables an actor
// to read limited game state information and dictate intended future
// actions for specific quadcopters.
//
// This class is an interface --- it should not be constructed as a standalone
// object. Instead, a sub-class should implement this class. Sub-classes must
// implement the UpdateTrajectories function. The UpdateTrajectories function
// should map friendly quadcopters current positions to intended trajectories.
//
// UpdateTrajectories is called every 200ms (5 Hz). A protocol may either
// specify a new trajectory, overwriting the last trajectory, or return an
// empty trajectory, implicitly instructing the quadcopter to continue following
// the previous trajectory or to hold its final position.
//
// Warning: sometimes ROS drops the first couple of messages in a
// stream. It's busy connecting publishers and subscribers together and does
// not properly pass on messages. As a result, if a trajectory is only
// published the first time there is no guarantee that the quad will get to
// the destination. The trajectory may need to be re-published.
//
// The AutonomyProtocol should be run as its own thread.
class AutonomyProtocol {
 protected:
  std::vector<std::string> friendly_names_;
  std::vector<std::string> enemy_names_;
  std::shared_ptr<GameSnapshot> snapshot_;
  std::shared_ptr<TrajectoryWardenClient> trajectory_warden_client_;
  std::shared_ptr<PreSubmissionTrajectoryVetter> prevetter_;
  Map3D map3d_;
  std::shared_ptr<BalloonStatus> red_balloon_status_;
  std::shared_ptr<Eigen::Vector3d> red_balloon_position_;
  std::shared_ptr<BalloonStatus> blue_balloon_status_;
  std::shared_ptr<Eigen::Vector3d> blue_balloon_position_;
  Eigen::Vector3d goal_position_;
  WindIntensity wind_intensity_;

  volatile std::atomic<bool> ok_{true};
  std::map<std::string, TrajectoryCode> trajectoryCodeMap_;

 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  AutonomyProtocol(
      const std::vector<std::string>& friendly_names,
      const std::vector<std::string>& enemy_names,
      const std::shared_ptr<GameSnapshot> snapshot,
      const std::shared_ptr<TrajectoryWardenClient> trajectory_warden_client,
      const std::shared_ptr<PreSubmissionTrajectoryVetter> prevetter,
      const Map3D& map3d,
      const std::shared_ptr<BalloonStatus> red_balloon_status,
      const std::shared_ptr<Eigen::Vector3d> red_balloon_position,
      const std::shared_ptr<BalloonStatus> blue_balloon_status,
      const std::shared_ptr<Eigen::Vector3d> blue_balloon_position,
      const Eigen::Vector3d& goal_position, const WindIntensity& wind_intensity)
      : friendly_names_(friendly_names),
        enemy_names_(enemy_names),
        snapshot_(snapshot),
        trajectory_warden_client_(trajectory_warden_client),
        prevetter_(prevetter),
        map3d_(map3d),
        red_balloon_status_(red_balloon_status),
        red_balloon_position_(red_balloon_position),
        blue_balloon_status_(blue_balloon_status),
        blue_balloon_position_(blue_balloon_position),
        goal_position_(goal_position),
        wind_intensity_(wind_intensity) {}

  virtual ~AutonomyProtocol() {}

  // Stop this thread from running
  void Stop();

  // Main loop for this thread
  void Run(
      std::unordered_map<std::string, std::shared_ptr<TrajectoryClientNode>>
          proposed_trajectory_clients,
      bool joy_mode = false, bool camera_mode = false);

  // Virtual function to be implemented as by an actor. Input is a snapshot
  // of the system, output is an intended trajectory for each of the
  // friendly quads.
  virtual std::unordered_map<std::string, Trajectory> UpdateTrajectories() = 0;
};

//  ******************
//  * IMPLEMENTATION *
//  ******************
inline void AutonomyProtocol::Run(
    std::unordered_map<std::string, std::shared_ptr<TrajectoryClientNode>>
        proposed_trajectory_clients,
    bool joy_mode, bool camera_mode) {
  while (ok_) {
    // Request trajectory updates from the virtual function
    const std::unordered_map<std::string, Trajectory> trajectories =
        UpdateTrajectories();

    // For every friendly quad, push the intended trajectory to the trajectory
    // warden
    for (const std::string& quad_name : friendly_names_) {
      try {
        const Trajectory trajectory = trajectories.at(quad_name);
        TrajectoryCode trajectoryCode = trajectory_warden_client_->Write(
            quad_name, trajectory, proposed_trajectory_clients);
        trajectoryCodeMap_[quad_name] = trajectoryCode;
      } catch (const std::out_of_range& e) {
        continue;
      }
    }
    // Sleep for 200 ms is the default. This essentially creates a loop that
    // runs at 5 Hz.
    int sleep_time = 200;
    if (camera_mode) {
      // TBD: need to test with how fast camera values come in
      sleep_time = 200;
    }
    if (joy_mode) {
      // Sleep for 100 ms. This creates a loop that runs at 10 Hz for the joy
      // mode.
      sleep_time = 100;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
  }
}

inline void AutonomyProtocol::Stop() { ok_ = false; }
}  // namespace game_engine
