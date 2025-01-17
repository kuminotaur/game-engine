#include <ros/ros.h>

#include <Eigen/Dense>
#include <csignal>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "balloon_watchdog.h"
#include "goal_watchdog.h"
#include "map3d.h"
#include "mediation_layer.h"
#include "quad_state.h"
#include "quad_state_subscriber_node.h"
#include "quad_state_watchdog.h"
#include "safety_monitor.h"
#include "safety_monitor_status.h"
#include "trajectory.h"
#include "trajectory_publisher_node.h"
#include "trajectory_server.h"
#include "trajectory_watchdog.h"
#include "view_manager.h"
#include "warden.h"
#include "yaml-cpp/yaml.h"

using namespace game_engine;

namespace {
// Signal variable and handler
volatile std::sig_atomic_t kill_program;
void SigIntHandler(int sig) { kill_program = 1; }
}  // namespace

int main(int argc, char** argv) {
  // Configure sigint handler
  std::signal(SIGINT, SigIntHandler);

  // Start ROS
  ros::init(argc, argv, "mediation_layer", ros::init_options::NoSigintHandler);
  ros::NodeHandle nh("/game_engine/");

  // YAML config
  std::string map_file_path;
  if (false == nh.getParam("map_file_path", map_file_path)) {
    std::cerr << "Required parameter not found on server: map_file_path"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }

  YAML::Node node;
  try {
    node = YAML::LoadFile(map_file_path);
  } catch (...) {
    std::cerr << "Map file not found.  Check map_file_path in params.yaml"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }
  const Map3D map = node["map"].as<Map3D>();

  // Load the server topics
  std::map<std::string, std::string> proposed_trajectory_topics;
  if (false ==
      nh.getParam("proposed_trajectory_topics", proposed_trajectory_topics)) {
    std::cerr
        << "Required parameter not found on server: proposed_trajectory_topics"
        << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // Initialize clients
  // Load the client topics
  std::map<std::string, std::string> updated_trajectory_topics;
  if (false ==
      nh.getParam("updated_trajectory_topics", updated_trajectory_topics)) {
    std::cerr
        << "Required parameter not found on server: updated_trajectory_topics"
        << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // Initialize the TrajectoryWardens. The TrajectoryWarden enables safe,
  // multi-threaded access to trajectory data. Internal components that require
  // access to proposed and updated trajectories should request access through
  // TrajectoryWarden.
  auto trajectory_warden_srv = std::make_shared<TrajectoryWardenServer>();
  auto trajectory_warden_pub = std::make_shared<TrajectoryWardenPublisher>();
  for (const auto& kv : proposed_trajectory_topics) {
    const std::string& quad_name = kv.first;
    trajectory_warden_srv->Register(quad_name);
    trajectory_warden_pub->Register(quad_name);
  }

  // Initialize servers
  // For every quad, the service has corresponding proposed_trajectory topic.
  // Service gets trajectories from the AP client.
  std::unordered_map<std::string, std::shared_ptr<TrajectoryServerNode>>
      trajectory_servers;
  for (const auto& kv : proposed_trajectory_topics) {
    const std::string& quad_name = kv.first;
    const std::string& topic = kv.second;
    trajectory_servers[quad_name] = std::make_shared<TrajectoryServerNode>(
        topic, quad_name, trajectory_warden_srv);
  }

  // For every quad, publish to its corresponding updated_trajectory topic
  std::unordered_map<std::string, std::shared_ptr<TrajectoryPublisherNode>>
      trajectory_publishers;
  for (const auto& kv : updated_trajectory_topics) {
    const std::string& quad_name = kv.first;
    const std::string& topic = kv.second;
    trajectory_publishers[quad_name] =
        std::make_shared<TrajectoryPublisherNode>(topic);
  }

  std::map<std::string, std::string> quad_state_topics;
  if (false == nh.getParam("quad_state_topics", quad_state_topics)) {
    std::cerr << "Required parameter not found on server: quad_state_topics"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }

  std::vector<std::string> quad_names;
  for (const auto& kv : quad_state_topics) {
    quad_names.push_back(kv.first);
  }

  // Parse the initial quad positions
  std::map<std::string, std::string> initial_quad_positions_string;
  if (false ==
      nh.getParam("initial_quad_positions", initial_quad_positions_string)) {
    std::cerr
        << "Required parameter not found on server: initial_quad_positions"
        << std::endl;
    std::exit(EXIT_FAILURE);
  }
  std::map<
      std::string, Eigen::Vector3d, std::less<std::string>,
      Eigen::aligned_allocator<std::pair<const std::string, Eigen::Vector3d>>>
      initial_quad_positions;
  for (const auto& kv : initial_quad_positions_string) {
    const std::string& quad_name = kv.first;
    const std::string& quad_position_string = kv.second;
    std::stringstream ss(quad_position_string);
    double x, y, z;
    ss >> x >> y >> z;
    initial_quad_positions[quad_name] = Eigen::Vector3d(x, y, z);
  }

  // Initialize the QuadStateWarden. The QuadStateWarden enables safe,
  // multi-threaded access to quadcopter state data. Internal components that
  // require access to state data should request access through QuadStateWarden.
  auto quad_state_warden = std::make_shared<QuadStateWarden>();
  for (const auto& kv : quad_state_topics) {
    const std::string& quad_name = kv.first;
    quad_state_warden->Register(quad_name);

    const Eigen::Vector3d& initial_quad_position =
        initial_quad_positions[quad_name];
    quad_state_warden->Write(
        quad_name,
        QuadState((Eigen::Matrix<double, 13, 1>() << initial_quad_position(0),
                   initial_quad_position(1), initial_quad_position(2), 0, 0, 0,
                   1, 0, 0, 0, 0, 0, 0)
                      .finished()));
  }

  int revision_mode = 0;
  if (false == nh.getParam("revision_mode", revision_mode)) {
    std::cerr << "Required parameter not found on server: revision_mode"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }

  int quad_safety_limits = 0;
  if (false == nh.getParam("quad_safety_limits", quad_safety_limits)) {
    std::cerr << "Required parameter not found on server: quad_safety_limits"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }

  bool joy_mode;
  if (false == nh.getParam("joy_mode", joy_mode)) {
    std::cerr << "Required parameter not found on server: joy_mode"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }

  bool camera_mode;
  if (false == nh.getParam("camera_mode", camera_mode)) {
    std::cerr << "Required parameter not found on server: camera_mode"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // For every quad, subscribe to its corresponding state topic
  std::vector<std::shared_ptr<QuadStateSubscriberNode>> state_subscribers;
  for (const auto& kv : quad_state_topics) {
    const std::string& quad_name = kv.first;
    const std::string& topic = kv.second;
    state_subscribers.push_back(std::make_shared<QuadStateSubscriberNode>(
        topic, quad_name, quad_state_warden));
  }

  std::vector<double> blue_balloon_position_vector;
  if (false ==
      nh.getParam("blue_balloon_position", blue_balloon_position_vector)) {
    std::cerr << "Required parameter not found on server: blue_balloon_position"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }
  Eigen::Vector3d blue_balloon_position(blue_balloon_position_vector[0],
                                        blue_balloon_position_vector[1],
                                        blue_balloon_position_vector[2]);

  std::vector<double> red_balloon_position_vector;
  if (false ==
      nh.getParam("red_balloon_position", red_balloon_position_vector)) {
    std::cerr << "Required parameter not found on server: red_balloon_position"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }
  Eigen::Vector3d red_balloon_position(red_balloon_position_vector[0],
                                       red_balloon_position_vector[1],
                                       red_balloon_position_vector[2]);

  std::vector<double> goal_position_vector;
  if (false == nh.getParam("goal_position", goal_position_vector)) {
    std::cerr << "Required parameter not found on server: goal_position"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }
  Eigen::Vector3d goal_position(goal_position_vector[0],
                                goal_position_vector[1],
                                goal_position_vector[2]);

  bool move_blue;
  if (false == nh.getParam("move_blue", move_blue)) {
    std::cerr << "Required parameter not found on server: move_blue"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }

  double blue_balloon_max_move_time;
  Eigen::Vector3d blue_balloon_position_new;
  if (move_blue) {
    // new position vectors
    std::vector<double> blue_balloon_position_vector_new;
    if (false == nh.getParam("blue_balloon_position_new",
                             blue_balloon_position_vector_new)) {
      std::cerr
          << "Required parameter not found on server: blue_balloon_position_new"
          << std::endl;
      std::exit(EXIT_FAILURE);
    }
    blue_balloon_position_new << blue_balloon_position_vector_new[0],
        blue_balloon_position_vector_new[1],
        blue_balloon_position_vector_new[2];

    if (false ==
        nh.getParam("blue_balloon_max_move_time", blue_balloon_max_move_time)) {
      std::cerr << "Required parameter not found on server: "
                   "blue_balloon_max_move_time"
                << std::endl;
      std::exit(EXIT_FAILURE);
    }
  } else {
    blue_balloon_max_move_time = 600.0;
    blue_balloon_position_new = blue_balloon_position;
  }

  bool move_red;
  if (false == nh.getParam("move_red", move_red)) {
    std::cerr << "Required parameter not found on server: move_red"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }

  double red_balloon_max_move_time;
  Eigen::Vector3d red_balloon_position_new;
  if (move_red) {
    std::vector<double> red_balloon_position_vector_new;
    if (false == nh.getParam("red_balloon_position_new",
                             red_balloon_position_vector_new)) {
      std::cerr
          << "Required parameter not found on server: red_balloon_position_new"
          << std::endl;
      std::exit(EXIT_FAILURE);
    }
    red_balloon_position_new << red_balloon_position_vector_new[0],
        red_balloon_position_vector_new[1], red_balloon_position_vector_new[2];

    if (false ==
        nh.getParam("red_balloon_max_move_time", red_balloon_max_move_time)) {
      std::cerr
          << "Required parameter not found on server: red_balloon_max_move_time"
          << std::endl;
      std::exit(EXIT_FAILURE);
    }
  } else {
    red_balloon_max_move_time = 600.0;
    red_balloon_position_new = red_balloon_position;
  }

  // Balloon Status
  std::map<std::string, std::string> balloon_status_topics;
  if (false == nh.getParam("balloon_status_topics", balloon_status_topics)) {
    std::cerr << "Required parameter not found on server: balloon_status_topics"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // Balloon Position
  std::map<std::string, std::string> balloon_position_topics;
  if (false ==
      nh.getParam("balloon_position_topics", balloon_position_topics)) {
    std::cerr
        << "Required parameter not found on server: balloon_position_topics"
        << std::endl;
    std::exit(EXIT_FAILURE);
  }

  for (auto& kv : balloon_position_topics) {
    // ML only cares about the true location of the balloon
    std::string& balloon_position_topic = kv.second;
    balloon_position_topic += "/true";
  }

  // Goal Status
  std::map<std::string, std::string> goal_status_topics;
  if (false == nh.getParam("goal_status_topics", goal_status_topics)) {
    std::cerr << "Required parameter not found on server: goal_status_topics"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // Seed for balloon position change
  bool use_seed;
  if (false == nh.getParam("use_seed", use_seed)) {
    std::cerr << "Required parameter not found on server: use_seed"
              << std::endl;
    std::exit(EXIT_FAILURE);
  }

  int seed;
  if (use_seed) {
    if (false == nh.getParam("seed", seed)) {
      std::cerr << "Required parameter not found on server: seed" << std::endl;
      std::exit(EXIT_FAILURE);
    }
  } else {
    seed = std::random_device{}();
  }

  std::mt19937 gen{static_cast<unsigned>(seed)};

  auto red_balloon_status = std::make_shared<BalloonStatus>();
  auto blue_balloon_status = std::make_shared<BalloonStatus>();

  auto red_balloon_status_subscriber_node =
      std::make_shared<BalloonStatusSubscriberNode>(
          balloon_status_topics["red"], red_balloon_status);
  auto blue_balloon_status_subscriber_node =
      std::make_shared<BalloonStatusSubscriberNode>(
          balloon_status_topics["blue"], blue_balloon_status);

  auto red_balloon_status_publisher_node =
      std::make_shared<BalloonStatusPublisherNode>(
          balloon_status_topics["red"]);
  auto blue_balloon_status_publisher_node =
      std::make_shared<BalloonStatusPublisherNode>(
          balloon_status_topics["blue"]);

  auto red_balloon_position_publisher_node =
      std::make_shared<BalloonPositionPublisherNode>(
          balloon_position_topics["red"]);
  auto blue_balloon_position_publisher_node =
      std::make_shared<BalloonPositionPublisherNode>(
          balloon_position_topics["blue"]);

  auto red_balloon_watchdog = std::make_shared<BalloonWatchdog>();
  auto blue_balloon_watchdog = std::make_shared<BalloonWatchdog>();

  std::thread red_balloon_watchdog_thread([&]() {
    red_balloon_watchdog->Run(
        red_balloon_status_publisher_node, red_balloon_status_subscriber_node,
        red_balloon_position_publisher_node, quad_state_warden, quad_names,
        red_balloon_position, red_balloon_position_new,
        red_balloon_max_move_time, gen, "manual_red_pop");
  });

  std::thread blue_balloon_watchdog_thread([&]() {
    blue_balloon_watchdog->Run(
        blue_balloon_status_publisher_node, blue_balloon_status_subscriber_node,
        blue_balloon_position_publisher_node, quad_state_warden, quad_names,
        blue_balloon_position, blue_balloon_position_new,
        blue_balloon_max_move_time, gen, "manual_blue_pop");
  });

  // Status watchdogs
  auto quad_state_watchdog_status = std::make_shared<QuadStateWatchdogStatus>();
  auto trajectory_watchdog_status =
      std::make_shared<TrajectoryWatchdogStatus>();
  auto safety_monitor_status = std::make_shared<SafetyMonitorStatus>();
  for (const std::string& quad_name : quad_names) {
    quad_state_watchdog_status->Register(quad_name);
    trajectory_watchdog_status->Register(quad_name);
    safety_monitor_status->Register(quad_name);
  }

  auto quad_state_watchdog =
      std::make_shared<QuadStateWatchdog>(quad_safety_limits, joy_mode);
  std::thread quad_state_watchdog_thread([&]() {
    quad_state_watchdog->Run(quad_state_warden, quad_names,
                             quad_state_watchdog_status, map);
  });

  auto trajectory_watchdog = std::make_shared<TrajectoryWatchdog>();
  std::thread trajectory_watchdog_thread([&]() {
    trajectory_watchdog->Run(quad_names, quad_state_warden,
                             trajectory_warden_srv, trajectory_warden_pub,
                             trajectory_watchdog_status);
  });

  //  auto safety_monitor = std::make_shared<SafetyMonitor>(revision_mode);
  //  std::thread safety_monitor_thread(
  //      [&]() {
  //          safety_monitor->Run(
  //              quad_names,
  //              safety_monitor_status,
  //              quad_state_warden,
  //              quad_state_watchdog_status,
  //              trajectory_warden_pub,
  //              trajectory_watchdog_status,
  //              map);
  //      }
  //  );

  auto goal_status = std::make_shared<GoalStatus>();

  auto goal_status_subscriber_node = std::make_shared<GoalStatusSubscriberNode>(
      goal_status_topics["home"], goal_status);
  auto goal_status_publisher_node =
      std::make_shared<GoalStatusPublisherNode>(goal_status_topics["home"]);
  auto goal_watchdog = std::make_shared<GoalWatchdog>();

  std::thread goal_watchdog_thread([&]() {
    goal_watchdog->Run(goal_status_publisher_node, goal_status_subscriber_node,
                       quad_state_warden, quad_names, goal_position);
  });

  // Mediation layer thread. The mediation layer runs continuously, forward
  // integrating the proposed trajectories and modifying them so that the
  // various agents will not crash into each other. Data is asynchonously read
  // and written from the TrajectoryWardens
  auto mediation_layer =
      std::make_shared<MediationLayer>(quad_safety_limits, joy_mode);
  std::thread mediation_layer_thread([&]() {
    mediation_layer->Run(map, trajectory_warden_srv, trajectory_warden_pub,
                         quad_state_warden, quad_state_watchdog_status,
                         trajectory_watchdog_status, safety_monitor_status,
                         trajectory_publishers);
  });

  // Kill program thread. This thread sleeps for a second and then checks if the
  // 'kill_program' variable has been set. If it has, it shuts ros down and
  // sends stop signals to any other threads that might be running.
  std::thread kill_thread([&]() {
    while (true) {
      if (true == kill_program) {
        break;
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      }
    }
    ros::shutdown();

    mediation_layer->Stop();
    trajectory_warden_srv->Stop();
    trajectory_warden_pub->Stop();
    quad_state_warden->Stop();
    red_balloon_watchdog->Stop();
    blue_balloon_watchdog->Stop();
    goal_watchdog->Stop();
    quad_state_watchdog->Stop();
    trajectory_watchdog->Stop();
    //        safety_monitor->Stop();
  });

  // Spin for ros subscribers
  ros::spin();

  // Wait for program termination via ctl-c
  kill_thread.join();

  // Wait for other threads to die
  mediation_layer_thread.join();
  red_balloon_watchdog_thread.join();
  blue_balloon_watchdog_thread.join();
  goal_watchdog_thread.join();
  quad_state_watchdog_thread.join();
  trajectory_watchdog_thread.join();
  //  safety_monitor_thread.join();

  return EXIT_SUCCESS;
}
