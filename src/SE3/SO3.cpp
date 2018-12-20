/* $COPYRIGHT_SKOLTECH
 * $LICENSE_LGPL
 *
 * SO3.cpp
 *
 *  Created on: Feb 12, 2018
 *      Author: Gonzalo Ferrer
 *              g.ferrer@skoltech.ru
 *              Mobile Robotics Lab, Skoltech
 */


#include "skmr/SO3.hpp"
#include <cmath>
#include <iostream>


using namespace skmr;

SO3::SO3(const Mat31 &w) : Mat3(Mat3::Identity())
{
    //std::cout << "SO3 with Mat31" << std::endl;
    this->exp(hat3(w));
}
template<typename OtherDerived>
SO3::SO3(const Eigen::MatrixBase<OtherDerived>& other)  :
Mat3(other)
{
    //std::cout << "SO3 MAT3" << std::endl;
}

template<typename OtherDerived>
SO3& SO3::operator=(const Eigen::MatrixBase <OtherDerived>& other)
{
    //std::cout << "SO3 operator equal" << std::endl;
    this->Mat3::operator=(other);
    return *this;
}

void SO3::update(const Mat31 &dw)
{
    SO3 dR(dw);
    *this = dR * (*this);
}

Mat31 skmr::vee3(const Mat3 &w_hat)
{
    Mat31 w;
    w << -w_hat(1,2), w_hat(0,2), -w_hat(0,1);
    return w;
}

Mat3 skmr::hat3(const Mat31 &w)
{
    Mat3 w_hat;
    w_hat <<     0.0, -w(2),  w(1),
                w(2),   0.0, -w(0),
               -w(1),  w(0),   0.0;
    return w_hat;
}


void SO3::exp(const Mat3 &w_hat)
{
    Mat31 w = vee3(w_hat);
    double o = w.norm();
    if ( o < 1e-12){
        // sin(o)/0 -> 1. Others approximate this with Taylor, but we will leave it as 1
        *this << Mat3::Identity() + w_hat;
        return;
    }
    double c1 = std::sin(o)/o;
    double c2 = (1 - std::cos(o))/o/o;
    *this << Mat3::Identity() + c1 * w_hat + c2 * w_hat *w_hat;
}

Mat3 SO3::ln(double *ro) const
{
    // Logarithmic mapping of the rotations
    Mat3 res;
    double tr = (this->trace()-1)*0.5;
    double o;
    if (tr < 1.0 && tr > -1.0 )
    {
        // Usual case, tr \in (-1,1) and theta \in (-pi,pi)
        o = std::fabs(std::acos(tr));
        res << 0.5 * o / std::sin(o) * ( (*this) - this->transpose());
    }
    else if (tr >= 1.0)
    {
        // Special case tr =1  and theta = 0
        o = 0.0;
        res << Mat3::Zero();
    }
    else
    {
        // Special case tr = -1  and theta = +- pi or multiples
        o = M_PI;
        // R = I + 0 + (2/pi^2)W^2, which makes it symmetric R = Rt (W = hat(w))
        // From here, we know that W^2 = ww^t - theta^2I, (you can span W^2 to see this)
        // which leaves R = I + 2/pi2 (wwt - pi2 I)
        // R+I = 2/pi2 wwt
        // wwt = pi2 / 2 (R+I)
        // so we find the maximum row and apply that formula
        // knowing that norm(w) = pi
        Mat31 w;
        if( (*this)(0,0) > (*this)(1,1) && (*this)(0,0) > (*this)(2,2) )
        {
            // For stability, we average the two elements since it must be symetric
            w << (*this)(0,0) + 1.0,
                 0.5 * ( (*this)(0,1) + (*this)(1,0)),
                 0.5 * ( (*this)(0,2) + (*this)(2,0));
        }
        else if( (*this)(1,1) > (*this)(0,0) && (*this)(1,1) > (*this)(2,2) )
        {
            w << 0.5 * ( (*this)(1,0) + (*this)(0,1)),
                 (*this)(1,1) + 1.0,
                 0.5 * ( (*this)(1,2) + (*this)(2,1));
        }
        else
        {
            w << 0.5 * ( (*this)(2,0) + (*this)(0,2)),
                 0.5 * ( (*this)(2,1) + (*this)(1,2)),
                 (*this)(2,2) + 1.0;
        }
        // normalize the vector w, such that norm(w) = pi
        double length = w.norm();
        if (length > 0.0)
        {
            w *= M_PI / length;
        }
        else
        {
            w << 0.0, 0.0, 0.0;
        }
        res = hat3(w);
    }
    if (ro != nullptr) *ro = o;
    return res;
}

Mat31 SO3::ln_vee() const
{
    Mat3 w_hat = this->ln();
    return vee3(w_hat);
}

SO3 SO3::inv(void) const
{
    return this->transpose();
}

Mat3 SO3::adj() const
{
    return *this;
}

void SO3::print(void) const
{
    std::cout << *this << std::endl;
}


void SO3::print_lie(void) const
{

    Mat31 w =  this->ln_vee();
    std::cout << w << std::endl;
}
