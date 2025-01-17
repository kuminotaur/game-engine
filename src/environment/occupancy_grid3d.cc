#include "occupancy_grid3d.h"

#include <iostream>

namespace game_engine {

OccupancyGrid3D::~OccupancyGrid3D() {
  if (false == this->heap_allocated_) {
    return;
  }

  // Deallocate memory on heap
  for (size_t idx = 0; idx < this->size_z_; ++idx) {
    for (size_t idx2 = 0; idx2 < this->size_y_; ++idx2) {
      std::free(this->data_[idx][idx2]);
    }
  }
  std::free(this->data_);
}

bool OccupancyGrid3D::IsOccupied(const size_t z, const size_t y,
                                 const size_t x) const {
  if (x < 0 || y < 0 || z < 0 || x >= size_x_ || y >= size_y_ || z >= size_z_) {
    return true;
  }

  return this->data_[z][y][x];
}

const bool*** OccupancyGrid3D::Data() const {
  return const_cast<const bool***>(this->data_);
}

bool OccupancyGrid3D::LoadFromFile(const std::string& file_path) {
  std::ifstream f(file_path);
  if (!f.is_open()) {
    std::cerr << "OccupancyGrid3D::LoadFromFile: File could not be opened."
              << std::endl;
    return false;
  }

  f >> this->size_y_;
  f >> this->size_x_;
  f >> this->size_z_;

  // Allocate memory on the heap for the file
  this->data_ =
      reinterpret_cast<bool***>(std::malloc(this->size_z_ * sizeof(bool*)));
  for (size_t idx = 0; idx < this->size_z_; ++idx) {
    this->data_[idx] =
        reinterpret_cast<bool**>(std::malloc(this->size_y_ * sizeof(bool*)));
    for (size_t idx2 = 0; idx2 < this->size_y_; ++idx2) {
      this->data_[idx][idx2] =
          reinterpret_cast<bool*>(std::malloc(this->size_x_ * sizeof(bool)));
    }
  }
  this->heap_allocated_ = true;

  // Read in file
  for (size_t height = 0; height < this->size_z_; ++height) {
    for (size_t row = 0; row < this->size_y_; ++row) {
      for (size_t col = 0; col < this->size_x_; ++col) {
        f >> this->data_[height][row][col];
      }
    }
  }
  f.close();
  return true;
}

bool OccupancyGrid3D::LoadFromMap(const Map3D& map, const double sample_delta,
                                  const double safety_bound) {
  double min_x{std::numeric_limits<double>::max()},
      min_y{std::numeric_limits<double>::max()},
      min_z{std::numeric_limits<double>::max()},
      max_x{std::numeric_limits<double>::min()},
      max_y{std::numeric_limits<double>::min()},
      max_z{std::numeric_limits<double>::min()};

  // We are setting the limits of OccupancyGrid3D based on map3D boundaries.
  // map2D boundary is just given by a plane, which consist of vertices.
  // map3D boundary is given by multiple planes.  The planes can be obtained
  // from Faces() method.

  for (const Plane3D& plane : map.Boundary().Faces()) {
    for (const Line3D& edge : plane.Edges()) {
      // Each line3d object has a start point and end point.
      for (int i = 0; i < 2; i++) {
        Point3D vertex;
        if (i == 0)
          vertex = edge.Start();
        else
          vertex = edge.End();

        if (vertex.x() < min_x) {
          min_x = vertex.x();
        }
        if (vertex.y() < min_y) {
          min_y = vertex.y();
        }
        if (vertex.z() < min_z) {
          min_z = vertex.z();
        }
        if (vertex.x() > max_x) {
          max_x = vertex.x();
        }
        if (vertex.y() > max_y) {
          max_y = vertex.y();
        }
        if (vertex.z() > max_z) {
          max_z = vertex.z();
        }
      }
    }
  }

  this->size_y_ = std::ceil((max_y - min_y) / sample_delta) + 1;
  this->size_x_ = std::ceil((max_x - min_x) / sample_delta) + 1;
  this->size_z_ = std::ceil((max_z - min_z) / sample_delta) + 1;

  Eigen::Vector3d origin(min_x, min_y, min_z);
  this->origin_ = origin;
  this->gridsize_ = sample_delta;

  // Allocate memory on the heap for the file
  this->data_ =
      reinterpret_cast<bool***>(std::malloc(this->size_z_ * sizeof(bool*)));
  for (size_t idx = 0; idx < this->size_z_; ++idx) {
    this->data_[idx] =
        reinterpret_cast<bool**>(std::malloc(this->size_y_ * sizeof(bool*)));
    for (size_t idx2 = 0; idx2 < this->size_y_; ++idx2) {
      this->data_[idx][idx2] =
          reinterpret_cast<bool*>(std::malloc(this->size_x_ * sizeof(bool)));
    }
  }
  this->heap_allocated_ = true;

  const Map3D inflated_map = map.Inflate(safety_bound);

  // Read in file
  for (size_t height = 0; height < this->size_z_; ++height) {
    for (size_t row = 0; row < this->size_y_; ++row) {
      for (size_t col = 0; col < this->size_x_; ++col) {
        Point3D p(min_x + col * sample_delta + sample_delta * .5,
                  min_y + row * sample_delta + sample_delta * .5,
                  min_z + height * sample_delta + sample_delta * .5);
        // True indicates occupied, false indicates free
        this->data_[height][row][col] =
            !inflated_map.Contains(p) || !inflated_map.IsFreeSpace(p);
      }
    }
  }
  return true;
}

bool OccupancyGrid3D::LoadFromBuffer(const bool** buffer, const size_t size_x,
                                     const size_t size_y, const size_t size_z) {
  this->size_y_ = size_y;
  this->size_x_ = size_x;
  this->size_z_ = size_z;

  // Allocate memory on the heap for the file
  this->data_ =
      reinterpret_cast<bool***>(std::malloc(this->size_z_ * sizeof(bool*)));
  for (size_t idx = 0; idx < this->size_z_; ++idx) {
    this->data_[idx] =
        reinterpret_cast<bool**>(std::malloc(this->size_y_ * sizeof(bool*)));
    for (size_t idx2 = 0; idx2 < this->size_y_; ++idx2) {
      this->data_[idx][idx2] =
          reinterpret_cast<bool*>(std::malloc(this->size_x_ * sizeof(bool)));
    }
  }
  this->heap_allocated_ = true;

  return true;
}

// Returns the minimum corner coordinates of the grid cell at index [x,y,z]
Eigen::Vector3d OccupancyGrid3D::boxCorner(int x, int y, int z) {
  return Eigen::Vector3d(x * gridsize_ + origin_.x(), y * gridsize_ + origin_.y(),
                         z * gridsize_ + origin_.z());
}

// Returns the center coordinates of the grid cell at index [x,y,z]
Eigen::Vector3d OccupancyGrid3D::boxCenter(int x, int y, int z) {
  return boxCorner(x, y, z) +
         Eigen::Vector3d(gridsize_ * 0.5, gridsize_ * 0.5, gridsize_ * 0.5);
}

std::tuple<int, int, int> OccupancyGrid3D::mapToGridCoordinates(
    Eigen::Vector3d pt) {
  return std::tuple<int, int, int>(floor((pt[0] - origin_.x()) / gridsize_),
                                   floor((pt[1] - origin_.y()) / gridsize_),
                                   floor((pt[2] - origin_.z()) / gridsize_));
}

Graph3D OccupancyGrid3D::AsGraph() const {
  // Build a 3D array of nodes
  std::shared_ptr<Node3D> node_grid[this->size_z_][this->size_y_]
                                   [this->size_x_];
  for (size_t height = 0; height < this->size_z_; ++height) {
    for (size_t row = 0; row < this->size_y_; ++row) {
      for (size_t col = 0; col < this->size_x_; ++col) {
        node_grid[height][row][col] = std::make_shared<Node3D>(
            Eigen::Matrix<double, 3, 1>(col, row, height));
      }
    }
  }
  // Convert node grid to directed edges and add to graph
  std::vector<DirectedEdge3D> edges;
  for (int height = 0; height < this->size_z_; ++height) {
    for (int row = 0; row < this->size_y_; ++row) {
      for (int col = 0; col < this->size_x_; ++col) {
        if (true == this->IsOccupied(height, row, col)) continue;

        constexpr double ADJACENT_COST = 1.0;
        constexpr double DIAGONAL_COST = std::sqrt(2);
        constexpr double VERTEX_COST = std::sqrt(3);

        // DirectedEdge3D has 3 attributes: Source, sink, cost.
        // The 6 adjacent nodes:
        if (row - 1 >= 0)
          edges.emplace_back(node_grid[height][row - 1][col],
                             node_grid[height][row][col], ADJACENT_COST);
        if (col - 1 >= 0)
          edges.emplace_back(node_grid[height][row][col - 1],
                             node_grid[height][row][col], ADJACENT_COST);
        if (height - 1 >= 0)
          edges.emplace_back(node_grid[height - 1][row][col],
                             node_grid[height][row][col], ADJACENT_COST);
        if (row + 1 < this->size_y_)
          edges.emplace_back(node_grid[height][row + 1][col],
                             node_grid[height][row][col], ADJACENT_COST);
        if (col + 1 < this->size_x_)
          edges.emplace_back(node_grid[height][row][col + 1],
                             node_grid[height][row][col], ADJACENT_COST);
        if (height + 1 < this->size_z_)
          edges.emplace_back(node_grid[height + 1][row][col],
                             node_grid[height][row][col], ADJACENT_COST);

        // The 12 diagonal nodes:
        if (row - 1 >= 0 && col - 1 >= 0) {
          edges.emplace_back(node_grid[height][row - 1][col - 1],
                             node_grid[height][row][col], DIAGONAL_COST);
        }
        if (row - 1 >= 0 && col + 1 < this->size_x_) {
          edges.emplace_back(node_grid[height][row - 1][col + 1],
                             node_grid[height][row][col], DIAGONAL_COST);
        }
        if (row + 1 < this->size_y_ && col - 1 >= 0) {
          edges.emplace_back(node_grid[height][row + 1][col - 1],
                             node_grid[height][row][col], DIAGONAL_COST);
        }
        if (row + 1 < this->size_y_ && col + 1 < this->size_x_) {
          edges.emplace_back(node_grid[height][row + 1][col + 1],
                             node_grid[height][row][col], DIAGONAL_COST);
        }
        // topface of the cube
        if (row + 1 < this->size_y_ && height + 1 < this->size_z_) {
          edges.emplace_back(node_grid[height + 1][row + 1][col],
                             node_grid[height][row][col], DIAGONAL_COST);
        }
        if (row - 1 >= 0 && height + 1 < this->size_z_) {
          edges.emplace_back(node_grid[height + 1][row - 1][col],
                             node_grid[height][row][col], DIAGONAL_COST);
        }
        if (col + 1 < this->size_x_ && height + 1 < this->size_z_) {
          edges.emplace_back(node_grid[height + 1][row][col + 1],
                             node_grid[height][row][col], DIAGONAL_COST);
        }
        if (col - 1 >= 0 && height + 1 < this->size_z_) {
          edges.emplace_back(node_grid[height + 1][row][col - 1],
                             node_grid[height][row][col], DIAGONAL_COST);
        }
        // bottom face of the cube
        if (row + 1 < this->size_y_ && height - 1 >= 0) {
          edges.emplace_back(node_grid[height - 1][row + 1][col],
                             node_grid[height][row][col], DIAGONAL_COST);
        }
        if (row - 1 >= 0 && height - 1 >= 0) {
          edges.emplace_back(node_grid[height - 1][row - 1][col],
                             node_grid[height][row][col], DIAGONAL_COST);
        }
        if (col + 1 < this->size_x_ && height - 1 >= 0) {
          edges.emplace_back(node_grid[height - 1][row][col + 1],
                             node_grid[height][row][col], DIAGONAL_COST);
        }
        if (col - 1 >= 0 && height - 1 >= 0) {
          edges.emplace_back(node_grid[height - 1][row][col - 1],
                             node_grid[height][row][col], DIAGONAL_COST);
        }

        // The 8 vertex nodes:
        // Top face vertices of the block
        if (row + 1 < this->size_y_ && height + 1 < this->size_z_ &&
            col + 1 < this->size_x_) {
          edges.emplace_back(node_grid[height + 1][row + 1][col + 1],
                             node_grid[height][row][col], VERTEX_COST);
        }
        if (row + 1 < this->size_y_ && height + 1 < this->size_z_ &&
            col - 1 >= 0) {
          edges.emplace_back(node_grid[height + 1][row + 1][col - 1],
                             node_grid[height][row][col], VERTEX_COST);
        }
        if (row - 1 >= 0 && height + 1 < this->size_z_ &&
            col + 1 < this->size_x_) {
          edges.emplace_back(node_grid[height + 1][row - 1][col + 1],
                             node_grid[height][row][col], VERTEX_COST);
        }
        if (row - 1 >= 0 && height + 1 < this->size_z_ && col - 1 >= 0) {
          edges.emplace_back(node_grid[height + 1][row - 1][col - 1],
                             node_grid[height][row][col], VERTEX_COST);
        }
        // Bottom face vertices of the block
        if (row + 1 < this->size_y_ && height - 1 >= 0 &&
            col + 1 < this->size_x_) {
          edges.emplace_back(node_grid[height - 1][row + 1][col + 1],
                             node_grid[height][row][col], VERTEX_COST);
        }
        if (row + 1 < this->size_y_ && height - 1 >= 0 && col - 1 >= 0) {
          edges.emplace_back(node_grid[height - 1][row + 1][col - 1],
                             node_grid[height][row][col], VERTEX_COST);
        }
        if (row - 1 >= 0 && height - 1 >= 0 && col + 1 < this->size_x_) {
          edges.emplace_back(node_grid[height - 1][row - 1][col + 1],
                             node_grid[height][row][col], VERTEX_COST);
        }
        if (row - 1 >= 0 && height - 1 >= 0 && col - 1 >= 0) {
          edges.emplace_back(node_grid[height - 1][row - 1][col - 1],
                             node_grid[height][row][col], VERTEX_COST);
        }
      }
    }
  }
  return Graph3D(edges);
}

}  // namespace game_engine
