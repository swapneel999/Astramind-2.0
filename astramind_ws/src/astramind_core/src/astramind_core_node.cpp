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

#include "astramind_core/astramind_core_node.hpp"

namespace astramind_core
{

AstramindCoreNode::AstramindCoreNode(const rclcpp::NodeOptions & options)
: Node("astramind_core_node", options), command_counter_(0)
{
  subscription_ = this->create_subscription<astramind_interfaces::msg::AnomalyAlert>(
    "spacecraft/anomaly_alert", rclcpp::QoS(10),
    std::bind(&AstramindCoreNode::anomaly_callback, this, std::placeholders::_1));

  publisher_ = this->create_publisher<astramind_interfaces::msg::CommandExecution>(
    "spacecraft/command_execution", rclcpp::QoS(10));
}

void AstramindCoreNode::anomaly_callback(
  const astramind_interfaces::msg::AnomalyAlert::SharedPtr msg)
{
  auto cmd = astramind_interfaces::msg::CommandExecution();
  cmd.header.stamp = this->now();
  cmd.header.frame_id = "spacecraft_body";
  cmd.command_id = "cmd_" + std::to_string(command_counter_++);

  bool severe = msg->severity >= astramind_interfaces::msg::AnomalyAlert::SEVERITY_HIGH;
  cmd.command_type = severe ? "ENTER_SAFE_MODE" : "LOG_ADVISORY";
  cmd.status = astramind_interfaces::msg::CommandExecution::STATUS_SUCCEEDED;
  cmd.details = "Response to anomaly: " + msg->anomaly_type;

  publisher_->publish(cmd);
}

}  // namespace astramind_core
