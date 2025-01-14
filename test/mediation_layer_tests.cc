#include <cstdlib>
#include <iostream>
#include <Eigen/StdVector>

#undef NDEBUG
#include <cassert>

#include "trajectory.h"
#include "quad_state.h"
#include "warden.h"

using namespace game_engine;

void test_TrajectoryWarden() {
  { // Trivial
    TrajectoryWardenSubscriber warden;

    Trajectory dummy_trajectory;
    assert(0 == warden.Keys().size());
    assert(MediationLayerCode::Success == warden.Register(""));
    assert(MediationLayerCode::Success == warden.Read("", dummy_trajectory));
    assert(MediationLayerCode::Success == warden.Write("", dummy_trajectory));
    assert(MediationLayerCode::Success == warden.Await("", dummy_trajectory));
  }

  { // Test read/write
    TrajectoryWardenSubscriber warden;

    Trajectory trajectory_write({(Eigen::Matrix<double, 11, 1>() << 1,1,1,1,1,1,1,1,1,1,1).finished()});
    assert(MediationLayerCode::Success == warden.Register("test"));
    assert(MediationLayerCode::Success == warden.Write("test", trajectory_write));

    Trajectory trajectory_read;
    assert(MediationLayerCode::Success == warden.Read("test", trajectory_read));
    assert(trajectory_read.Size() == trajectory_write.Size());
    assert(trajectory_read.PVAYT(0).isApprox(trajectory_write.PVAYT(0)));
  }
}

void test_Trajectory() {
  { // Trivial
    Trajectory trajectory;
    assert(0 == trajectory.Size());
  }

  { // Test access
    TrajectoryVector3D hist = {};
    hist.push_back((Eigen::Matrix<double, 11, 1>() << 1,1,1,2,2,2,3,3,3,0.1,0.2).finished());
    Trajectory trajectory(hist);

    assert(Eigen::Vector3d(1,1,1).isApprox(trajectory.Position(0)));
    assert(Eigen::Vector3d(2,2,2).isApprox(trajectory.Velocity(0)));
    assert(Eigen::Vector3d(3,3,3).isApprox(trajectory.Acceleration(0)));
    assert(0.1 == trajectory.Yaw(0));
    assert(0.2 == trajectory.Time(0));
    assert(((Eigen::Matrix<double, 9, 1>() << 1,1,1,2,2,2,3,3,3).finished()).isApprox(trajectory.PVA(0)));
    assert(((Eigen::Matrix<double, 11, 1>() << 1,1,1,2,2,2,3,3,3,0.1,0.2).finished()).isApprox(trajectory.PVAYT(0)));
  }
}

void test_QuadState() {
  { // Trivial
    QuadState state((Eigen::Matrix<double, 13, 1>() << 1,1,1,2,2,2,1,0,0,0,1,2,3).finished());
    assert((Eigen::Vector3d(1,1,1)).isApprox(state.Position()));
    assert((Eigen::Vector3d(2,2,2)).isApprox(state.Velocity()));
    assert((Eigen::Vector4d(1,0,0,0)).isApprox(state.Orientation()));
    assert((Eigen::Vector3d(1,2,3)).isApprox(state.Twist()));
  }
}

void test_QuadStateWarden() {
  { // Trivial
    QuadStateWarden warden;

    QuadState dummy_state;
    assert(0 == warden.Keys().size());
    assert(MediationLayerCode::Success == warden.Register(""));
    assert(MediationLayerCode::Success == warden.Read("", dummy_state));
    assert(MediationLayerCode::Success == warden.Write("", dummy_state));
    assert(MediationLayerCode::Success == warden.Await("", dummy_state));
  }

  { // Test read/write
    QuadStateWarden warden;

    QuadState state_write({(Eigen::Matrix<double, 13, 1>() << 0,0,0,0,0,0,1,0,0,0,0,0,0).finished()});
    assert(MediationLayerCode::Success == warden.Register("test"));
    assert(MediationLayerCode::Success == warden.Write("test", state_write));

    QuadState state_read;
    assert(MediationLayerCode::Success == warden.Read("test", state_read));
    assert(state_read.Position().isApprox(state_write.Position()));
    assert(state_read.Velocity().isApprox(state_write.Velocity()));
    assert(state_read.Orientation().isApprox(state_write.Orientation()));
    assert(state_read.Twist().isApprox(state_write.Twist()));
  }
}

int main(int argc, char** argv) {
  test_Trajectory();
  test_TrajectoryWarden();
  test_QuadState();
  test_QuadStateWarden();

  std::cout << "All tests passed!" << std::endl;
  return EXIT_SUCCESS;
}
