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
\file    particle-filter.cc
\brief   Particle Filter Starter Code
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
#include "shared/math/line2d.h"
#include "shared/math/math_util.h"
#include "shared/util/timer.h"


#include "config_reader/config_reader.h"
#include "particle_filter.h"

#include "vector_map/vector_map.h"

using geometry::line2f;
using std::cout;
using std::endl;
using std::string;
using std::swap;
using std::vector;
using Eigen::Vector2f;
using Eigen::Vector2i;
using vector_map::VectorMap;

DEFINE_double(num_particles, 30, "Number of particles");

namespace particle_filter {

config_reader::ConfigReader config_reader_({"config/particle_filter.lua"});

ParticleFilter::ParticleFilter() :
    prev_odom_loc_(0, 0),
    prev_odom_angle_(0),
    odom_initialized_(false)
{
  particles_.resize(FLAGS_num_particles); 
  I_<< I_xx_, 0,     0,
       0,     I_yy_, 0,
       0,     0,     I_aa_;

  Q_<< Q_vxvx_, 0,       0,
       0,       Q_vyvy_, 0,
       0,       0,       Q_vava_;

  R_<< R_xx_, 0,     0,
       0,     R_yy_, 0,
       0,     0,     R_aa_;

}

void ParticleFilter::GetParticles(vector<Particle>* particles) const {
  *particles = particles_;
}

void ParticleFilter::GetPredictedPointCloud(const Vector2f& loc,
                                            const float angle,
                                            int num_ranges,
                                            float range_min,
                                            float range_max,
                                            float angle_min,
                                            float angle_max,
                                            vector<Vector2f>* scan_ptr) {
  if( !scan_ptr )
  {
    std::cout<<"Update() was passed a nullptr! What the hell man...\n";
    return;
  }
  Vector2f const laser_link( 0.2,0 );
  vector<Vector2f>& scan = *scan_ptr;
  scan.resize(num_ranges);

  int const step_size = scan.size()/num_beams_;
  for( size_t i = 0; i <scan.size(); i += step_size ) 
  { 
    float const laser_angle = angle_min + i*(angle_max - angle_min)/num_ranges;
    float const line_x0 = loc[0] + laser_link.x()*cos(angle) + range_min*cos(angle + laser_angle);
    float const line_y0 = loc[1] + range_min*sin(angle + laser_angle);
    float const line_x1 = loc[0] + laser_link.x()*cos(angle) + range_max*cos(angle + laser_angle);
    float const line_y1 = loc[1] + range_max*sin(angle + laser_angle);
    
    // Intersection_final is updating as line shortens if there are intersections
    Vector2f intersection_final( line_x1, line_y1 ); 
    Vector2f intersection_point;

    for( size_t j = 0; j < map_.lines.size(); ++j ) 
    {
      const line2f map_line = map_.lines[j];
      line2f my_line( line_x0, line_y0, intersection_final.x(), intersection_final.y() );
      const bool intersects = map_line.Intersection( my_line, &intersection_point );

      if( intersects ) 
      {
        // Replace intersection_final with closest obstacle point
        intersection_final = intersection_point; 
      } 
    }  
    scan[i] = intersection_final;
  }
  
  return;
}

void ParticleFilter::Update(const vector<float>& ranges,
                            float range_min,
                            float range_max,
                            float angle_min,
                            float angle_max,
                            vector<Particle> *particle_set_ptr) {
  
  if( !particle_set_ptr )
  {
    std::cout<<"Update() was passed a nullptr! What the hell man...\n";
    return;
  }

  vector<Particle>& particle_set = *particle_set_ptr;
  double max_weight = 0.0;
  for(auto& p: particle_set)
  {
    vector<Vector2f> predicted_point_cloud;

    GetPredictedPointCloud(
      p.loc,
      p.angle,
      ranges.size(),
      range_min,
      range_max,
      angle_min,
      angle_max,
      &predicted_point_cloud);

    
    Vector2f const laser_position( p.loc[0] + .2, p.loc[1] ); //TODO dont hardcode laser tf
    
    vector<float> predicted_ranges( ranges.size() );

    for( size_t i = 0; i < ranges.size(); ++i )
    { 
      predicted_ranges[i] = (predicted_point_cloud[i] - laser_position).norm() ;
    }

    p.weight = log( p.weight ) + log( MeasurementLikelihood(ranges, predicted_ranges, gamma_, num_beams_) );  // Now we are in log land

    if( p.weight>max_weight )
    {
      max_weight = p.weight;
    }
  }

  logLikelihoodReweight( max_weight, &particles_ ); // After this executes we are out of log land
  
  return;
}


double ParticleFilter::MeasurementLikelihood( const vector<float>& ranges, const vector<float>& predicted_ranges, const float& gamma, const float& beam_count ){

  float const d_short = 0.5;
  float const d_long = 1.0;
  float const s_min = 0.5;
  float const s_max = 10.0;
  float const sigma_s = 0.1;
  
  int const step_size = ranges.size()/beam_count;
  double p_z_x = 1.0;
  double p_z_x_i;
  
  for(size_t i = 0; i <ranges.size(); i += step_size )
  {
    if( ranges[i] < s_min ||
        ranges[i] > s_max )
    {
      p_z_x_i = 1.0;
    }

    else if(ranges[i]<predicted_ranges[i] - d_short)
    {
      p_z_x_i = exp(-0.5*d_short*d_short/(sigma_s*sigma_s));
    }

    else if(ranges[i]>predicted_ranges[i] + d_short)
    {
      p_z_x_i = exp(-0.5*d_long*d_long/(sigma_s*sigma_s));
    }

    else
    {
      p_z_x_i = exp(-0.5*(ranges[i] - predicted_ranges[i])*(ranges[i] - predicted_ranges[i])/(sigma_s*sigma_s));
    }

    p_z_x *= p_z_x_i;
  }

  return p_z_x;
}


void ParticleFilter::Resample() {
  
  // Create a variable to store the new particles 
  vector <Particle> new_particle_set(FLAGS_num_particles);

  // Find incremental sum of weight vector
  vector <double> weight_sum{0.0};
  for( const auto& particle: particles_)
  {
    weight_sum.push_back(weight_sum.back() + particle.weight);
  }

  assert(weight_sum.back() == 1.0 );
  
  // Pick new particles from old particle set
  for( auto& new_particle: new_particle_set)
  {
    const double pick = rng_.UniformRandom(0,1);
    for(int i=0; i< FLAGS_num_particles; i++)
    {
      if (pick > weight_sum[i] && pick < weight_sum[i+1])
      {
        new_particle = particles_[i];
        new_particle.weight = 1.0/FLAGS_num_particles;
        new_particle.loc.x() += rng_.Gaussian( 0, R_(0,0) ); 
        new_particle.loc.y() += rng_.Gaussian( 0, R_(1,1) );   
        new_particle.angle += rng_.Gaussian( 0, R_(2,2) ); 
      }
    }
  }

  particles_ = move( new_particle_set );

  return; 
}

void ParticleFilter::ObserveLaser(const vector<float>& ranges,
                                  float range_min,
                                  float range_max,
                                  float angle_min,
                                  float angle_max) {
  // A new laser scan observation is available (in the laser frame)
  // Call the Update and Resample steps as necessary.
  
  if( (prev_update_loc_ - prev_odom_loc_).norm() > min_update_dist_) 
  {
    prev_update_loc_ = prev_odom_loc_;
    std::cout << "Updating\n";
    Update( ranges,
            range_min,
            range_max,
            angle_min,
            angle_max,
            &particles_);

    if( isDegenerate() )
    {    
      Resample();
    }
  }
}

void ParticleFilter::ObserveOdometry(const Vector2f& odom_loc,
                                     const float odom_angle) {
  if( !odom_initialized_ )
  {
    prev_odom_loc_ = odom_loc;
    prev_odom_angle_ = odom_angle;
    odom_initialized_ = true;

    return;
  }

  else
  {
    Eigen::Rotation2D<float> base_link_rot( -prev_odom_angle_ );        // THIS HAS TO BE NEGATIVE :)
    Vector2f delta_T_bl = base_link_rot*( odom_loc - prev_odom_loc_ );  // delta_T_base_link: pres 6 slide 14
    double const delta_angle_bl = odom_angle - prev_odom_angle_;        // delta_angle_base_link: pres 6 slide 15
  
    for(auto& particle: particles_)
    {
      Eigen::Rotation2D<float> map_rot( particle.angle );
      particle.loc += map_rot*delta_T_bl;
      particle.angle += delta_angle_bl;
    }
    
    prev_odom_loc_ = odom_loc;
    prev_odom_angle_ = odom_angle;

    return;
  }
}

void ParticleFilter::logLikelihoodReweight(const double &max_weight, vector<Particle> *particle_set )
{
    // Preventing numerical underflow  
    double weight_sum = 0;
    for( auto& particle: *particle_set )
    {
      // particle-weight = log(p(z|x)) + log(w(k-1))
      // max_weight = log(max_weight)
      particle.weight = exp( particle.weight - max_weight ); // weights are already in in log land
      weight_sum += particle.weight; 
    }

    // Normalize the cdf to 1
    for( auto& particle: *particle_set )
    {
        particle.weight /= weight_sum;
    }

    return;
}

void ParticleFilter::Initialize(const string& map_file,
                                const Vector2f& loc,
                                const float angle) {
  odom_initialized_ = false;

  map_ = VectorMap("maps/"+ map_file +".txt");
  
  for(auto& particle: particles_)
  {
    particle.loc.x() = rng_.Gaussian( loc.x(), I_(0,0) );
    particle.loc.y() = rng_.Gaussian( loc.y(), I_(1,1) );
    particle.angle = rng_.Gaussian( angle, I_(2,2) );
    particle.weight = 1/FLAGS_num_particles;
  }

  return;
}

void ParticleFilter::GetLocation(Eigen::Vector2f* loc_ptr, 
                                 float* angle_ptr) const {
  if(!loc_ptr || !angle_ptr)
  {
    std::cout<<"GetLocation() was passed a nullptr! What the hell man...\n";
    return;
  }

  Vector2f& loc = *loc_ptr;
  float& angle = *angle_ptr;

  loc = Vector2f(0, 0);
  angle = 0;

  for( const auto& particle: particles_ )
  {
    loc.x() += particle.loc.x();
    loc.y() += particle.loc.y();
    angle += particle.angle;
  }

  loc /= FLAGS_num_particles;
  angle /= FLAGS_num_particles;
  
  return;
}

bool ParticleFilter::isDegenerate()
{
  double sum = 0;
  for( auto& p: particles_)
  {
    sum += p.weight*p.weight;
  }

  double np_effective = 1.0/sum ;
  std::cout << np_effective << std::endl;

  if( np_effective < 0.5*particles_.size() )
  {    
    // Degenerate
    return true;
  }

  return false;
}

}  // namespace particle_filter
