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

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include <rclcpp/rclcpp.hpp>

#include "astramind_core/astramind_core_node.hpp"

TEST(AstramindCoreNodeTest, InitializesWithExpectedName)
{
  auto node = std::make_shared<astramind_core::AstramindCoreNode>();
  EXPECT_STREQ(node->get_name(), "astramind_core_node");
}

TEST(AstramindCoreNodeTest, IssuesSafeModeCommandForHighSeverityAlert)
{
  auto node = std::make_shared<astramind_core::AstramindCoreNode>();
  auto publisher_node = std::make_shared<rclcpp::Node>("alert_publisher_test");
  auto pub = publisher_node->create_publisher<astramind_interfaces::msg::AnomalyAlert>(
    "spacecraft/anomaly_alert", rclcpp::QoS(10));

  auto listener = std::make_shared<rclcpp::Node>("command_listener_test");
  astramind_interfaces::msg::CommandExecution received;
  bool got_command = false;
  auto sub = listener->create_subscription<astramind_interfaces::msg::CommandExecution>(
    "spacecraft/command_execution", rclcpp::QoS(10),
    [&](const astramind_interfaces::msg::CommandExecution::SharedPtr msg) {
      received = *msg;
      got_command = true;
    });

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  executor.add_node(publisher_node);
  executor.add_node(listener);

  auto alert = astramind_interfaces::msg::AnomalyAlert();
  alert.anomaly_type = "thermal_threshold_exceeded";
  alert.severity = astramind_interfaces::msg::AnomalyAlert::SEVERITY_HIGH;

  auto start = std::chrono::steady_clock::now();
  while (!got_command && std::chrono::steady_clock::now() - start < std::chrono::seconds(3)) {
    pub->publish(alert);
    executor.spin_some(std::chrono::milliseconds(50));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  ASSERT_TRUE(got_command);
  EXPECT_EQ(received.command_type, "ENTER_SAFE_MODE");
  EXPECT_EQ(received.status, astramind_interfaces::msg::CommandExecution::STATUS_SUCCEEDED);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int result = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return result;
}
