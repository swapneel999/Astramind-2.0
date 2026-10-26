# AstraMind 2.0

An edge-optimized, memory-safe ROS2 autonomous orbital operations system in C++. AstraMind 2.0 ingests mock spacecraft telemetry, runs onboard anomaly-detection inference against a TorchScript model, and drives a lightweight reasoning/command state machine — all as three ROS2 Humble nodes with non-blocking inference and a containerized runtime.

## Architecture

Three nodes, connected by three custom messages:

```
telem_ingest_node  --SpacecraftTelemetry-->  astro_guardian_node  --AnomalyAlert-->  astramind_core_node  --CommandExecution-->
   (10Hz mock publisher,                     (LibTorch inference on a                (severity -> command
    SensorData QoS)                           dedicated worker thread,                 reasoning)
                                               non-blocking on the executor)
```

| Package | Contents |
|---|---|
| `astramind_interfaces` | `SpacecraftTelemetry.msg`, `AnomalyAlert.msg`, `CommandExecution.msg` (float32 where precision allows) |
| `astramind_core` | The three nodes, gtest unit tests, the bringup launch file, and the TorchScript model |

`astro_guardian_node` loads a TorchScript model (`astramind_core/models/astramind_model.pt`, produced by `generate_model.py`) and runs inference on a dedicated background thread — the ROS2 executor callback only ever hands off the latest telemetry reading and returns immediately, so inference never blocks the executor.

## Requirements

- Ubuntu 22.04 LTS (or WSL2 with an Ubuntu-22.04 distro)
- ROS2 Humble
- LibTorch 2.7.1 (CPU, cxx11 ABI) at `/opt/libtorch`
- CMake / colcon
- Docker (for the containerized build)

## Build

```bash
source /opt/ros/humble/setup.bash
cd astramind_ws
colcon build --symlink-install
```

## Test

```bash
colcon test && colcon test-result --verbose
```

## Run

```bash
source install/setup.bash
ros2 launch astramind_core astramind_bringup.launch.py
```

Configurable launch parameters:

| Parameter | Default | Description |
|---|---|---|
| `inference_threshold` | `0.8` | Anomaly score (0-1) above which `astro_guardian_node` publishes an `AnomalyAlert` |
| `telemetry_rate_hz` | `10.0` | Rate at which `telem_ingest_node` publishes mock telemetry |

Example:

```bash
ros2 launch astramind_core astramind_bringup.launch.py inference_threshold:=0.3 telemetry_rate_hz:=5.0
```

## Docker

Multi-stage build (`ros:humble-ros-base` base): stage 1 builds the workspace and downloads LibTorch; stage 2 is a minimal runtime image carrying only the compiled binaries, LibTorch's shared libraries, and the model file.

```bash
docker build -t astramind_core:latest .
docker run --rm astramind_core:latest
```

## Coverage

Line coverage is measured via a separate `gcov`/`lcov`-instrumented build (does not affect normal builds, CI, or the Docker image):

```bash
colcon build --build-base build-coverage --install-base install-coverage \
  --packages-select astramind_core astramind_interfaces \
  --cmake-args -DASTRAMIND_ENABLE_COVERAGE=ON
colcon test --build-base build-coverage --install-base install-coverage --packages-select astramind_core
lcov --capture --directory build-coverage/astramind_core --output-file /tmp/coverage.info
lcov --extract /tmp/coverage.info '*/astramind_core/src/*' -o /tmp/coverage_filtered.info
lcov --list /tmp/coverage_filtered.info
```

## Status

| Metric | Target | Measured |
|---|---|---|
| Inference latency | < 50ms/frame | Passing (gtest) |
| Docker runtime memory | < 250MB | ~206MB |
| Core node line coverage | > 85% | 100% (120/120 lines) |
| `colcon test` | all suites pass | 81 tests, 0 failures (CI green) |

## CI

`.github/workflows/ci.yml` runs `colcon build` and `colcon test` inside a `ros:humble-ros-base` container on every push and pull request, with LibTorch cached between runs.
