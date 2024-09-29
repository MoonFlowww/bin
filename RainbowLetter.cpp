#include <iostream>
#include <string>
#include <thread>
#include <chrono>

void dynamic(int n, std::string& sentence, std::string& p1);

int main() {
	std::string sentence = "hellow world";
	std::string p1;
	auto t1 = std::chrono::high_resolution_clock::now();
	dynamic(0, sentence, p1);
	auto t2 = std::chrono::high_resolution_clock::now();
	auto cd = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

	int r = 0;
	for (auto& x : sentence) r += (x - 'a' + 1);
	r *= 25;

	std::cout << "cd : " << cd.count() << std::endl;
	std::cout << "r : " <<r << std::endl;
	std::cout << "Latency : " << cd.count() - r << std::endl;
	return 0;
}

void dynamic(int n, std::string& sentence, std::string& p1) {
	
	
	for (char i = 'a'; i <= 'z'; ++i) { // 'z'+1 = '{'
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
		std::cout << p1 << i << std::endl;
		if (i == sentence[n]) break;
	}
	p1.push_back(sentence[n]); //std::string p1 = sentence.substr(0, n);
	if (sentence.size()-1 != n) dynamic(++n, sentence, p1);
	
}
