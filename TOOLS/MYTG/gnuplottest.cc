#include <vector>
#include <cmath>
#include <boost/tuple/tuple.hpp>
#include "gnuplot-iostream.h"
int main() {
    Gnuplot gp;
    std::vector<boost::tuple<double, double, double, double> > pts_A;
    std::vector<double> pts_B_x;
    std::vector<double> pts_B_y;
    std::vector<double> pts_B_dx;
    std::vector<double> pts_B_dy;
    for(double alpha=0; alpha<1; alpha+=1.0/24.0) {
        double theta = alpha*2.0*3.14159;
        pts_A.push_back(boost::make_tuple(
             cos(theta),
             sin(theta),
            -cos(theta)*0.1,
            -sin(theta)*0.1
        ));

        pts_B_x .push_back( cos(theta)*0.8);
        pts_B_y .push_back( sin(theta)*0.8);
        pts_B_dx.push_back( sin(theta)*0.1);
        pts_B_dy.push_back(-cos(theta)*0.1);
    }
    gp << "set xrange [-2:2]\nset yrange [-2:2]\n";
    gp << "plot '-' with vectors title 'pts_A', '-' with vectors title 'pts_B'\n";
    gp.send1d(pts_A);
    gp.send1d(boost::make_tuple(pts_B_x, pts_B_y, pts_B_dx, pts_B_dy));
} // very simple tool right???
