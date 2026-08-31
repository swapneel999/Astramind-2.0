// Copyright 2026 AstraMind
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "astramind_core/telem_ingest_node.hpp"

#include <chrono>
#include <cmath>

namespace astramind_core
{

TelemIngestNode::TelemIngestNode(const rclcpp::NodeOptions & options)
: Node("telem_ingest_node", options), mission_elapsed_time_s_(0.0)
{
  telemetry_rate_hz_ = this->declare_parameter<double>("telemetry_rate_hz", 10.0);

  publisher_ = this->create_publisher<astramind_interfaces::msg::SpacecraftTelemetry>(
    "spacecraft/telemetry", rclcpp::SensorDataQoS());

  auto period = std::chrono::duration<double>(1.0 / telemetry_rate_hz_);
  timer_ = this->create_wall_timer(
    std::chrono::duration_cast<std::chrono::nanoseconds>(period),
    std::bind(&TelemIngestNode::publish_telemetry, this));
}

void TelemIngestNode::publish_telemetry()
{
  auto msg = astramind_interfaces::msg::SpacecraftTelemetry();
  msg.header.stamp = this->now();
  msg.header.frame_id = "spacecraft_body";

  mission_elapsed_time_s_ += 1.0 / telemetry_rate_hz_;

  const double orbital_phase = mission_elapsed_time_s_ * 0.001;
  msg.position_x_m = static_cast<float>(7000000.0 * std::cos(orbital_phase));
  msg.position_y_m = static_cast<float>(7000000.0 * std::sin(orbital_phase));
  msg.position_z_m = 0.0f;

  msg.velocity_x_mps = static_cast<float>(-7500.0 * std::sin(orbital_phase));
  msg.velocity_y_mps = static_cast<float>(7500.0 * std::cos(orbital_phase));
  msg.velocity_z_mps = 0.0f;

  msg.battery_voltage_v = 28.0f;
  msg.battery_current_a = 1.5f;
  msg.battery_soc_pct = static_cast<float>(90.0 - 0.001 * mission_elapsed_time_s_);

  msg.temperature_c = static_cast<float>(20.0 + 5.0 * std::sin(mission_elapsed_time_s_ * 0.01));

  msg.mission_elapsed_time_s = mission_elapsed_time_s_;

  publisher_->publish(msg);
}

}  // namespace astramind_core
