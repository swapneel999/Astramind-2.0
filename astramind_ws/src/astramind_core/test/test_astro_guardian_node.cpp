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

#include "astramind_core/astro_guardian_node.hpp"

TEST(AstroGuardianNodeTest, InitializesWithExpectedName)
{
  auto node = std::make_shared<astramind_core::AstroGuardianNode>();
  EXPECT_STREQ(node->get_name(), "astro_guardian_node");
}

TEST(AstroGuardianNodeTest, InferenceScoreIsWithinUnitInterval)
{
  auto node = std::make_shared<astramind_core::AstroGuardianNode>();
  astramind_interfaces::msg::SpacecraftTelemetry telemetry;
  telemetry.temperature_c = 20.0f;
  telemetry.battery_soc_pct = 90.0f;

  double score = node->compute_anomaly_score(telemetry);
  EXPECT_GE(score, 0.0);
  EXPECT_LE(score, 1.0);
}

TEST(AstroGuardianNodeTest, InferenceScoreIsDeterministicForSameInput)
{
  auto node = std::make_shared<astramind_core::AstroGuardianNode>();
  astramind_interfaces::msg::SpacecraftTelemetry telemetry;
  telemetry.temperature_c = 42.0f;
  telemetry.battery_soc_pct = 55.0f;
  telemetry.velocity_x_mps = 100.0f;

  double first = node->compute_anomaly_score(telemetry);
  double second = node->compute_anomaly_score(telemetry);
  EXPECT_DOUBLE_EQ(first, second);
}

TEST(AstroGuardianNodeTest, InferenceCompletesUnderFiftyMilliseconds)
{
  auto node = std::make_shared<astramind_core::AstroGuardianNode>();
  astramind_interfaces::msg::SpacecraftTelemetry telemetry;
  telemetry.temperature_c = 30.0f;

  auto start = std::chrono::steady_clock::now();
  node->compute_anomaly_score(telemetry);
  auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 50);
}

TEST(AstroGuardianNodeTest, PublishesAnomalyAlertOnThresholdBreach)
{
  // Threshold set below zero so any real sigmoid-bounded inference score triggers an alert,
  // decoupling this plumbing test from the (untrained) model's actual output value.
  rclcpp::NodeOptions options;
  options.append_parameter_override("inference_threshold", -1.0);
  auto node = std::make_shared<astramind_core::AstroGuardianNode>(options);
  auto publisher_node = std::make_shared<rclcpp::Node>("telemetry_publisher_test");
  auto pub = publisher_node->create_publisher<astramind_interfaces::msg::SpacecraftTelemetry>(
    "spacecraft/telemetry", rclcpp::SensorDataQoS());

  auto listener = std::make_shared<rclcpp::Node>("alert_listener_test");
  bool got_alert = false;
  auto sub = listener->create_subscription<astramind_interfaces::msg::AnomalyAlert>(
    "spacecraft/anomaly_alert", rclcpp::QoS(10),
    [&got_alert](const astramind_interfaces::msg::AnomalyAlert::SharedPtr) {
      got_alert = true;
    });

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  executor.add_node(publisher_node);
  executor.add_node(listener);

  auto msg = astramind_interfaces::msg::SpacecraftTelemetry();
  msg.temperature_c = 95.0f;

  auto start = std::chrono::steady_clock::now();
  while (!got_alert && std::chrono::steady_clock::now() - start < std::chrono::seconds(3)) {
    pub->publish(msg);
    executor.spin_some(std::chrono::milliseconds(50));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  EXPECT_TRUE(got_alert);
}

TEST(AstroGuardianNodeTest, DoesNotPublishAlertForNominalTelemetry)
{
  // Threshold set above one so no real sigmoid-bounded inference score can ever breach it.
  rclcpp::NodeOptions options;
  options.append_parameter_override("inference_threshold", 2.0);
  auto node = std::make_shared<astramind_core::AstroGuardianNode>(options);
  auto publisher_node = std::make_shared<rclcpp::Node>("telemetry_publisher_test_nominal");
  auto pub = publisher_node->create_publisher<astramind_interfaces::msg::SpacecraftTelemetry>(
    "spacecraft/telemetry", rclcpp::SensorDataQoS());

  auto listener = std::make_shared<rclcpp::Node>("alert_listener_test_nominal");
  bool got_alert = false;
  auto sub = listener->create_subscription<astramind_interfaces::msg::AnomalyAlert>(
    "spacecraft/anomaly_alert", rclcpp::QoS(10),
    [&got_alert](const astramind_interfaces::msg::AnomalyAlert::SharedPtr) {
      got_alert = true;
    });

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  executor.add_node(publisher_node);
  executor.add_node(listener);

  auto msg = astramind_interfaces::msg::SpacecraftTelemetry();
  msg.temperature_c = 20.0f;

  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500)) {
    pub->publish(msg);
    executor.spin_some(std::chrono::milliseconds(50));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  EXPECT_FALSE(got_alert);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int result = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return result;
}
