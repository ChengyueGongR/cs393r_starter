//========================================================================
//  This software is free: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License Version 3,
//  as published by the Free Software Foundation.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  Version 3 in the file COPYING that came with this distribution.
//  If not, see <http://www.gnu.org/licenses/>.
//========================================================================
/*!
\file    navigation.h
\brief   Interface for reference Navigation class.
\author  Joydeep Biswas, (C) 2019
*/
//========================================================================

#include <vector>
#include <list> 
  
#include "eigen3/Eigen/Dense"

#include "amrl_msgs/AckermannCurvatureDriveMsg.h"

#ifndef NAVIGATION_H
#define NAVIGATION_H

namespace ros {
  class NodeHandle;
}  // namespace ros

namespace navigation {

struct PathOption {
  float curvature;
  float clearance;
  float free_path_length;
  Eigen::Vector2f obstruction;
  Eigen::Vector2f closest_point;
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW;
};
struct AccelerationCommand {
  float acceleration;
  ros::Time stamp;
};

class Navigation {
 public:

   // Constructor
  explicit Navigation(const std::string& map_file, ros::NodeHandle* n);

  // Used in callback from localization to update position.
  void UpdateLocation(const Eigen::Vector2f& loc, float angle);

  // Used in callback for odometry messages to update based on odometry.
  void UpdateOdometry(const Eigen::Vector2f& loc,
                      float angle,
                      const Eigen::Vector2f& vel,
                      float ang_vel);

  // Updates based on an observed laser scan
  void ObservePointCloud(const std::vector<Eigen::Vector2f>& cloud,
                         double time);

  // Main function called continously from main
  void Run();
  // Used to set the next target pose.
  void SetNavGoal(const Eigen::Vector2f& loc, float angle);

  ////HELMS DEEP ADDITIONS////
  // Predict the velocity the car will have after the lagged commands are executed
  double PredictedRobotVelocity();

 private:

  // Current robot location.
  Eigen::Vector2f robot_loc_;
  // Current robot orientation.
  float robot_angle_;
  // Current robot velocity.
  Eigen::Vector2f robot_vel_;
  // Current robot angular speed.
  float robot_omega_;
  // Odometry-reported robot location.
  Eigen::Vector2f odom_loc_;
  // Odometry-reported robot angle.
  float odom_angle_;

  // Whether navigation is complete.
  bool nav_complete_;
  // Navigation goal location.
  Eigen::Vector2f nav_goal_loc_;
  // Navigation goal angle.
  float nav_goal_angle_;

  ////HELMS DEEP ADDITIONS////
  // Navigation goal location tolerance
  const float nav_goal_loc_tol_ = 0.05; //TODO- make configurable parameter
  // Navigation goal angle tolerance
  const float nav_goal_angle_tol_ = 0.1; //TODO- make configurable parameter
  // Max velocity
  const float max_velocity_ = 1.0; // m/s
  // Max acceleration
  const float max_acceleration_ = 4.0; // m/s2
  // Max deceleration
  const float min_acceleration_ = -4.0; // m/s2
  // Timestamp
  const float time_step_ = 1.0/20; // s
  // Odom calibration offset 
  const float odom_offset_ = 0.0;  // m TODO-find how to remove this
  // Command history
  std::list<AccelerationCommand> command_history_;
  // Controller+actuation lag time
  const ros::Duration actuation_lag_time_ = ros::Duration(0.15); // s
  
};

}  // namespace navigation

#endif  // NAVIGATION_H
