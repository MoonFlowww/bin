#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <sstream>

std::ostringstream oss;

std::string marge(const std::string& str, int totalWidth) {
    if (str.size() >= static_cast<size_t>(totalWidth)) {
        return str;
    }
    int padding = (totalWidth - static_cast<int>(str.size())) / 2;
    int extra = (totalWidth - static_cast<int>(str.size())) % 2;
    return std::string(padding, ' ') + str + std::string(padding + extra, ' ');
}

void ClassicArray(std::vector<std::string>& s, bool title = false, bool color = false, bool addSpace = false) {
    std::vector<int> widths;
    int heights = s.size();

    for (int i = 0; i < heights; ++i) widths.push_back(s[i].size());
    int total_w = *std::max_element(widths.begin(), widths.end()) + 8 + 2; // 2 '|' + 8 ' '
    std::string Hbar = "+" + std::string(total_w - 2, '-') + "+";
    std::string Vbar = "|";
    if (color) {
        Hbar.insert(0, "\033[1m\033[36m");
        Hbar += "\033[0m";
        Vbar.insert(0, "\033[1m\033[36m");
        Vbar += "\033[0m";
    }
    if (addSpace) {
        s.insert(s.begin() + 1, "  ");
        s.push_back("  ");
        heights += 2;
    }

    if (title) {
        std::string w = marge(s[0], total_w - 2);
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
        std::string w = marge(s[i], total_w - 2);
        oss << Vbar
            << "\033[1m\033[37m"
            << w
            << "\033[0m"
            << Vbar
            << "\n";
    }
    oss << Hbar << "\n";
}


void DataArray(std::vector<std::string>& s, bool title = false, bool color = false, bool addSpace = false) {
    std::vector<int> widths;
    int heights = s.size();

    for (int i = 0; i < heights; ++i) widths.push_back(s[i].size());
    int total_w = *std::max_element(widths.begin(), widths.end()) + 8 + 2; // 2 '|' + 8 ' '
    std::string Hbar = "+" + std::string(total_w - 2, '=') + "+";
    std::string Vbar = "|";
    if (color) {
        Hbar.insert(0, "\033[1m\033[36m");
        Hbar += "\033[0m";
        Vbar.insert(0, "\033[1m\033[36m");
        Vbar += "\033[0m";
    }
    if (addSpace) {
        s.insert(s.begin() + 1, "  ");
        s.push_back("  ");
        heights += 2;
    }

    if (title) {
        std::string w = marge(s[0], total_w - 2);
        oss << Hbar << "\n";
        oss << Vbar
            << "\033[1m\033[37m"
            << w
            << "\033[0m"
            << Vbar
            << "\n";
    }

    oss << Hbar << "\n";
    std::string InnerLine = "|"+std::string(total_w-2,'-')+"|"; //error
    for (int i = title ? 1 : 0; i < heights; ++i) {
        std::string w = marge(s[i], total_w - 2);
        oss << Vbar
            << "\033[1m\033[37m"
            << w
            << "\033[0m"
            << Vbar
            << "\n";
            
        if(heights-5> 0) oss << InnerLine << "\n";
    }
    oss << Hbar << "\n";
}

int main() {
    std::vector<std::string> s = { "Title", "Hello World", "C++", "LeetCode", "HomeMade" };
    //ClassicArray(s, true, true, true);
    DataArray(s, true, true, true);
    std::cout << oss.str(); // cout everything 

    return 0;
}
