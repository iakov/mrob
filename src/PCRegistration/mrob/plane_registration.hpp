/* $COPYRIGHT SKOLTECH
 * $LICENSE_LGPL
 *
 * planeRegistration.hpp
 *
 *  Created on: Jan 28, 2019
 *      Author: Gonzalo Ferrer
 *              g.ferrer@skoltech.ru
 *              Mobile Robotics Lab, Skoltech
 */

#ifndef PLANEREGISTRATION_HPP_
#define PLANEREGISTRATION_HPP_

#include <vector>
#include "mrob/SE3.hpp"
#include <Eigen/StdVector>
#include "mrob/plane.hpp"

#include <unordered_map>
#include <memory>


namespace mrob{

/**
 * class PlaneRegistration introduced a class for the alignment of
 * planes.
 */
class PlaneRegistration{
  public:
    PlaneRegistration();
    ~PlaneRegistration();

    int solve();
    std::vector<SE3>& get_transformations();//if solved

    /**
     * add_plane adds a plane structure already initialized and filled with data
     */
    void add_plane(std::shared_ptr<Plane> &plane, uint_t id) {planes_.emplace(id, plane);};



  protected:
    std::unordered_map<uint_t, std::shared_ptr<Plane>> planes_;
    std::vector<SE3> transformations_;
    // flag for detecting when is has been solved
    uint_t isSolved_;
    uint_t time_;

};



}// namespace
#endif /* PLANEREGISTRATION_HPP_ */