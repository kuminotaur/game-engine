#include "quad_view.h"

namespace game_engine {
std::vector<visualization_msgs::Marker> QuadView::Markers() const {
  QuadState quad_state;
  this->quad_state_warden_->Read(quad_name_, quad_state);

  const Eigen::Vector3d quad_position = quad_state.Position();

  visualization_msgs::Marker marker;
  marker.header.frame_id = this->options_.frame_id;
  marker.id = this->unique_id_;
  marker.ns = "Quad";
  marker.type = visualization_msgs::Marker::MESH_RESOURCE;
  marker.action = visualization_msgs::Marker::ADD;
  marker.scale.x = 1.0f;
  marker.scale.y = 1.0f;
  marker.scale.z = 1.0f;
  // Empirical shifts since quad stl does not have origin at quad CM
  marker.pose.position.x = quad_position.x() - 0.15;
  marker.pose.position.y = quad_position.y() - 0.15;
  marker.pose.position.z = quad_position.z() - 0.05;
  marker.pose.orientation.w = 0.5f;
  marker.pose.orientation.x = 0.5f;
  marker.pose.orientation.y = 0.5f;
  marker.pose.orientation.z = 0.5f;
  marker.color.r = this->options_.r;
  marker.color.g = this->options_.g;
  marker.color.b = this->options_.b;
  marker.color.a = this->options_.a;
  marker.mesh_resource = this->options_.mesh_resource;
  marker.mesh_use_embedded_materials = true;

  return {marker};
}

uint32_t QuadView::GenerateUniqueId() {
  static std::mutex mtx;
  static uint32_t id = 0;

  std::lock_guard<std::mutex> lock(mtx);
  id++;
  return id;
}
}  // namespace game_engine
