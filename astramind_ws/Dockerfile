# syntax=docker/dockerfile:1

# ---- Stage 1: builder -------------------------------------------------
FROM ros:humble-ros-base AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    curl \
    unzip \
    python3-colcon-common-extensions \
    python3-rosdep \
    && rm -rf /var/lib/apt/lists/*

# LibTorch (CPU, cxx11 ABI) — matches the local dev environment so binaries
# built here can run unmodified against the copy installed in the runtime stage.
RUN curl -sSL -o /tmp/libtorch.zip \
    "https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.7.1%2Bcpu.zip" \
    && unzip -q /tmp/libtorch.zip -d /opt/ \
    && rm /tmp/libtorch.zip

WORKDIR /workspace
COPY src/ src/

RUN . /opt/ros/humble/setup.sh \
    && (rosdep init || true) \
    && rosdep update \
    && rosdep install --from-paths src --ignore-src -r -y

RUN . /opt/ros/humble/setup.sh \
    && colcon build --merge-install --cmake-args -DCMAKE_BUILD_TYPE=Release

# ---- Stage 2: runtime ---------------------------------------------------
FROM ros:humble-ros-base AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Only the shared libraries are needed at runtime, not headers/CMake configs/
# static archives — this keeps the runtime image well short of the full
# LibTorch distribution size.
COPY --from=builder /opt/libtorch/lib /opt/libtorch/lib

COPY --from=builder /workspace/install /opt/astramind_ws/install

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["ros2", "launch", "astramind_core", "astramind_bringup.launch.py"]
