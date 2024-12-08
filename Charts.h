#pragma once
#include <vector>
#include <numeric>
#include <matplot/matplot.h>
#include <Eigen/Dense>

namespace plt = matplot;

class Plot3D {
private:
    struct DataStorage {
        std::vector<double> x1, y1, z1;
        std::vector<double> x2, y2, z2;
        std::vector<double> t;
        std::vector<double> error_x, error_y, error_z;

        void init_data(const Eigen::MatrixXd& xyz1, const Eigen::MatrixXd& xyz2) {
            int cols = xyz1.cols();
            x1.resize(cols);
            y1.resize(cols);
            z1.resize(cols);
            x2.resize(cols);
            y2.resize(cols);
            z2.resize(cols);
            t.resize(cols);

            for (int i = 0; i < cols; ++i) {
                x1[i] = xyz1(0, i);
                y1[i] = xyz1(1, i);
                z1[i] = xyz1(2, i);
                x2[i] = xyz2(0, i);
                y2[i] = xyz2(1, i);
                z2[i] = xyz2(2, i);
            }
            std::iota(t.begin(), t.end(), 0);
        }

        void init_error() {
            size_t size = t.size();
            error_x.resize(size);
            error_y.resize(size);
            error_z.resize(size);

            for (size_t i = 0; i < size; ++i) {
                error_x[i] = std::abs(x1[i] - x2[i]);
                error_y[i] = std::abs(y1[i] - y2[i]);
                error_z[i] = std::abs(z1[i] - z2[i]);
            }
        }
    };

    DataStorage data;

public:
    void setData(const Eigen::MatrixXd& xyz1, const Eigen::MatrixXd& xyz2) {
        data.init_data(xyz1, xyz2);
        data.init_error();
    }

    void plot_3l() const {
        using namespace matplot;

        auto fig = figure(true);

        auto ax1 = fig->add_subplot(3, 1, 1);
        hold(on);
        ax1->plot(data.t, data.x1, "b")->line_width(2);
        ax1->plot(data.t, data.x2, "g")->line_width(1);
        ax1->title("Comparison: x");
        ax1->legend({ "x_test", "x_pred" });
        ax1->grid(true);

        auto ax2 = fig->add_subplot(3, 1, 2);
        hold(on);
        ax2->plot(data.t, data.y1, "b")->line_width(2);
        ax2->plot(data.t, data.y2, "g")->line_width(1);
        ax2->title("Comparison: y");
        ax2->legend({ "y_test", "y_pred" });
        ax2->grid(true);

        auto ax3 = fig->add_subplot(3, 1, 3);
        hold(on);
        ax3->plot(data.t, data.z1, "b")->line_width(2);
        ax3->plot(data.t, data.z2, "g")->line_width(1);
        ax3->title("Comparison: z");
        ax3->legend({ "z_test", "z_pred" });
        ax3->grid(true);

        show();
    }

    void plot_3d_points() const {
        using namespace matplot;

        plot3(data.x1, data.y1, data.z1);
        hold(on);

        plot3(data.x2, data.y2, data.z2);
        view(45, 45);
        matplot::legend({ "xyz1", "xyz2" });
        grid(true);
        show();
    }

    void plot_error() const {
        using namespace matplot;
        auto fig = figure(true);
        fig->size(1900, 800); // clearer view

        auto ax = fig->current_axes();
        ax->plot(data.t, data.error_x, "r")->line_width(2);
        ax->plot(data.t, data.error_y, "m")->line_width(2);
        ax->plot(data.t, data.error_z, "k")->line_width(2);
        ax->title("Error through time");
        ax->legend({ "Error_x", "Error_y", "Error_z" });
        ax->grid(true);

        show();
    }

};
