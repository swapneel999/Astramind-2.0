# Copyright 2026 AstraMind
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    inference_threshold_arg = DeclareLaunchArgument(
        'inference_threshold',
        default_value='0.8',
        description=(
            'Anomaly score (0-1) above which astro_guardian_node publishes an AnomalyAlert.'
        ),
    )
    telemetry_rate_arg = DeclareLaunchArgument(
        'telemetry_rate_hz',
        default_value='10.0',
        description='Rate, in Hz, at which telem_ingest_node publishes mock SpacecraftTelemetry.',
    )

    telem_ingest_node = Node(
        package='astramind_core',
        executable='telem_ingest_node',
        name='telem_ingest_node',
        output='screen',
        parameters=[{
            'telemetry_rate_hz': LaunchConfiguration('telemetry_rate_hz'),
        }],
    )

    astro_guardian_node = Node(
        package='astramind_core',
        executable='astro_guardian_node',
        name='astro_guardian_node',
        output='screen',
        parameters=[{
            'inference_threshold': LaunchConfiguration('inference_threshold'),
        }],
    )

    astramind_core_node = Node(
        package='astramind_core',
        executable='astramind_core_node',
        name='astramind_core_node',
        output='screen',
    )

    return LaunchDescription([
        inference_threshold_arg,
        telemetry_rate_arg,
        telem_ingest_node,
        astro_guardian_node,
        astramind_core_node,
    ])
