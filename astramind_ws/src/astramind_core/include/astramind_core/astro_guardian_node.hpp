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

#ifndef ASTRAMIND_CORE__ASTRO_GUARDIAN_NODE_HPP_
#define ASTRAMIND_CORE__ASTRO_GUARDIAN_NODE_HPP_

#include <torch/script.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include <astramind_interfaces/msg/spacecraft_telemetry.hpp>
#include <astramind_interfaces/msg/anomaly_alert.hpp>

namespace astramind_core
{

class AstroGuardianNode : public rclcpp::Node
{
public:
  explicit AstroGuardianNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~AstroGuardianNode() override;

  // Exposed so unit tests can exercise the scoring logic directly, without pub/sub.
  // Runs the TorchScript model synchronously on the calling thread; production code only
  // calls this from the dedicated inference worker thread, never from a ROS2 callback.
  double compute_anomaly_score(const astramind_interfaces::msg::SpacecraftTelemetry & msg);

private:
  // Non-blocking: just stashes the latest telemetry reading and wakes the worker thread.
  void telemetry_callback(const astramind_interfaces::msg::SpacecraftTelemetry::SharedPtr msg);

  // Runs on a dedicated thread so model inference never blocks the ROS2 executor.
  void inference_worker_loop();

  rclcpp::Subscription<astramind_interfaces::msg::SpacecraftTelemetry>::SharedPtr subscription_;
  rclcpp::Publisher<astramind_interfaces::msg::AnomalyAlert>::SharedPtr publisher_;
  double inference_threshold_;

  torch::jit::script::Module model_;

  std::thread worker_thread_;
  std::mutex mutex_;
  std::condition_variable cv_;
  astramind_interfaces::msg::SpacecraftTelemetry::SharedPtr pending_telemetry_;
  bool running_;
};

}  // namespace astramind_core

#endif  // ASTRAMIND_CORE__ASTRO_GUARDIAN_NODE_HPP_
