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

#include "astramind_core/astro_guardian_node.hpp"

#include <memory>
#include <stdexcept>
#include <vector>

#include <ament_index_cpp/get_package_share_directory.hpp>

namespace astramind_core
{

namespace
{
// Feature order the model was traced against: position (x,y,z), velocity (x,y,z),
// battery (voltage, current, soc), temperature. Must stay in sync with generate_model.py.
constexpr int kFeatureCount = 10;

std::string default_model_path()
{
  return ament_index_cpp::get_package_share_directory("astramind_core") +
         "/models/astramind_model.pt";
}
}  // namespace

AstroGuardianNode::AstroGuardianNode(const rclcpp::NodeOptions & options)
: Node("astro_guardian_node", options), running_(true)
{
  inference_threshold_ = this->declare_parameter<double>("inference_threshold", 0.8);
  std::string model_path =
    this->declare_parameter<std::string>("model_path", default_model_path());

  try {
    model_ = torch::jit::load(model_path);
    model_.eval();
  } catch (const c10::Error & e) {
    throw std::runtime_error(
            "Failed to load TorchScript model from '" + model_path + "': " + e.what());
  }

  subscription_ = this->create_subscription<astramind_interfaces::msg::SpacecraftTelemetry>(
    "spacecraft/telemetry", rclcpp::SensorDataQoS(),
    std::bind(&AstroGuardianNode::telemetry_callback, this, std::placeholders::_1));

  publisher_ = this->create_publisher<astramind_interfaces::msg::AnomalyAlert>(
    "spacecraft/anomaly_alert", rclcpp::QoS(10));

  worker_thread_ = std::thread(&AstroGuardianNode::inference_worker_loop, this);
}

AstroGuardianNode::~AstroGuardianNode()
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
  }
  cv_.notify_all();
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
}

double AstroGuardianNode::compute_anomaly_score(
  const astramind_interfaces::msg::SpacecraftTelemetry & msg)
{
  std::vector<float> features(kFeatureCount);
  features[0] = msg.position_x_m;
  features[1] = msg.position_y_m;
  features[2] = msg.position_z_m;
  features[3] = msg.velocity_x_mps;
  features[4] = msg.velocity_y_mps;
  features[5] = msg.velocity_z_mps;
  features[6] = msg.battery_voltage_v;
  features[7] = msg.battery_current_a;
  features[8] = msg.battery_soc_pct;
  features[9] = msg.temperature_c;

  torch::Tensor input = torch::from_blob(
    features.data(), {1, kFeatureCount}, torch::kFloat32).clone();

  torch::NoGradGuard no_grad;
  torch::Tensor output = model_.forward({input}).toTensor();

  return output.item<float>();
}

void AstroGuardianNode::telemetry_callback(
  const astramind_interfaces::msg::SpacecraftTelemetry::SharedPtr msg)
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_telemetry_ = msg;
  }
  cv_.notify_one();
}

void AstroGuardianNode::inference_worker_loop()
{
  while (true) {
    astramind_interfaces::msg::SpacecraftTelemetry::SharedPtr telemetry;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] {return pending_telemetry_ != nullptr || !running_;});
      if (!running_ && pending_telemetry_ == nullptr) {
        return;
      }
      telemetry = pending_telemetry_;
      pending_telemetry_.reset();
    }

    double score = compute_anomaly_score(*telemetry);
    if (score <= inference_threshold_) {
      continue;
    }

    auto alert = astramind_interfaces::msg::AnomalyAlert();
    alert.header.stamp = this->now();
    alert.header.frame_id = "spacecraft_body";
    alert.anomaly_type = "inference_threshold_exceeded";
    alert.confidence = static_cast<float>(score);
    alert.severity = score > 0.95 ?
      astramind_interfaces::msg::AnomalyAlert::SEVERITY_CRITICAL :
      astramind_interfaces::msg::AnomalyAlert::SEVERITY_HIGH;
    alert.description = "LibTorch inference score exceeded configured threshold";

    publisher_->publish(alert);
  }
}

}  // namespace astramind_core
