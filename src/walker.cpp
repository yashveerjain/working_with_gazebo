// Copyright [2022] Yashveer Jain
#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/logger.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

/**
 * @brief WalkerBrain, class will handle the collision avoidance and robot
 * motion code
 *
 */
class WalkerBrain {
 public:
  /**
   * @brief Construct a new Walker Brain object
   *
   */
  WalkerBrain() {}
  /**
   * @brief Construct a new Walker Brain object
   *
   * @param speed
   * @param angle
   */
  WalkerBrain(double speed, double angle) {
    this->speed = speed;
    this->angle = angle;
  }

  void turn(geometry_msgs::msg::Twist& motion) { motion.angular.z = angle; }
  void move_forward(geometry_msgs::msg::Twist& motion) {
    motion.linear.x = speed;
  }
  void check_obstacle(const sensor_msgs::msg::LaserScan scan) {
    RCLCPP_INFO_STREAM(logger, "obstacle checking -----");
    for (int i = 0; i < front_view[0]; i++) {
      if (scan.ranges[i] <= dist_thresh) {
        this->obstacle_in_front_ = true;
        RCLCPP_INFO_STREAM(logger, "obstacle detected within 1 m");
        return;
      }
    }
    for (int i = 358; i > front_view[1]; i--) {
      if (scan.ranges[i] <= dist_thresh) {
        this->obstacle_in_front_ = true;
        RCLCPP_INFO_STREAM(logger, "obstacle detected within 1 m");
        return;
      }
    }
    this->obstacle_in_front_ = false;
  }
  void move(geometry_msgs::msg::Twist& motion) {
    RCLCPP_INFO_STREAM(
        logger, "motion after seeing obstacle " << this->obstacle_in_front_);
    if (this->obstacle_in_front_) {
      turn(motion);
      RCLCPP_INFO_STREAM(
          logger, "publisher callback with rotate motion " << motion.angular.z);
    } else {
      move_forward(motion);
      RCLCPP_INFO_STREAM(
          logger, "publisher callback with linear motion " << motion.linear.x);
    }
  }
  rclcpp::Logger logger = rclcpp::get_logger("walker_brain");

 private:
  double angle = .1;       // rad/sec
  double speed = 0.1;      // m/sec
  double dist_thresh = 1;  // 1m is Object distance threshold
  // between -20* to 20*
  int front_view[2] = {
      20,
      340};  // Note: this is array index of range according to laser scan msg
  bool obstacle_in_front_;

  /*
  laser scan msg :
  angle_min: 0.0
  angle_max: 6.28000020980835
  angle_increment: 0.01749303564429283
  */
};

/**
 * @brief Walker is ros2 node which will publish the command velocity using
 * WalkerBrain
 * @details Walker node is also subscriber to laser_scan topic to get laser data
 * and use it for collision avoidance.
 */
class Walker : public rclcpp::Node {
 public:
  /**
   * @brief Construct a new Walker object
   *
   */
  Walker() : Node("walker") {
    RCLCPP_INFO_STREAM(this->get_logger(), "Started Walker Node");
    publisher_ = this->create_publisher<geometry_msgs::msg::Twist>(
        "/walker/cmd_vel", 10);

    timer_ = this->create_wall_timer(
        std::chrono::milliseconds(static_cast<int>((10))),
        std::bind(&Walker::walk, this));
  }

 private:
  /**
   * @brief callback function used for publishing the velocity to
   * /walker/cmd_vel topic after making decision using WalkerBrain class
   *
   */
  void walk() {
    subscription_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
        "/scan", 10,
        std::bind(&Walker::check_obstacle, this, std::placeholders::_1));
    geometry_msgs::msg::Twist motion = geometry_msgs::msg::Twist();
    brain.move(motion);
    publisher_->publish(motion);
  }
  /**
   * @brief callback function subscribed to scan topic and use laser scan data
   * for collision avoidance
   *
   * @param scan
   */
  void check_obstacle(const sensor_msgs::msg::LaserScan& scan) {
    brain.check_obstacle(scan);
    RCLCPP_INFO_STREAM(this->get_logger(), "subcribed /scan topic");
  }
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr subscription_;
  WalkerBrain brain;
};

int main(int argc, char* argv[]) {
  rclcpp::init(argc, argv);
  auto node = std::make_shared<Walker>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
