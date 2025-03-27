
#include "psm_orin_ros.hpp"
#include "psm_orin_driver.h"

PSMOrinNode::PSMOrinNode() : Node("psm_orin_node") {
    set_subscribers_and_publishers();
    i2c_psm_init();
    config_ads();

    watchdog_timer_ = this->create_wall_timer(
        std::chrono::milliseconds(500),
        std::bind(&PSMOrinNode::read_ads_callback, this));

    last_msg_time_ = this->now();
}

void PSMOrinNode::set_subscribers_and_publishers() {
    voltage_pub_ =
        this->create_publisher<std_msgs::msg::Float64>("voltage", 10);
    current_pub_ =
        this->create_publisher<std_msgs::msg::Float64>("current", 10);
}

void PSMOrinNode::publish_voltage() {
    auto voltage_msg = std_msgs::msg::Float64();
    voltage_msg.data = voltage;
    voltage_pub_->publish(voltage_msg);
}

void PSMOrinNode::publish_current() {
    auto current_msg = std_msgs::msg::Float64();
    current_msg.data = current;
    current_pub_->publish(current_msg);
}

void PSMOrinNode::read_ads_callback() {
    read_scaled_measurements(&voltage, &current);

    publish_current();
    publish_voltage();
}
