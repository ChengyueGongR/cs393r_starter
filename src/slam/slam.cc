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
\file    slam.cc
\brief   SLAM Starter Code
\author  Joydeep Biswas, (C) 2019
*/
//========================================================================

#include <algorithm>
#include <cmath>
#include <iostream>
#include "eigen3/Eigen/Dense"
#include "eigen3/Eigen/Geometry"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "shared/math/geometry.h"
#include "shared/math/math_util.h"
#include "shared/util/timer.h"

#include "slam.h"

#include "vector_map/vector_map.h"

using namespace math_util;
using Eigen::Affine2f;
using Eigen::Rotation2Df;
using Eigen::Translation2f;
using Eigen::Vector2f;
using Eigen::Vector2i;
using std::cout;
using std::endl;
using std::string;
using std::swap;
using std::vector;
using vector_map::VectorMap;

namespace slam {

SLAM::SLAM() 
  : prev_odom_loc_( 0, 0 ),
    prev_odom_angle_( 0 ),
    odom_initialized_( false ) 
  {}


void SLAM::GetPose( Eigen::Vector2f* loc, float* angle ) const 
{
  if( !loc || !angle )
  {
    std::cout<<" SLAM::GetPose() was passed a nullptr! What the hell man...\n";
    return;
  }

  // Return the latest pose estimate of the robot.
  *loc = state_loc_;
  *angle = state_angle_;
}


void SLAM::ObserveLaser( const vector<float>& ranges,
                         float range_min,
                         float range_max,
                         float angle_min,
                         float angle_max ) 
{
  // A new laser scan has been observed. Decide whether to add it as a pose
  // for SLAM. If decided to add, align it to the scan from the last saved pose,
  // and save both the scan and the optimized pose.
  if( !map_initialized_ )
  {
    map_pose_scan_.clear();

    // Note that the first cloud does not need to be transformed to world frame becasue it is the world frame!
    PoseScan origin{ state_loc_, 
                     state_angle_, 
                     ScanToPointCloud( ranges, angle_min, angle_max ) };

    map_pose_scan_.push_back( origin );

    map_initialized_ = true;
    return;
  }

  if( (map_pose_scan_.back().state_loc - state_loc_).norm() > min_trans_ ||
      fabs(map_pose_scan_.back().state_angle - state_angle_) > min_rot_ )
  {
    PoseScan node;
    node.state_loc = state_loc_; 
    node.state_angle = state_angle_;
    node.point_cloud = ScanToPointCloud( ranges, angle_min, angle_max ) ; 

    map_pose_scan_.push_back( node );

    return;
  }
}


void SLAM::ObserveOdometry( const Vector2f& odom_loc, const float odom_angle ) 
{
  if ( !odom_initialized_ ) 
  {
    prev_odom_angle_ = odom_angle;
    prev_odom_loc_ = odom_loc;
    odom_initialized_ = true;

    return;
  }
  // Keep track of odometry to estimate how far the robot has moved between 
  // poses.
  
  const Eigen::Rotation2D<float> base_link_rot( -prev_odom_angle_ );        // THIS HAS TO BE NEGATIVE :)
  Vector2f delta_T_bl = base_link_rot*( odom_loc - prev_odom_loc_ );  // delta_T_base_link: pres 6 slide 14
  double const delta_angle_bl = odom_angle - prev_odom_angle_;        // delta_angle_base_link: pres 6 slide 15
  
  Eigen::Rotation2D<float> map_rot( state_angle_ );
  state_loc_ += map_rot*delta_T_bl;
  state_angle_ += delta_angle_bl;

  prev_odom_loc_ = odom_loc;
  prev_odom_angle_ = odom_angle;

  return;
}


vector<Vector2f> SLAM::GetMap() 
{
  vector<Vector2f> map;
  // Reconstruct the map as a single aligned point cloud from all saved poses
  // and their respective scans.
  return map;
}


vector<Vector2f> ScanToPointCloud( const vector<float>& ranges,
                                   const float angle_min,
                                   const float angle_max )
{
  // Adopted from Adrian Abeyta's work in "navigation.cc"

  Vector2f const kLaserLoc( 0.2, 0 );

  vector<Vector2f> point_cloud;
  point_cloud.reserve( ranges.size() );

  float const angle_increment = ( angle_max - angle_min )/ranges.size();

  for( size_t i=0; i<ranges.size(); ++i )
  {
    double const theta = angle_min + angle_increment*i; 
    
    // Rotation
    Vector2f point( cos(theta)*ranges[i], sin(theta)*ranges[i] ); 
    // Translation
    point += kLaserLoc; 

    point_cloud.push_back( point );
  }
  
  return point_cloud;
}

}  // namespace slam
