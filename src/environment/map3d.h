#pragma once

#include <cstdlib>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "polyhedron.h"
#include "yaml-cpp/yaml.h"

namespace game_engine {
// The Map3D class encapsulates data about static obstacles and map
// boundaries. The map maintains data structures, setters, and accessors to
// the various planes, lines, and vertices that make up a map.
class Map3D {
 private:
  // The boundary of the map is represented by a convex polyhedron
  Polyhedron boundary_;

  // The obstacles in the map are represented by a list of convex polyhedra
  std::vector<Polyhedron> obstacles_;

  // The dynamic obstacles in the map are represented by a list of convex
  // polyhedra
  std::unordered_map<std::string, Polyhedron> dynamic_obstacles_;

  // Forward-declare friend class for parsing
  friend class YAML::convert<Map3D>;

 public:
  // Constructor
  Map3D(const Polyhedron& boundary = Polyhedron(),
        const std::vector<Polyhedron>& obstacles = {})
      : boundary_(boundary), obstacles_(obstacles) {}

  // Boundary accessor
  const Polyhedron& Boundary() const;

  // Obstacles accessor
  const std::vector<Polyhedron>& Obstacles() const;

  // Obstacles accessor
  void AddInflatedDynamicObstacle(const std::string& quad_name,
                                  const Polyhedron& dynamic_obstacle,
                                  const double distance);

  // Determines whether or not a point is considered in a free non dynamic spot.
  bool IsFreeDynamicSpace(const std::string& quad_name, const Point3D& point);

  // Clear dynamic obstacles
  void ClearDynamicObstacles();

  // Determines whether or not a point is contained within the map
  bool Contains(const Point3D& point) const;

  // Determines whether or not a point is considered free space. A point is
  // free space if it is contained in the map and not contained in any
  // obstacle
  bool IsFreeSpace(const Point3D& point) const;

  // Determines the extents of the map. Returns a list of tuples that
  // contain the {min,max} coordinates for the XYZ dimensions.
  std::vector<std::pair<double, double>> Extents() const;

  // Returns the plane with the smallest average z-coordinate
  Plane3D Ground() const;

  // Returns the list of planes minus the ground plane
  std::vector<Plane3D> Walls() const;

  // Inflates a map by a set distance. Map boundaries are shrunk and
  // obstacles are expanded. Shrinking and expanding affects x, y, and z
  // directions equally, therefore object aspect ratios are not guaranteed
  // to stay the same.
  Map3D Inflate(const double distance) const;

  // Returns the point closest to the given point that is either on the
  // map boundary or any obstacle
  Point3D ClosestPoint(const Point3D& point) const;
};
}  // namespace game_engine

namespace YAML {
template <>
struct convert<game_engine::Map3D> {
  static Node encode(const game_engine::Map3D& rhs) {
    Node node;
    node["boundary"] = rhs.boundary_;
    node["obstacles"] = rhs.obstacles_;
    return node;
  }

  static bool decode(const Node& node, game_engine::Map3D& rhs) {
    if (!node.IsMap() || !node["boundary"]) {
      return false;
    }

    rhs.boundary_ = node["boundary"].as<game_engine::Polyhedron>();

    if (node["obstacles"]) {
      rhs.obstacles_ =
          node["obstacles"].as<std::vector<game_engine::Polyhedron>>();
    }

    return true;
  }
};
}  // namespace YAML
