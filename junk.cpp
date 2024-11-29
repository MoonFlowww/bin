// toroidal loop
for (int i = 5, e = 0; e < 7; i = (i + 1) % 8, ++e) {
    std::cout << e << "  ||  " << i << std::endl;
}
