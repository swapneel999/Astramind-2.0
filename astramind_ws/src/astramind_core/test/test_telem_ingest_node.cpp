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

#include "astramind_core/telem_ingest_node.hpp"

TEST(TelemIngestNodeTest, InitializesWithExpectedName)
{
  auto node = std::make_shared<astramind_core::TelemIngestNode>();
  EXPECT_STREQ(node->get_name(), "telem_ingest_node");
}

TEST(TelemIngestNodeTest, DefaultsToTenHertzRate)
{
  auto node = std::make_shared<astramind_core::TelemIngestNode>();
  double rate = 0.0;
  node->get_parameter("telemetry_rate_hz", rate);
  EXPECT_DOUBLE_EQ(rate, 10.0);
}

TEST(TelemIngestNodeTest, PublishesTelemetryMessages)
{
  auto node = std::make_shared<astramind_core::TelemIngestNode>();
  auto listener = std::make_shared<rclcpp::Node>("telemetry_test_listener");

  bool got_message = false;
  auto sub = listener->create_subscription<astramind_interfaces::msg::SpacecraftTelemetry>(
    "spacecraft/telemetry", rclcpp::SensorDataQoS(),
    [&got_message](const astramind_interfaces::msg::SpacecraftTelemetry::SharedPtr) {
      got_message = true;
    });

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node);
  executor.add_node(listener);

  auto start = std::chrono::steady_clock::now();
  while (!got_message && std::chrono::steady_clock::now() - start < std::chrono::seconds(3)) {
    executor.spin_some(std::chrono::milliseconds(50));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  EXPECT_TRUE(got_message);
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  rclcpp::init(argc, argv);
  int result = RUN_ALL_TESTS();
  rclcpp::shutdown();
  return result;
}
