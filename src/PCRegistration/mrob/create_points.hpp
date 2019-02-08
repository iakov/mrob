/* $COPYRIGHT_SKOLTECH
 * $LICENSE_LGPL
 *
 * create_points.hpp
 *
 *  Created on: Feb 6, 2018
 *      Author: Gonzalo Ferrer
 *              g.ferrer@skoltech.ru
 *              Mobile Robotics Lab, Skoltech 
 */

#ifndef CREATE_POINTS_HPP_
#define CREATE_POINTS_HPP_

#include "mrob/SE3.hpp"
#include <random>
#include <Eigen/StdVector> // for fixed size SE3 objects



using namespace Eigen;

namespace mrob{

/**
 * Class samples a random configuration on SE3 using a
 * uniform distribution U(-R_range, R_range)
 * or any implemented distribution
 */
class SampleUniformSE3{
  public:
    SampleUniformSE3(double R_range, double t_range);
    SampleUniformSE3(double R_min, double R_max, double t_min, double t_max);
    ~SampleUniformSE3();
    SE3 samplePose();
    Mat31 samplePosition();
    SO3 sampleOrientation();
  protected:
    std::default_random_engine generator_;
    std::uniform_real_distribution<double> rotationUniform_;
    std::uniform_real_distribution<double> tUniform_;
};

/**
 * Class samples a point on a surface over the plane XY
 * according to a fixed noise on height
 */
class SamplePlanarSurface{
  public:
    SamplePlanarSurface(double zStd);
    ~SamplePlanarSurface();
    /**
     * samples a point with noise on a square of given length
     */
    Mat31 samplePoint(double length);

  protected:
    std::default_random_engine generator_;
    std::uniform_real_distribution<double> x_, y_;
    std::normal_distribution<double> z_;
};

/**
 * Class generating a sequence of pointClouds given some specifications.
 */
class CreatePoints{
public:
    /**
     * Creates a class
     */
    CreatePoints(uint_t N = 10, uint_t numberPlanes = 4, uint_t numberPoses = 2, double noisePerPoint = 0.1);
    ~CreatePoints();

    std::vector<Mat31>& get_point_cloud(uint_t t);
    std::vector<int>& get_plane_ids(uint_t t);

    std::vector<SE3>& get_poses() {return poseGroundTruth_;};

    void print() const;


protected:
    // generation parameters
    uint_t numberPoints_; // Number of points
    uint_t numberPlanes_; // Number of planes in the virtual environment
    double noisePerPoint_;
    double rotationRange_;
    double transRange_;
    double lamdaOutlier_;
    SampleUniformSE3 samplePoses_;
    SamplePlanarSurface samplePoints_;

    // Point cloud data generated
    std::vector< std::vector<Mat31> > X_;
    // IDs for facilitating the task of DA and normal computation
    std::vector< std::vector<uint_t>> pointId_;

    // Trajectory parameters
    double xRange_, yRange_; // dimension of the workspace
    SE3 initialPose_, finalPose_;
    std::vector<SE3> poseGroundTruth_;
    uint_t numberPoses_;

    // Generation of planes
    void sample_plane(uint_t nPoints, uint_t id, uint_t t);
    std::vector<SE3> planes_;

};

}
#endif /* CREATE_POINTS_HPP_ */
