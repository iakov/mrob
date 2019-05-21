/* $COPYRIGHT_SKOLTECH
 * $LICENSE_LGPL
 *
 * factor2Poses3d.cpp
 *
 *  Created on: Feb 28, 2018
 *      Author: Gonzalo Ferrer
 *              g.ferrer@skoltech.ru
 *              Mobile Robotics Lab, Skoltech 
 */


#include "mrob/factors/factor2Poses3d.hpp"

#include <iostream>
#include <Eigen/Cholesky>

using namespace mrob;


Factor2Poses3d::Factor2Poses3d(const Mat61 &observation, std::shared_ptr<Node> &nodeOrigin,
        std::shared_ptr<Node> &nodeTarget, const Mat6 &obsInf, bool updateNodeTarget):
        Factor(6,12), obs_(observation), Tobs_(observation), W_(obsInf)
{
    if (nodeOrigin->get_id() < nodeTarget->get_id())
    {
        neighbourNodes_.push_back(nodeOrigin);
        neighbourNodes_.push_back(nodeTarget);
    }
    else
    {
        neighbourNodes_.push_back(nodeTarget);
        neighbourNodes_.push_back(nodeOrigin);

        // inverse observations to correctly modify this
        obs_ = -observation;
        Tobs_ = SE3(obs_);
    }
    WT2_ = W_.llt().matrixU();
    if (updateNodeTarget)
    {
        // dx =  Tobs * T_xO * Tx_t^-1
        Mat4 TxOrigin = nodeOrigin->get_stateT();
        Mat4 TxTarget = nodeTarget->get_stateT();
        SE3 dT = Tobs_ * SE3(TxOrigin) * SE3(TxTarget).inv();
        nodeTarget->update(dT.ln_vee());
    }
}

Factor2Poses3d::~Factor2Poses3d()
{
}

void Factor2Poses3d::evaluate_residuals()
{
    // r = h(x_O,x_T) - z (in general). From Origin we observe Target
    // Tr = Tobs * Tx1 * Txt^-1
    Mat4 TxOrigin = get_neighbour_nodes()->at(0)->get_stateT();
    Mat4 TxTarget = get_neighbour_nodes()->at(1)->get_stateT();
    Tr_ = Tobs_ * SE3(TxOrigin) * SE3(TxTarget).inv();
    r_ = Tr_.ln_vee();
}
void Factor2Poses3d::evaluate_jacobians()
{
    // it assumes you already have evaluated residuals
    J_.topLeftCorner<6,6>() = Tobs_.adj();
    J_.topRightCorner<6,6>() = -Tr_.adj();
}

void Factor2Poses3d::evaluate_chi2()
{
    chi2_ = 0.5 * r_.dot(W_ * r_);
}
void Factor2Poses3d::print() const
{
    std::cout << "Printing Factor: " << id_ << ", obs= \n" << obs_
              << "\n Residuals= \n" << r_
              << " \nand Information matrix\n" << W_
              << "\n Calculated Jacobian = \n" << J_
              << "\n Chi2 error = " << chi2_
              << " and neighbour Nodes " << neighbourNodes_.size()
              << std::endl;
}

