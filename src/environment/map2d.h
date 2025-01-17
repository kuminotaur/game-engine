

#pragma once

#include <vector>

#include "polygon.h"
#include "yaml-cpp/yaml.h"

namespace game_engine {
// The Map3D class encapsulates data about static obstacles and map
// boundaries. The map maintains data structures, setters, and accessors to
// the various planes, lines, and vertices that make up a map.
class Map2D {
 private:
  // The boundary of the map is represented by a convex polygon
  Polygon boundary_;

  // The obstacles in the map are represented by a list of convex polygons
  std::vector<Polygon> obstacles_;

  // Forward-declare friend class for parsing
  friend class YAML::convert<Map2D>;

 public:
  Map2D(const Polygon& boundary = Polygon(),
        const std::vector<Polygon>& obstacles = {})
      : boundary_(boundary), obstacles_(obstacles) {}

  // Boundary accessor
  const Polygon& Boundary() const;

  // Boundry setter
  bool SetBoundary(const Polygon& boundary);

  // Obstacles accessor
  const std::vector<Polygon>& Obstacles() const;

  // Obstacles setter
  bool SetObstacles(const std::vector<Polygon>& obtacles);

  // Determines whether or not a point is contained within the map
  bool Contains(const Point2D& point) const;

  // Determines whether or not a point is considered free space. A point is
  // free space if it is contained in the map and not contained in any
  // obstacle
  bool IsFreeSpace(const Point2D& point) const;

  Polygon Extents() const;

  // Inflates a map by a set distance. Map boundaries are shrunk and
  // obstacles are expanded. Shrinking and expanding affects both x and y
  // directions equally, therefore object aspect ratios are not guaranteed
  // to stay the same.
  Map2D Inflate(const double distance) const;
};
}  // namespace game_engine

namespace YAML {
template <>
struct convert<game_engine::Map2D> {
  static Node encode(const game_engine::Map2D& rhs) {
    Node node;
    node["boundary"] = rhs.boundary_;
    node["obstacles"] = rhs.obstacles_;
    return node;
  }

  static bool decode(const Node& node, game_engine::Map2D& rhs) {
    if (!node.IsMap() || !node["boundary"]) {
      return false;
    }

    rhs.boundary_ = node["boundary"].as<game_engine::Polygon>();

    if (node["obstacles"]) {
      rhs.obstacles_ =
          node["obstacles"].as<std::vector<game_engine::Polygon>>();
    }

    return true;
  }
};
}  // namespace YAML
