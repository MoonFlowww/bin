#ifndef ARRAY_H
#define ARRAY_H

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace Array {

    inline std::string marge(const std::string& str, int totalWidth) {
        if (static_cast<int>(str.size()) >= totalWidth) {
            return str;
        }
        int padding = (totalWidth - static_cast<int>(str.size())) / 2;
        int extra = (totalWidth - static_cast<int>(str.size())) % 2;
        return std::string(padding, ' ') + str + std::string(padding + extra, ' ');
    }

    inline std::string ClassicArray(const std::vector<std::string>& s, bool title = false, bool color = false, bool addSpace = false) {
        std::ostringstream oss;
        std::vector<int> widths;
        int heights = s.size();

        for (int i = 0; i < heights; ++i) {
            widths.push_back(static_cast<int>(s[i].size()));
        }
        int total_w = *std::max_element(widths.begin(), widths.end()) + 8 + 2; // 2 '|' + 8 ' '
        std::string Hbar = "+" + std::string(total_w - 2, '-') + "+";
        std::string Vbar = "|";

        if (color) {
            Hbar.insert(0, "\033[1m\033[36m");
            Hbar += "\033[0m";
            Vbar.insert(0, "\033[1m\033[36m");
            Vbar += "\033[0m";
        }

        std::vector<std::string> formatted = s;
        if (addSpace) {
            formatted.insert(formatted.begin() + 1, "  ");
            formatted.push_back("  ");
            heights += 2;
        }

        if (title && !formatted.empty()) {
            std::string w = marge(formatted[0], total_w - 2);
            oss << Hbar << "\n";
            oss << Vbar
                << "\033[1m\033[37m"
                << w
                << "\033[0m"
                << Vbar
                << "\n";
        }

        oss << Hbar << "\n";
        for (int i = title ? 1 : 0; i < heights; ++i) {
            std::string w = marge(formatted[i], total_w - 2);
            oss << Vbar
                << "\033[1m\033[37m"
                << w
                << "\033[0m"
                << Vbar
                << "\n";
        }
        oss << Hbar << "\n";

        return oss.str();
    }


    inline std::string DataArray(const std::vector<std::string>& data, const std::vector<std::string>& cat, bool title = false, bool color = false) {
        std::ostringstream oss;
        int heights = data.size();
        int max_cat_width = 0;
        int max_data_width = 0;

        for (const auto& category : cat) {
            if (static_cast<int>(category.size()) > max_cat_width) {
                max_cat_width = static_cast<int>(category.size());
            }
        }


        for (int i = title ? 1 : 0; i < heights; ++i) {
            int data_length = static_cast<int>(data[i].size());
            if (data_length > max_data_width) {
                max_data_width = data_length;
            }
        }

        int total_w = max_cat_width + 3 + max_data_width + 6; // 2 '|' + 4 espaces
        std::string Hbar = "+" + std::string(total_w - 2, '=') + "+";
        std::string Vbar = "|";

        if (color) {
            Hbar.insert(0, "\033[1m\033[36m");
            Hbar += "\033[0m";
            Vbar.insert(0, "\033[1m\033[36m");
            Vbar += "\033[0m";
        }

        if (title && !data.empty()) {
            std::string w = marge(data[0], total_w - 2);
            oss << Hbar << "\n";
            oss << Vbar
                << "\033[1m\033[37m"
                << w
                << "\033[0m"
                << Vbar
                << "\n";
        }

        oss << Hbar << "\n";
        std::string InnerLine = "|" + std::string(total_w - 2, '-') + "|";
        for (int i = title ? 1 : 0; i < heights; ++i) {
            if (title && i - 1 >= static_cast<int>(cat.size())) {
                break;
            }
            std::string category = title ? cat[i - 1] : cat[i];
            std::string datum = data[i];

            oss << Vbar << "  "
                << "\033[1m\033[37m"
                << std::setw(max_cat_width) << std::left << category
                << "\033[0m"
                << " : "
                << "\033[1m\033[37m"
                << std::setw(max_data_width) << std::left << datum
                << "\033[0m"
                << "  " << Vbar << "\n";

            if (i < heights - 1) {
                oss << InnerLine << "\n";
            }
        }
        oss << Hbar << "\n";

        return oss.str();
    }

template <typename T> // in case std::vector<std::vector<..>> or Eigen::Matrix
    std::string MatrixArray(const T& matrix, bool InnerGrid = false, bool color = false) {
        std::ostringstream oss;
        int rows, cols;

        if constexpr (std::is_same<T, Eigen::MatrixXd>::value) { // constexpr : pre-compile
            rows = matrix.rows();
            cols = matrix.cols();
        } else if constexpr (std::is_same<T, std::vector<std::vector<double>>>::value) {
            rows = matrix.size();
            cols = rows > 0 ? matrix[0].size() : 0;
        } else { return "Unsupported matrix type!"; }

        int cellWidth = 10;
        std::string Hbar = "+" + std::string(cols * cellWidth + cols - 1, '-') + "+";
        std::string Vbar = "|";


        if (color) {
            Hbar.insert(0, "\033[1m\033[36m");
            Hbar += "\033[0m";
            Vbar.insert(0, "\033[1m\033[36m");
            Vbar += "\033[0m";
        }

        oss << Hbar << "\n";
        for (int r = 0; r < rows; ++r) {
            oss << Vbar;
            for (int c = 0; c < cols; ++c) {
                double value;
                if constexpr (std::is_same<T, Eigen::MatrixXd>::value) {
                    value = matrix(r, c);
                } else {
                    value = matrix[r][c];
                }
                oss << " " << std::setw(cellWidth - 1) << std::left << value << Vbar;
            }
            oss << "\n";
            if (InnerGrid && r < rows - 1) {
                oss << "|" + std::string(cols * cellWidth + cols - 1, '-') + "|\n";
            }
        }
        oss << Hbar << "\n";

        return oss.str();
    }

} // namespace Array

#endif // ARRAY_H

/* Exemple :
int main() {

    
    // 1 column
    std::vector<std::string> data1 = { "Ticket", "Pasta", "Lasagna", "Salade"};
    std::string array1 = Array::ClassicArray(data1, true, true, true); // ostringstream
    std::cout << "Classic Array:\n" << array1 << std::endl;



    //Data with columns
    std::vector<std::string> labels = { "Price", "Volume" };
    std::vector<std::string> data2 = { "AAPL", "192.36", "263.28" };
    std::string array2 = Array::DataArray(data2, labels, true, true); // ostringstream
    std::cout << "Data Array:\n" << array2 << std::endl;



    // Matrix
    Eigen::MatrixXd eigen_matrix(2, 3);
    eigen_matrix << 1.1, 2.2, 3.3, 4.4, 5.5, 6.6;
    std::vector<std::vector<double>> vector_matrix = {{1.1, 2.2, 3.3}, {4.4, 5.5, 6.6}};
    std::cout << Array::PrintMatrix(eigen_matrix, true, true); // ostringstream



    return 0;
}
*/
