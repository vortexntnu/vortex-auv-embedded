#ifndef PSM_ORIN_NODE_HPP
#define PSM_ORIN_NODE_HPP

#include <chrono>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float64.hpp>

class PSMOrinNode : public rclcpp::Node {
   public:
    explicit PSMOrinNode();

   private:
    void set_subscribers_and_publishers();
    void publish_voltage();
    void publish_current();
    void read_ads_callback();

    rclcpp::TimerBase::SharedPtr watchdog_timer_;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr voltage_pub_;
    rclcpp::Publisher<std_msgs::msg::Float64>::SharedPtr current_pub_;
    rclcpp::Time last_msg_time_;

    double voltage = 0.0;
    double current = 0.0;
};

#endif  // PSM_ORIN_NODE_HPP
