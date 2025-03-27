#include "psm_orin_node.hpp"
#include "psm_orin_driver.h"

PSMOrinNode::PSMOrinNode() {
    set_subscribers_and_publishers();
    i2c_psm_init();

    watchdog_timer_ = this->create_wall_timer(
        std::chrono::milliseconds(500),
        std::bind(&PSMOrinNode::read_ads_callback, this));
    last_msg_time_ = this->now();
}

void PSMOrinNode::publish_voltage() {}

void PSMOrinNode::publish_current() {}

void PSMOrinNode::read_ads_callback(
    const vortex_msgs::msg::ThrusterForces::SharedPtr msg) {
    read_scaled_measurements(&voltage, &current);
    
}
