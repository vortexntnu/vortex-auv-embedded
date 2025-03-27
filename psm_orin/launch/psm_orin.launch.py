from os import path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:

    thruster_interface_auv_node = Node(
        package="psm_orin",
        executable="psm_orin_node",
        name="psm_orin_node",
        namespace="orca",
        output="screen",
    )

    return LaunchDescription([thruster_interface_auv_node])
