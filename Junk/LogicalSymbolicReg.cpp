#include <iostream>
#include <vector>
#include <algorithm>
#include <climits>

void OppPermut(const std::vector<char>&opps, std::vector<char>&currOpp, int depth, int maxDepth,
    const std::vector<std::vector<int>>&xsp, int target, double&correct, double&incorrect) {
    if (depth == maxDepth) {
        for (const auto& num : xsp) {
            int result = num[0];
            for (size_t i = 0; i < currOpp.size(); ++i) {
                switch (currOpp[i]) {
                case '+':
                    result += num[i + 1];
                    break;
                case '-':
                    result -= num[i + 1];
                    break;
                case '*':
                    result *= num[i + 1];
                    break;
                case '/':
                    if (num[i + 1] != 0) {
                        result /= num[i + 1];
                    }
                    else {
                        result = INT_MAX;
                    }
                    break;
                }
            }
            if (result == target) {
                ++correct;
                for (size_t i = 0; i < currOpp.size(); ++i) {
                    std::cout << num[i] << " " << currOpp[i] << " ";
                }
                std::cout << num.back() << " = " << target << std::endl;
            }
            else {
                ++incorrect;
            }
        }
        return;
    }

    for (auto& opp : opps) {
        currOpp.push_back(opp);
        OppPermut(opps, currOpp, depth + 1, maxDepth, xsp, target, correct, incorrect);
        currOpp.pop_back();
    }
}

int main() {
    std::vector<int> xs = { 1, 1, 3, 6 };
    std::vector<char> opps = { '+', '-', '*', '/' };
    std::vector<std::vector<int>> xsp;
    do {
        xsp.push_back(xs);
    } while (std::next_permutation(xs.begin(), xs.end()));

    double correct = 0.0, incorrect = 0.0;
    std::vector<char> currOpp;
    OppPermut(opps, currOpp, 0, 3, xsp, 24, correct, incorrect);

    std::cout << "Correct Answers: " << correct << std::endl;
    std::cout << "Incorrect Answers: " << incorrect << std::endl;
    double p = correct / incorrect;
    std::cout << "Prob: " << p*100 << "%" << std::endl;

    return 0;
}
