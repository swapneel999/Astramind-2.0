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

#ifndef ASTRAMIND_CORE__TELEM_INGEST_NODE_HPP_
#define ASTRAMIND_CORE__TELEM_INGEST_NODE_HPP_

#include <rclcpp/rclcpp.hpp>
#include <astramind_interfaces/msg/spacecraft_telemetry.hpp>

namespace astramind_core
{

class TelemIngestNode : public rclcpp::Node
{
public:
  explicit TelemIngestNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  void publish_telemetry();

  rclcpp::Publisher<astramind_interfaces::msg::SpacecraftTelemetry>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  double telemetry_rate_hz_;
  double mission_elapsed_time_s_;
};

}  // namespace astramind_core

#endif  // ASTRAMIND_CORE__TELEM_INGEST_NODE_HPP_
