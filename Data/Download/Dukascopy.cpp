#include "Dukasloader.hpp"

int main() {
    DukascopyDownloader downloader("BTCUSD", "2023-01-01", "2025-03-19", "", "DOWNLOAD_PATH", "POSTGRE_URL", 1); // 1: Progress Bar, 2: Verbose
    downloader.download();
    return 0;
}



//postgresql://postgres:Monarch@localhost:5432/PallasDB
