#ifndef ARRAY_H
#define ARRAY_H

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <Eigen/Dense>

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

    template <typename T> // for Eigen::Matrix, std::vector<std::vector<..>>, and std::vector<std::pair<int, int>>
    inline std::string MatrixArray(const T& matrix, bool showGrid = false, bool showTitle = false, const std::string& title = "Matrix") {
        std::ostringstream oss;
        int rows, cols;

        // Determine the type and dimensions of the matrix
        if constexpr (std::is_same<T, Eigen::MatrixXd>::value) {
            rows = matrix.rows();
            cols = matrix.cols();
        }
        else if constexpr (std::is_same<T, std::vector<std::vector<double>>>::value) {
            rows = matrix.size();
            cols = rows > 0 ? matrix[0].size() : 0;
        }
        else if constexpr (std::is_same<T, std::vector<std::pair<int, int>>>::value) {
            rows = matrix.size();
            cols = 2; // Each pair has two elements
        }
        else {
            return "Unsupported Data type!\n Only Eigen::Matrix, std::vector<std::vector<..>>, and std::vector<std::pair<int, int>> are valid!";
        }

        int maxWidth = 0;
        // Calculate the maximum width needed for cell formatting
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                std::ostringstream temp;
                if constexpr (std::is_same<T, Eigen::MatrixXd>::value) {
                    temp << matrix(r, c);
                }
                else if constexpr (std::is_same<T, std::vector<std::vector<double>>>::value) {
                    temp << matrix[r][c];
                }
                else if constexpr (std::is_same<T, std::vector<std::pair<int, int>>>::value) {
                    if (c == 0)
                        temp << matrix[r].first;
                    else
                        temp << matrix[r].second;
                }
                maxWidth = std::max(maxWidth, static_cast<int>(temp.str().size()));
            }
        }
        maxWidth += 2;

        int totalWidth = cols * (maxWidth + 3) - 1;
        std::string Hbar = "\033[34m+" + std::string(totalWidth, '-') + "+\033[0m";
        std::string grayHline = "\033[90m" + std::string(totalWidth, '-') + "\033[0m";
        std::string grayVbar = "\033[90m|\033[0m";
        std::string blueVbar = "\033[34m|\033[0m";

        //title
        if (showTitle) {
            std::string displayTitle = title.empty() ? "Matrix" : title;
            int padding = (totalWidth - displayTitle.size()) / 2;
            int extra = (totalWidth - displayTitle.size()) % 2;
            oss << Hbar << "\n";
            oss << blueVbar << std::string(padding, ' ') << displayTitle << std::string(padding + extra, ' ') << blueVbar << "\n";
        }

        oss << Hbar << "\n"; // Upper bar

        for (int r = 0; r < rows; ++r) {
            oss << blueVbar;
            for (int c = 0; c < cols; ++c) {
                std::ostringstream cell;
                if constexpr (std::is_same<T, Eigen::MatrixXd>::value) {
                    cell << matrix(r, c);
                }
                else if constexpr (std::is_same<T, std::vector<std::vector<double>>>::value) {
                    cell << matrix[r][c];
                }
                else if constexpr (std::is_same<T, std::vector<std::pair<int, int>>>::value) {
                    if (c == 0)
                        cell << matrix[r].first;
                    else
                        cell << matrix[r].second;
                }

                std::string cellStr = cell.str();
                int padding = (maxWidth - cellStr.size()) / 2;
                int extra = (maxWidth - cellStr.size()) % 2;

                oss << " " << std::string(padding, ' ') << cellStr << std::string(padding + extra, ' ') << " ";

                if (c < cols - 1) {
                    oss << grayVbar;
                }
                else {
                    oss << blueVbar;
                }
            }
            oss << "\n";

            if (showGrid && r < rows - 1) {
                oss << blueVbar << grayHline << blueVbar << "\n";
            }
        }

        oss << Hbar << "\n"; // Lower bar

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
