
#include "psm_orin_ros.hpp"
#include "psm_orin_driver.h"

PSMOrinNode::PSMOrinNode() : Node("psm_orin_node") {
    set_subscribers_and_publishers();
    i2c_psm_init();

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
    if (read_measurements(&voltage, &current)) {
        return;
    }

    publish_current();
    publish_voltage();

    auto logger = spdlog::get("file_logger");
    if (logger) {
        auto now = this->now();
        logger->info("{},{},{}", now.seconds(), voltage, current);
    }
}

int main(int argc, char** argv) {
    auto file_logger = spdlog::basic_logger_mt("file_logger", "psm_data.csv");
    spdlog::set_pattern("%v");

    rclcpp::init(argc, argv);
    spdlog::info("Hello ros world!");
    rclcpp::spin(std::make_shared<PSMOrinNode>());
    rclcpp::shutdown();
    return 0;
}
