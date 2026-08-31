#!/bin/bash
set -e
source /opt/ros/humble/setup.bash
source /opt/astramind_ws/install/setup.bash
exec "$@"
