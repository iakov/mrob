/* $COPYRIGHT SKOLTECH
 * $LICENSE_LGPL
 *
 * factor_graph_solve.cpp
 *
 *  Created on: Mar 23, 2018
 *      Author: Gonzalo Ferrer
 *              g.ferrer@skoltech.ru
 *              Mobile Robotics Lab, Skoltech
 */


#include "mrob/factor_graph_solve.hpp"
#include "mrob/CustomCholesky.hpp"

#include <iostream>
#include <Eigen/SparseCore>
#include <Eigen/SparseLU>
#include <Eigen/SparseCholesky>
#include <mrob/factor_graph_solve.hpp>


using namespace mrob;
using namespace std;
using namespace Eigen;


FGraphSolve::FGraphSolve(solveType type, uint_t potNumberNodes, uint_t potNumberFactors):
	FGraph(potNumberNodes, potNumberFactors), type_(type),
    last_stateDim(-1), last_obsDim(-1),
	last_solved_node(-1), last_solved_factor(-1)
{

}

FGraphSolve::~FGraphSolve() = default;


void FGraphSolve::build_problem()
{
    switch(type_)
    {
    case QR:
    case CHOL_ADJ:
        build_adjacency();
        break;
    case CHOL:
    case SCHUR:
    default:
        assert(0 && "FGraphSolve::buildProblem: Method not implemented\n");
        break;
    }
}

void FGraphSolve::solve_once()
{
    // Linearizes and calculates the Jacobians and required matrices
    this->build_problem();

    this->solve_cholesky();

    // Keep indices of last node and factor, as well as dimensions
    last_stateDim = stateDim_;
    last_obsDim = obsDim_;

    last_solved_node = nodes_.size() - 1;
    last_solved_factor = factors_.size() - 1;
    this->update_nodes();
}

void FGraphSolve::solve_incremental()
{
    /**
     * NOTE: it works only with odometry factors
     * and also requires matrix to have at least 2 nodes
     *
     * NOTE: mechanism for evaluation time of the incremental update vs
     * batch update is not present
     */
    solve_chol_incremental();

    // Keep indices of last node and factor
    last_stateDim = stateDim_;
    last_obsDim = obsDim_;

    last_solved_node = nodes_.size() - 1;
    last_solved_factor = factors_.size() - 1;
}

std::vector<MatX1> FGraphSolve::get_estimated_positions()
{
    vector<MatX1> results;
    int acc_start = 0;

    for (uint_t i = 0; i <= last_solved_node; i++) {
        MatX1 updated_pos = nodes_[i]->get_state() + dx_.block(acc_start, 0, nodes_[i]->get_dim(), 1);
        results.emplace_back(updated_pos);

        acc_start += nodes_[i]->get_dim();
    }

    return results;
}


void FGraphSolve::build_adjacency()
{
    // 0) resize properly matrices (if needed)
    r_.resize(obsDim_,1);//dense vector
    A_.resize(obsDim_, stateDim_);//Sparse matrix clear data
    W_.resize(obsDim_, obsDim_);//TODO should we reinitialize this all the time? an incremental should be fairly easy

    // 1) create the vector's structures to iterate faster
    std::vector<std::shared_ptr<Factor> >* factors;
    std::vector<std::shared_ptr<Node> >* nodes;
    // TODO: optimizaing subgrapsh is not an option now
    if (isHoleProblem_)
    {
        factors = &factors_;
        nodes = &nodes_;
    }
    else
    {
        // XXX this option is not working now
        factors = &localFactors_;
        nodes = &localNodes_;
    }

    // 2) vector structure to bookkeep the starting Nodes indices inside A
    std::vector<uint_t> indNodesMatrix;
    indNodesMatrix.reserve(nodes->size());
    uint_t N = 0;
    for (uint_t i = 0; i < nodes->size(); ++i)
    {
        // calculate the indices to access
        uint_t dim = (*nodes)[i]->get_dim();
        indNodesMatrix.push_back(N);
        N += dim;
    }
    assert(N == stateDim_ && "FGraphSolve::buildAdjacency: State Dimensions are not coincident\n");

    // 3) Evaluate every factor given the current state and bookeeping of Factor indices
    std::vector<uint_t> reservationA;
    reservationA.reserve( obsDim_ );
    std::vector<uint_t> reservationW;
    reservationW.reserve( obsDim_ );
    std::vector<uint_t> indFactorsMatrix;
    indFactorsMatrix.reserve(factors->size());
    uint_t M = 0;
    for (uint_t i = 0; i < factors->size(); ++i)
    {
        auto f = (*factors)[i];
        f->evaluate_residuals();
        f->evaluate_jacobians();

        // calculate dimensions for reservation and bookeping vector
        uint_t dim = f->get_dim();
        uint_t allDim = f->get_all_nodes_dim();
        for (uint_t j = 0; j < dim; ++j)
        {
            reservationA.push_back(allDim);
            reservationW.push_back(dim-j);
        }
        indFactorsMatrix.push_back(M);
        M += dim;
    }
    assert(M == obsDim_ && "FGraphSolve::buildAdjacency: Observation dimensions are not coincident\n");
    A_.reserve(reservationA); //Exact allocation for elements.
    W_.reserve(reservationW); //same


    // This could be subject to parallelization, maybe on two steps: eval + build
    for (uint_t i = 0; i < factors->size(); ++i)
    {
        auto f = (*factors)[i];

        // 4) Get the calculated residual
        r_.block(indFactorsMatrix[i], 0, f->get_dim(), 1) << f->get_residual();

        // 5) build Adjacency matrix as a composition of rows
        // 5.1) Get the number of nodes involved. It is a sorted list
        auto neighNodes = f->get_neighbour_nodes();
        //TODO test if a local variable for jacobian speepds up things
        // Iterates over the Jacobian row
        for (uint_t l=0; l < f->get_dim() ; ++l)
        {
            uint_t totalK = 0;
            // Iterates over the number of neighbour Nodes (ordered by construction)
            for (uint_t j=0; j < neighNodes->size(); ++j)
            {
                uint_t indNode = (*neighNodes)[j]->get_id() -1;//Ids start at 1, while vector at 0
                uint_t dimNode = (*neighNodes)[j]->get_dim();
                for(uint_t k = 0; k < dimNode; ++k)
                {
                    uint_t iRow = indFactorsMatrix[i] + l;
                    uint_t iCol = indNodesMatrix[indNode] + k;
                    // This is an ordered insertion
                    A_.insert(iRow,iCol) = f->get_jacobian()(l, k + totalK);
                }
                totalK += dimNode;
            }
        }


        // 5) Get information matrix for every factor, ONLY for the QR we need W^T/2
        for (uint_t l =0; l < f->get_dim(); ++l)
        {
            // only iterates over the upper triangular part
            for (uint_t k = l; k < f->get_dim(); ++k)
            {
                uint_t iRow = indFactorsMatrix[i] + l;
                uint_t iCol = indFactorsMatrix[i] + k;
                if (QR)
                {
                    W_.insert(iRow,iCol) = f->get_trans_sqrt_information_matrix()(l,k);
                }
                else
                {
                    W_.insert(iRow,iCol) = f->get_information_matrix()(l,k);
                }
            }
        }
    } //end factors loop

//    std::cout << A_ << std::endl;
//    std::cout << W_ << std::endl;
//    cout << endl << r_ << endl;
}

matData_t FGraphSolve::evaluate_problem()
{
    matData_t totalChi2 = 0.0;
    for (uint_t i = 0; i < factors_.size(); ++i)
    {
        auto f = factors_[i];
        f->evaluate_residuals();
        f->evaluate_chi2();
        totalChi2 += f->get_chi2();
    }
    return totalChi2;
}

void FGraphSolve::build_adjacency_incremental(SMatCol &A_new, SMatCol &W_new, MatX1 &r_new)
{
    auto    state_dim = stateDim_ - last_stateDim + (last_solved_node == -1 ? 0 : nodes_[last_solved_node]->get_dim()),
            obs_dim = obsDim_ - last_obsDim;

    A_new.resize(obs_dim, state_dim);
    W_new.resize(obs_dim, obs_dim);
    r_new.resize(obs_dim, 1);

    std::vector<uint_t> reservationA, reservationW;

    reservationA.reserve(static_cast<unsigned long>(obs_dim));
    reservationW.reserve(static_cast<unsigned long>(obs_dim));

    auto last_factors_count = last_solved_factor + 1;
    auto states = nodes_.size() - last_solved_node, dims = factors_.size() - last_factors_count;

    std::vector<uint_t> indNodesMatrix;
    uint_t N = 0;

    indNodesMatrix.reserve(states);

    for(auto i = nodes_.begin() + states; i != nodes_.end(); ++i) {
        uint_t dim = (*i)->get_dim();
        indNodesMatrix.push_back(N);
        N += dim;
    }

    std::vector<uint_t> indFactorsMatrix;
    uint_t M = 0;

    indFactorsMatrix.reserve(dims);

    for(auto i = factors_.begin() + states; i != factors_.end(); ++i) {
        (*i)->evaluate_residuals();
        (*i)->evaluate_jacobians();

        uint_t dim = (*i)->get_dim();
        uint_t allDim = (*i)->get_all_nodes_dim();
        for (uint_t j = 0; j < dim; ++j)
        {
            reservationA.push_back(allDim);
            reservationW.push_back(dim - j);
        }
        indFactorsMatrix.push_back(M);
        M += dim;
    }

    A_new.reserve(reservationA);
    W_new.reserve(reservationW);

    // TODO solve this warning!
    for (auto i = last_factors_count; i < factors_.size(); i++) {
        auto f = factors_[i];

        r_new.block(indFactorsMatrix[i - last_factors_count], 0, f->get_dim(), 1) << f->get_residual();

        auto neighNodes = f->get_neighbour_nodes();
        for (uint_t l = 0; l < f->get_dim() ; ++l) {
            uint_t totalK = 0;
            for (auto &j: *neighNodes) {
                uint_t indNode = j->get_id() - 1 - last_solved_node;
                uint_t dimNode = j->get_dim();
                for (uint_t k = 0; k < dimNode; ++k) {
                    uint_t iRow = indFactorsMatrix[i - last_factors_count] + l;
                    uint_t iCol = indNodesMatrix[indNode] + k;

                    A_new.insert(iRow, iCol) = f->get_jacobian()(l, k + totalK);
                }
                totalK += dimNode;
            }
        }


        for (uint_t l = 0; l < f->get_dim(); ++l) {
            for (uint_t k = l; k < f->get_dim(); ++k) {
                uint_t iRow = indFactorsMatrix[i - last_factors_count] + l;
                uint_t iCol = indFactorsMatrix[i - last_factors_count] + k;
                W_new.insert(iRow, iCol) = f->get_information_matrix()(l, k);
            }
        }
    }

//    cout << A_new.toDense() << endl;
//    cout << W_new.toDense() << endl;
//    cout << r_new << endl;
}


void FGraphSolve::build_direct_info()
{
    assert(0 && "FGraphSolve::buildProblemDirectInfo: Routine not implemented");
}

void FGraphSolve::solve_QR()
{
    // W^T/2 corresponds to the upper triangular matrix, so we keep that format
    A_ = W_.triangularView<Eigen::Upper>() * A_;//creates a compressed Matrix
    b_ = W_.triangularView<Eigen::Upper>() * r_;
}

void FGraphSolve::solve_cholesky()
{
    /**
     * I_ dx = b_ corresponds to the normal equation A'*W*A dx = A'*W*r
     * only store the lower part of the information matrix (symmetric)
     */
    I_ = (A_.transpose() * W_.selfadjointView<Eigen::Upper>() * A_).selfadjointView<Eigen::Upper>();
    b_ = A_.transpose() * W_.selfadjointView<Eigen::Upper>() * r_;

    /**
     * Solve LSQ via sparse Cholesky
     * TODO reordering
     */
    CustomCholesky<SMatCol> cholesky(I_);
    y_ = cholesky.matrixL().solve(b_);
    dx_ = cholesky.matrixU().solve(y_);

//    cout << cholesky.getL().toDense() << endl;
//    cout << y_ << endl;
//    cout << endl;
//    cout << dx_ << endl;

    // Save data for incremental update
    long intersection = nodes_.back()->get_dim(),
            starting_index = I_.cols() - intersection,
            correlation = (*(nodes_.end() - 2))->get_dim();

    L00 = cholesky.getL().block(0, 0, starting_index, starting_index);
    L10 = cholesky.getL().block(starting_index, starting_index - correlation, intersection, correlation);
    L11 = cholesky.getL().block(starting_index, starting_index, intersection, intersection);
    I11 = I_.block(starting_index, starting_index, intersection, intersection);
}


void FGraphSolve::solve_chol_incremental() {
    // 0) Build information form of the update
    SMatCol A_new, W_new;
    MatX1 r_new;
    build_adjacency_incremental(A_new, W_new, r_new);

    SMatCol I_new = (A_new.transpose() * W_new.selfadjointView<Eigen::Upper>() * A_new).selfadjointView<Eigen::Upper>();
    MatX1 b_new = A_new.transpose() * W_new.selfadjointView<Eigen::Upper>() * r_new;

    // 1) Make I_new suitable for Cholesky
    vector<Eigen::Triplet<matData_t>> tripletList;

    // Copy the matrix itself
    for (int i = 0; i < I_new.outerSize(); ++i) {
        for (SMatCol::InnerIterator it(I_new, i); it; ++it) {
            tripletList.emplace_back(it.row(), it.col(), it.value());
        }
    }

    // Add old information
    for (int i = 0; i < I11.cols(); ++i) {
        for (SMatCol::InnerIterator it(I11, i); it; ++it) {
            tripletList.emplace_back(it.row(), it.col(), it.value());
        }
    }

    SMatCol _LLT = -1 * L10 * L10.transpose();

    // Subtract cross-terms
    for (int i = 0; i < _LLT.outerSize(); ++i) {
        for (SMatCol::InnerIterator it(_LLT, i); it; ++it) {
            tripletList.emplace_back(it.row(), it.col(), it.value());
        }
    }

    I_new.setFromTriplets(tripletList.begin(), tripletList.end());

    // 2) Calculate L11_new and y10_new incrementally
    long old_intersection = (last_solved_node == -1 ? 0 : nodes_[last_solved_node]->get_dim()),
            old_starting_index = L00.cols();

    CustomCholesky<SMatCol> cholesky;
    cholesky.compute(I_new);

    MatX1 y10 = y_.block(old_starting_index, 0, old_intersection, 1);
    b_new.block(0, 0, old_intersection, 1) += L11 * y10;

    MatX1 y10_new = cholesky.matrixL().solve(b_new);

    y_.conservativeResize(stateDim_, 1);
    y_.block(old_starting_index, 0, y10_new.rows(), 1) << y10_new;

    SMatCol L11_new = cholesky.getL();

    // 3) Join L00, L10 and L11_new together
    SMatCol L_new;
    std::vector<uint_t> reservationL;

    L_new.resize(stateDim_, stateDim_);
    reservationL.reserve(stateDim_);

    for (int i = 0; i < L00.outerSize(); i++) {
        auto    curr = *(L00.outerIndexPtr() + i),
                next = *(L00.outerIndexPtr() + i + 1);
        reservationL.push_back(static_cast<unsigned int &&>(next - curr));
    }

    for (int i = 0; i < L11.outerSize(); i++) {
        auto    curr = *(L11.outerIndexPtr() + i),
                next = *(L11.outerIndexPtr() + i + 1);
        reservationL.push_back(static_cast<unsigned int &&>(next - curr));
    }

    L_new.reserve(reservationL);

    tripletList.clear();

    long old_correlation = (last_solved_node == -1 ? 0 : nodes_[last_solved_node - 1]->get_dim());

    for (int i = 0; i < L00.outerSize(); ++i) {
        for (SMatCol::InnerIterator it(L00, i); it; ++it) {
            tripletList.emplace_back(it.row(), it.col(), it.value());
        }
    }

    for (int i = 0; i < L10.outerSize(); ++i) {
        for (SMatCol::InnerIterator it(L10, i); it; ++it) {
            tripletList.emplace_back(it.row() + old_starting_index, it.col() + old_starting_index - old_correlation, it.value());
        }
    }

    for (int i = 0; i < L11_new.outerSize(); ++i) {
        for (SMatCol::InnerIterator it(L11_new, i); it; ++it) {
            tripletList.emplace_back(it.row() + old_starting_index, it.col() + old_starting_index, it.value());
        }
    }

    L_new.setFromTriplets(tripletList.begin(), tripletList.end());

    // 4) Solve Rdx = y
    dx_ = L_new.adjoint().triangularView<Eigen::Upper>().solve(y_);

//    cout << L_new.toDense() << endl;
//    cout << y_new << endl;
//    cout << endl;
//    cout << dx_ << endl;

    // Save data for incremental update
    long intersection = nodes_.back()->get_dim(),
            starting_index = L_new.cols() - intersection,
            correlation = (*(nodes_.end() - 2))->get_dim();
    L00 = L_new.block(0, 0, starting_index, starting_index);
    L10 = L_new.block(starting_index, starting_index - correlation, intersection, correlation);
    L11 = L_new.block(starting_index, starting_index, intersection, intersection);
    I11 = I_new.block(I_new.rows() - intersection, I_new.cols() - intersection, intersection, intersection);
}


void FGraphSolve::update_nodes()
{
    // TODO reordering? not considered here
    int acc_start = 0;
    for (int i = 0; i <= last_solved_node; i++) {
        auto node_update = dx_.block(acc_start, 0, nodes_[i]->get_dim(), 1);
        nodes_[i]->update(node_update);

        acc_start += nodes_[i]->get_dim();
    }
}



