/* Copyright (c) 2018, Skolkovo Institute of Science and Technology (Skoltech)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * factor1Pose1Plane4d.cpp
 *
 *  Created on: Oct 10, 2020
 *      Author: Gonzalo Ferrer
 *              g.ferrer@skoltech.ru
 *              Mobile Robotics Lab, Skoltech
 */

#include "mrob/factors/factor1Pose1Plane4d.hpp"
#include "mrob/SE3.hpp"

#include <iostream>

using namespace mrob;

Factor1Pose1Plane4d::Factor1Pose1Plane4d(const Mat41 &observation, std::shared_ptr<Node> &nodePose,
        std::shared_ptr<Node> &nodePlane, const Mat4 &obsInf):
                Factor(4,10), obs_(observation), W_(obsInf), reversedNodeOrder_(false)
{
    // ensure that plane 4d, normal \in P2 and distance \in R
    obs_.head(3).normalize();
    // To preserve the order when building the Adjacency matrix
    if (nodePose->get_id() < nodePlane->get_id())
    {
        neighbourNodes_.push_back(nodePose);
        neighbourNodes_.push_back(nodePlane);
    }
    else
    {
        neighbourNodes_.push_back(nodePlane);
        neighbourNodes_.push_back(nodePose);
        // set reverse mode
        reversedNodeOrder_ = true;
    }
}
Factor1Pose1Plane4d::~Factor1Pose1Plane4d()
{

}

void Factor1Pose1Plane4d::evaluate_residuals()
{
    // From nosePose we observe a plane, in local coordinates
    // which then it is related to the landmark plane, in global coordinates. Thus,
    // the residual is formulated, according to our convention in factor.hpp:
    //
    //      r = z_pi - h(T,pi_0) = z_pi - T^{-T}pi.
    //
    //  Points and planes are duals in 3D, as explained in Multiview-geometry by hartley and zisserman
    // From T we observe z_pi, and the residual is r = z - T^{-T}pi
    uint_t poseIndex = 0;
    uint_t landmarkIndex = 1;
    if (reversedNodeOrder_)
    {
        landmarkIndex = 0;
        poseIndex = 1;
    }
    Mat4 Tx = get_neighbour_nodes()->at(poseIndex)->get_state();
    // The transformation we are looking for here is Txw, from world to local x.
    // Check on transfomration of planes
    Tinv_transp_ = SE3(Tx).inv().T().transpose();
    plane_ = get_neighbour_nodes()->at(landmarkIndex)->get_state();
    r_ = Tinv_transp_*plane_ - obs_;
}
void Factor1Pose1Plane4d::evaluate_jacobians()
{
    Mat<4,6> Jx = Mat<4,6>::Zero();
    Mat41 pi = Tinv_transp_*plane_;
    Mat31 normal = pi.head(3);
    Jx.topLeftCorner<3,3>() = hat3(normal);
    Jx.bottomRightCorner<1,3>() =  normal;
    if (!reversedNodeOrder_)
    {
        J_.topLeftCorner<4,6>() = Jx;
        J_.topRightCorner<4,4>() = Tinv_transp_;
    }
    else
    {
        J_.topLeftCorner<4,4>() = Tinv_transp_;
        J_.topRightCorner<4,6>() = Jx;
    }
}
void Factor1Pose1Plane4d::evaluate_chi2()
{
    chi2_ = 0.5 * r_.dot(W_ * r_);
}

void Factor1Pose1Plane4d::print() const
{
    std::cout << "Printing Plane Factor: " << id_ << ", obs= \n" << obs_
               << "\n Residuals= \n" << r_
               << " \nand Information matrix\n" << W_
               << "\n Calculated Jacobian = \n" << J_
               << "\n Chi2 error = " << chi2_
               << " and neighbour Nodes " << neighbourNodes_.size()
               << std::endl;
}