#ifndef PSM_ORIN_NODE_HPP
#define PSM_ORIN_NODE_HPP

#endif  // !PSM_ORIN_NODE_HPP

#include <chrono>
#include <rclcpp/rclcpp.hpp>
class PSMOrinNode : public rclcpp::Node {
   public:
    explicit PSMOrinNode();

   private:
    void set_subscribers_and_publishers();

    void publish_voltage();
    void publish_current();
};
