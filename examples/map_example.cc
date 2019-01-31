// Author: Tucker Haydon

#include <iostream>
#include <vector>
#include <algorithm>
#include <iostream>

#include "node.h"
#include "directed_edge.h"
#include "graph.h"
#include "dijkstra.h"
#include "occupancy_grid2d.h"
#include "a_star.h"
#include "gnu_visualizer.h"

using namespace path_planning;
using namespace geometry;


std::ostream & operator<<(std::ostream& out, const Node& node) {
  const int* data = reinterpret_cast<const int*>(node.Data());
  out << data[0] << ", " << data[1];
}

void RunAStar(const Graph& graph, 
              const OccupancyGrid2D& occupancy_grid,
              const Node& start, 
              const Node& end) {

    const std::vector<Node> path = AStar().Run(graph, start, end);

    std::cout << "Path (" << path.size() << " nodes):" << std::endl;
    std::for_each(
        path.begin(),
        path.end(),
        [](const Node& node){
          std::cout << node << std::endl;
        });

    GNUVisualizer().Run(occupancy_grid, path);
}

int main(int argc, char** argv) {

  Map2D map;
  {
    Polygon boundary;
    {
      const Point2D a(0,0), b(10,0), c(10,10), d(0,10);
      const Line2D l1(a,b), l2(b,c), l3(c,d), l4(d,a);
      boundary.SetEdges({l1,l2,l3,l4});
    }

    Polygon obstacle;
    {
      const Point2D a(4,4), b(6,4), c(6,6), d(4,6);
      const Line2D l1(a,b), l2(b,c), l3(c,d), l4(d,a);
      obstacle.SetEdges({l1,l2,l3,l4});
    }

    map.SetBoundary(boundary);
    map.SetObstacles({obstacle});
  }

  const double SAFETY_BOUND = 0.1;
  const double SAMPLE_DELTA = 0.5;

  OccupancyGrid2D occupancy_grid;
  occupancy_grid.LoadFromMap(map.Inflate(SAFETY_BOUND), SAMPLE_DELTA);
  Graph graph(occupancy_grid);

  const size_t data_size = 2*sizeof(int);
  const double start[2] = {0.2,0.2}, end[2] = {9.8,9.8};
  const int start_int[2] = {(int)(start[0]/SAMPLE_DELTA),(int)(start[1]/SAMPLE_DELTA)};
  const int end_int[2] = {(int)(end[0]/SAMPLE_DELTA),(int)(end[1]/SAMPLE_DELTA)};

  Node start_node, end_node;
  start_node.SetData(reinterpret_cast<const uint8_t*>(start_int), data_size);
  end_node.SetData(reinterpret_cast<const uint8_t*>(end_int), data_size);

  RunAStar(graph, occupancy_grid, start_node, end_node);

  return 0;
}
