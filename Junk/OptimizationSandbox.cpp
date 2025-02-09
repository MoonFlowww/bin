#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include <string>
#include <cmath>


std::ostringstream oss;

std::string marge(const std::string& str, int totalWidth) {
    if (str.size() >= static_cast<size_t>(totalWidth)) {
        return str;
    }
    int padding = (totalWidth - static_cast<int>(str.size())) / 2;
    int extra = (totalWidth - static_cast<int>(str.size())) % 2;
    return std::string(padding, ' ') + str + std::string(padding + extra, ' ');
}

void array(std::vector<std::string>& s, bool title = false, bool color = false, bool addSpace = false, bool AddWords = false, std::string b = "") {
    std::vector<int> widths;
    int heights = s.size();
    if (AddWords) {
        if (!b.empty() && b[0] != ' ') b.insert(0, " ");
        for (auto& x : s) x.append(b);
    }
    
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


void opti(std::vector<int> v, int set) {
    std::vector<std::string> result = { "Latency test (+/-)"};
    {
        auto t1 = std::chrono::high_resolution_clock::now();
        for (auto& x : v) std::abs(x);
        auto t2 = std::chrono::high_resolution_clock::now();
        std::cout << "Last result from abs() : " << abs(v[0]) << std::endl;

        result.push_back("abs(x) : " + std::to_string((std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()) / set));
    }


    {
        auto t1 = std::chrono::high_resolution_clock::now();
        for (auto& x : v) std::fabs(x);
        auto t2 = std::chrono::high_resolution_clock::now();
        std::cout << "Last result from std::fabs(x) : " << std::fabs(v[0]) << std::endl;
        result.push_back("std::fabs(x) : " + std::to_string((std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()) / set));
    }

    {
        auto t1 = std::chrono::high_resolution_clock::now();
        for (auto& x : v) std::copysign(x, 1.0);
        auto t2 = std::chrono::high_resolution_clock::now();
        std::cout << "Last result from std::copysign(x, 1.0) : " << std::copysign(v[0], 1.0) << std::endl;
        result.push_back("std::copysign(x, 1.0) : " + std::to_string((std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()) / set));
    }
    


    {
        auto t1 = std::chrono::high_resolution_clock::now();
        for (auto& x : v) (unsigned)x;
        auto t2 = std::chrono::high_resolution_clock::now();
        std::cout << "Last result from unsigned : " << (unsigned)v[0] << std::endl;
        result.push_back("(unsigned)x :" + std::to_string((std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()) / set));
    }





    {
        auto t1 = std::chrono::high_resolution_clock::now();
        for (auto& x : v) x * -1;
        auto t2 = std::chrono::high_resolution_clock::now();
        std::cout << "Last result from *-1 : " << v[0] * -1 << std::endl;
        result.push_back("x*-1 : " + std::to_string((std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()) / set));
    }
    
    

    {
        auto t1 = std::chrono::high_resolution_clock::now();
        for (auto& x : v) -x;
        auto t2 = std::chrono::high_resolution_clock::now();
        std::cout << "Last result from -x : " << -v[0] << std::endl;
        result.push_back("-x : " + std::to_string((std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()) / set));
    }
    
    std::cout << "\n\n\n\n";
    array(result, true, true, true, true, "ns");
}


int main() {
    int set = 100;
	std::vector<int> v(set, -5);
    opti(v, set);
    std::cout << oss.str() << std::endl;
	return 0;
}

