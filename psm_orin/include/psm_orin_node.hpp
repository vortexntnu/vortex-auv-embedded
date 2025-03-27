#ifndef PSM_ORIN_NODE_HPP
#define PSM_ORIN_NODE_HPP

#endif  // !PSM_ORIN_NODE_HPP

#include <chrono>
#include <rclcpp/rclcpp.hpp>
#include <vortex_msgs/msg/thruster_forces.hpp>
class PSMOrinNode : public rclcpp::Node {
   public:
    explicit PSMOrinNode();

   private:
    void set_subscribers_and_publishers();

    void publish_voltage();
    void publish_current();

    void read_ads_callback(
        const vortex_msgs::msg::ThrusterForces::SharedPtr msg);

    rclcpp::TimerBase::SharedPtr watchdog_timer_;
    rclcpp::Time last_msg_time_;

    double voltage = 0;
    double current = 0;
};
