/* $COPYRIGHT SKOLTECH
 * $LICENSE_LGPL
 *
 *  Created on: Jan 14, 2019
 *      Author: Konstantin Pakulev
 *              konstantin.pakulev@skoltech.ru
 *              Gonzalo Ferrer
 *              g.ferrer@skoltech.ru
 *              Mobile Robotics Lab, Skoltech
 */

#include <iostream>
#include <Eigen/Cholesky>
#include <mrob/factors/factor2Poses2d.hpp>


using namespace mrob;


Factor2Poses2d::Factor2Poses2d(const Mat31 &observation, std::shared_ptr<Node> &nodeOrigin,
                               std::shared_ptr<Node> &nodeTarget, const Mat3 &obsInf):
        Factor(3, 6), obs_(observation), W_(obsInf)
{
    assert(nodeOrigin->get_id() && "Factor2Poses2d::Factor2Poses2d: Non initialized Node1. Add nodes first and then Factors to the FG\n");
    assert(nodeTarget->get_id() && "Factor2Poses2d::Factor2Poses2d: Non initialized Node2. Add nodes first and then Factors to the FG\n");
    if (nodeOrigin->get_id() < nodeTarget->get_id())
    {
        neighbourNodes_.push_back(nodeOrigin);
        neighbourNodes_.push_back(nodeTarget);
    }
    else
    {
        // we reverse the order and simply invert the observation function (not always true)
        neighbourNodes_.push_back(nodeTarget);
        neighbourNodes_.push_back(nodeOrigin);

        // reverse observations to account for this
        obs_ = -observation;
    }
    WT2_ = W_.llt().matrixU();
}


void Factor2Poses2d::evaluate_residuals() {
    // Evaluation of h(i,j)
    auto    node1 = get_neighbour_nodes()->at(0).get()->get_state(),
            node2 = get_neighbour_nodes()->at(1).get()->get_state();

    auto h = node2 - node1;

    r_ = h - obs_;
    r_[2] = wrap_angle(r_[2]);

}

void Factor2Poses2d::evaluate_jacobians()
{
    // Jacobians
    J_ <<   1, 0, 0, -1, 0, 0,
            0, 1, 0, 0, -1, 0,
            0, 0, 1, 0, 0, -1;
}

void Factor2Poses2d::evaluate_chi2()
{
    chi2_ = r_.dot(W_ * r_);
}

void Factor2Poses2d::print() const
{
    std::cout << "Printing Factor:" << id_ << ", obs= \n" << obs_
              << "\n Residuals=\n " << r_
              << " \nand Information matrix\n" << W_
              << "\n Calculated Jacobian = \n" << J_
              << "\n Chi2 error = " << chi2_
              << " and neighbour Nodes " << neighbourNodes_.size()
              << std::endl;
}


Factor2Poses2dOdom::Factor2Poses2dOdom(const Mat31 &observation, std::shared_ptr<Node> &nodeOrigin, std::shared_ptr<Node> &nodeTarget,
                         const Mat3 &obsInf, bool updateNodeTarget) :
                         Factor2Poses2d(observation, nodeOrigin, nodeTarget, obsInf)
{
    assert(nodeOrigin->get_id() < nodeTarget->get_id() && "Factor2Poses2dOdom::Factor2Poses2dodom: Node origin id is posterior to the destination node\n");
    if (updateNodeTarget)
    {
        Mat31 dx = get_odometry_prediction(nodeOrigin->get_state(), obs_) - nodeTarget->get_state();
        nodeTarget->update(dx);
    }
}

void Factor2Poses2dOdom::evaluate_residuals()
{
    // Evaluation of residuals as g (x_origin, observation) - x_dest
    auto    stateOrigin = get_neighbour_nodes()->at(0).get()->get_state(), // x[i - 1]
            stateTarget = get_neighbour_nodes()->at(1).get()->get_state(); // x[i]
    auto prediction = get_odometry_prediction(stateOrigin, obs_);

    r_ = stateTarget - prediction;
    r_[2] = wrap_angle(r_[2]);

}
void Factor2Poses2dOdom::evaluate_jacobians()
{
    // Get the position of node we are traversing from
    auto node1 = get_neighbour_nodes()->at(0).get()->get_state();

    auto s = -obs_[1] * sin(node1[2]), c = obs_[1] * sin(node1[2]);

    // Jacobians for odometry model which are: G and -I
    J_ <<   1, 0, s,    -1, 0, 0,
            0, 1, c,    0, -1, 0,
            0, 0, 1,    0, 0, -1;
}


Mat31 Factor2Poses2dOdom::get_odometry_prediction(Mat31 state, Mat31 motion) {
    state[2] += motion[0];
    state[0] += motion[1] * cos(state[2]);
    state[1] += motion[1] * sin(state[2]);
    state[2] += motion[2];

    return state;
}

