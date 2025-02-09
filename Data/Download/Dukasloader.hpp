#ifndef DUKASLOADER_HPP
#define DUKASLOADER_HPP

#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <curl/curl.h>
#include <lzma.h>
#include <iostream>
#include <cstring>
#include <system_error>
#include <cmath>
#include <algorithm>

class DukascopyDownloader {
public:
    DukascopyDownloader(
        const std::string& asset,
        const std::string& start_date,
        const std::string& end_date,
        const std::string& download_dir,
        int verbose_level = 2
    ) : asset(asset), download_dir(download_dir), verbose_level(verbose_level) {
        std::cout << "Asset: " << asset << " || Range: " << start_date << " -> " << end_date << std::endl;
        std::cout << "Path: ";
        std::cout << download_dir + "/" + asset + "_ticks.csv" << std::endl;
        init_curl();
        parse_dates(start_date, end_date);
        prepare_csv();
        
    }

    ~DukascopyDownloader() {
        if (csv_file.is_open())
            csv_file.close();
        curl_global_cleanup();
    }

    void download() {
        auto current_time = start_time;
        int total_hours = std::chrono::duration_cast<std::chrono::hours>(end_time - start_time).count() + 1;
        int current_index = 0;
        size_t total_bytes_downloaded = 0;
        auto overall_start = std::chrono::steady_clock::now();

        while (current_time <= end_time) {
            std::tm tm_base = {};
            time_t t = std::chrono::system_clock::to_time_t(current_time);
            gmtime_s(&tm_base, &t);
            int year = tm_base.tm_year + 1900;
            int month = tm_base.tm_mon + 1;
            int day = tm_base.tm_mday;
            int hour = tm_base.tm_hour;
            std::ostringstream oss_url;
            oss_url << "https://datafeed.dukascopy.com/datafeed/" << asset
                << "/" << year
                << "/" << std::setw(2) << std::setfill('0') << month
                << "/" << std::setw(2) << std::setfill('0') << day
                << "/" << std::setw(2) << std::setfill('0') << hour << "h_ticks.bi5";
            std::string url = oss_url.str();

            if (verbose_level == 2)
                log("Starting download for " + url);

            auto download_start = std::chrono::steady_clock::now();
            std::vector<uint8_t> compressed_data;
            int http_code = download_file(url, compressed_data);
            if (http_code == 404) {
                if (verbose_level == 2)
                    log("Received 404 for " + url + ", skipping the entire day.");
                current_time = skip_day(current_time);
                current_index += (24 - hour);
                continue;
            }
            if (http_code != 200) {
                if (verbose_level == 2)
                    log("Download failed for " + url + " with HTTP code " + std::to_string(http_code));
                current_time += std::chrono::hours(1);
                current_index++;
                if (verbose_level == 1)
                    update_progress(current_index, total_hours, total_bytes_downloaded, overall_start);
                continue;
            }
            total_bytes_downloaded += compressed_data.size();
            auto download_end = std::chrono::steady_clock::now();
            auto download_duration = std::chrono::duration_cast<std::chrono::milliseconds>(download_end - download_start).count();
            if (verbose_level == 2)
                log("Download completed in " + std::to_string(download_duration) + " ms, size: " + std::to_string(compressed_data.size()) + " bytes");

            if (!compressed_data.empty()) {
                if (verbose_level == 2)
                    log("Starting decompression for " + url);
                std::vector<uint8_t> decompressed_data;
                auto decompress_start = std::chrono::steady_clock::now();
                if (decompress_bi5(compressed_data, decompressed_data)) {
                    auto decompress_end = std::chrono::steady_clock::now();
                    auto decompress_duration = std::chrono::duration_cast<std::chrono::milliseconds>(decompress_end - decompress_start).count();
                    if (verbose_level == 2)
                        log("Decompression completed in " + std::to_string(decompress_duration) + " ms, output size: " + std::to_string(decompressed_data.size()) + " bytes");
                    process_ticks(decompressed_data, tm_base);
                }
                else {
                    if (verbose_level == 2)
                        log("Decompression failed for " + url);
                }
            }
            else {
                if (verbose_level == 2)
                    log("Empty file downloaded for " + url);
            }
            current_time += std::chrono::hours(1);
            current_index++;
            if (verbose_level == 1)
                update_progress(current_index, total_hours, total_bytes_downloaded, overall_start);
        }
        if (verbose_level == 1)
            std::cout << std::endl;
    }

private:
    struct Tick {
        std::string timestamp;
        double ask;
        double bid;
        double ask_volume;
        double bid_volume;
    };

    std::string asset;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::string download_dir;
    int verbose_level;
    std::ofstream csv_file;

    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        auto* buffer = static_cast<std::vector<uint8_t>*>(userp);
        size_t total = size * nmemb;
        buffer->insert(buffer->end(), static_cast<uint8_t*>(contents), static_cast<uint8_t*>(contents) + total);
        return total;
    }

    void init_curl() {
        static bool initialized = false;
        if (!initialized) {
            CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
            if (res != CURLE_OK)
                throw std::runtime_error(std::string("Failed to initialize cURL: ") + curl_easy_strerror(res));
            initialized = true;
        }
    }

    void parse_dates(const std::string& start_str, const std::string& end_str) {
        start_time = parse_date(start_str);
        end_time = parse_date(end_str);
        if (start_time > end_time)
            throw std::invalid_argument("Start date must be before end date");
        end_time += std::chrono::hours(23);
    }

    std::chrono::system_clock::time_point parse_date(const std::string& date_str) {
        std::tm tm = {};
        std::istringstream ss(date_str);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail())
            throw std::invalid_argument("Invalid date format, use YYYY-MM-DD");
        tm.tm_isdst = -1;
        time_t t = _mkgmtime(&tm);
        if (t == -1)
            throw std::invalid_argument("Invalid date");
        return std::chrono::system_clock::from_time_t(t);
    }

    int download_file(const std::string& url, std::vector<uint8_t>& buffer) {
        CURL* curl = curl_easy_init();
        if (!curl)
            throw std::runtime_error("Failed to initialize cURL easy handle");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl/1.0");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
            log("Download failed: " + url + " - " + curl_easy_strerror(res));
        if (http_code != 200)
            log("Download failed: " + url + " - HTTP Error Code: " + std::to_string(http_code));
        return static_cast<int>(http_code);
    }

    std::string lzma_ret_to_string(lzma_ret ret) {
        switch (ret) {
        case LZMA_OK: return "LZMA_OK";
        case LZMA_STREAM_END: return "LZMA_STREAM_END";
        case LZMA_NO_CHECK: return "LZMA_NO_CHECK";
        case LZMA_UNSUPPORTED_CHECK: return "LZMA_UNSUPPORTED_CHECK";
        case LZMA_GET_CHECK: return "LZMA_GET_CHECK";
        case LZMA_MEM_ERROR: return "LZMA_MEM_ERROR";
        case LZMA_MEMLIMIT_ERROR: return "LZMA_MEMLIMIT_ERROR";
        case LZMA_FORMAT_ERROR: return "LZMA_FORMAT_ERROR";
        case LZMA_OPTIONS_ERROR: return "LZMA_OPTIONS_ERROR";
        case LZMA_DATA_ERROR: return "LZMA_DATA_ERROR";
        case LZMA_BUF_ERROR: return "LZMA_BUF_ERROR";
        default: return "Unknown LZMA error code: " + std::to_string(ret);
        }
    }

    bool decompress_bi5(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
        lzma_stream strm = LZMA_STREAM_INIT;
        lzma_ret ret = lzma_auto_decoder(&strm, UINT64_MAX, 0);
        if (ret != LZMA_OK) {
            log("Failed to initialize LZMA auto decoder: " + lzma_ret_to_string(ret));
            return false;
        }
        strm.next_in = input.data();
        strm.avail_in = input.size();
        output.resize(input.size() * 2);
        do {
            strm.next_out = output.data() + strm.total_out;
            strm.avail_out = output.size() - strm.total_out;
            ret = lzma_code(&strm, LZMA_FINISH);
            if (ret == LZMA_STREAM_END) {
                output.resize(strm.total_out);
                lzma_end(&strm);
                return true;
            }
            else if (ret != LZMA_OK) {
                log("LZMA auto decoding error: " + lzma_ret_to_string(ret));
                lzma_end(&strm);
                return false;
            }
            if (strm.avail_out == 0)
                output.resize(output.size() * 2);
        } while (true);
    }

    float read_be_float(const uint8_t* data) {
        uint32_t bits = (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16 | (uint32_t)data[2] << 8 | data[3];
        float value;
        std::memcpy(&value, &bits, sizeof(float));
        return value;
    }

    std::string format_tick_timestamp(const std::tm& base_tm, uint32_t offset_ms) {
        time_t base_seconds = _mkgmtime(const_cast<std::tm*>(&base_tm));
        time_t display_seconds = base_seconds + (offset_ms / 1000); // Removed +3600
        unsigned ms = offset_ms % 1000;
        std::tm display_tm;
        gmtime_s(&display_tm, &display_seconds);
        std::ostringstream oss;
        oss << std::put_time(&display_tm, "%d.%m.%Y %H:%M:%S")
            << "." << std::setw(3) << std::setfill('0') << ms
            << " GMT+0000"; // Keep it in UTC
        return oss.str();
    }


    Tick parse_tick20(const uint8_t* data, const std::tm& base_tm) {
        uint32_t offset = (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | data[3];
        uint32_t ask_raw = (uint32_t(data[4]) << 24) | (uint32_t(data[5]) << 16) | (uint32_t(data[6]) << 8) | data[7];
        uint32_t bid_raw = (uint32_t(data[8]) << 24) | (uint32_t(data[9]) << 16) | (uint32_t(data[10]) << 8) | data[11];
        float ask_vol_raw = read_be_float(data + 12);
        float bid_vol_raw = read_be_float(data + 16);

        double point = (asset == "usdrub" || asset == "xagusd" || asset == "xauusd") ? 1e3 : 1e5;
        double ask = static_cast<double>(ask_raw) / point;
        double bid = static_cast<double>(bid_raw) / point;
        double ask_volume = ask_vol_raw;
        double bid_volume = bid_vol_raw;

        std::string ts = format_tick_timestamp(base_tm, offset);
        return { ts, ask, bid, ask_volume, bid_volume };
    }

    void process_ticks(std::vector<uint8_t> data, const std::tm& base_tm) {
        while (!data.empty()) {
            std::string headerPrefix = "Timestamp";
            if (data.size() >= headerPrefix.size() &&
                std::equal(headerPrefix.begin(), headerPrefix.end(), data.begin())) {
                auto newlineIt = std::find(data.begin(), data.end(), '\n');
                if (newlineIt != data.end()) {
                    data.erase(data.begin(), newlineIt + 1);
                    continue;
                }
                break;
            }
            bool isPrintable = true;
            for (size_t i = 0; i < std::min<size_t>(data.size(), 10); ++i) {
                if (data[i] < 32 || data[i] > 126) { isPrintable = false; break; }
            }
            if (isPrintable) {
                auto newlineIt = std::find(data.begin(), data.end(), '\n');
                if (newlineIt != data.end()) {
                    data.erase(data.begin(), newlineIt + 1);
                    continue;
                }
            }
            break;
        }

        size_t valid_size = (data.size() / 20) * 20;
        if (data.size() != valid_size) {
            log("Warning: Decompressed data size (" + std::to_string(data.size()) +
                ") is not a multiple of 20 bytes. Trimming to " + std::to_string(valid_size));
            data.resize(valid_size);
        }

        size_t num_ticks = data.size() / 20;
        log("Processing " + std::to_string(num_ticks) + " ticks");
        for (size_t i = 0; i < num_ticks; ++i) {
            const uint8_t* p = data.data() + i * 20;
            uint32_t offset = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
            if (offset >= 3600000) {
                log("Skipping tick with invalid offset: " + std::to_string(offset));
                continue;
            }
            Tick tick = parse_tick20(p, base_tm);
            write_tick(tick);
        }
        log("Completed processing ticks");
    }

    std::string format_date_hour(const std::tm& tm) {
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:00:00");
        return oss.str();
    }

    std::chrono::system_clock::time_point skip_day(const std::chrono::system_clock::time_point& current) {
        std::tm tm = {};
        time_t t = std::chrono::system_clock::to_time_t(current);
        gmtime_s(&tm, &t);
        tm.tm_hour = 0;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        tm.tm_mday += 1;
        time_t next_day = _mkgmtime(&tm);
        return std::chrono::system_clock::from_time_t(next_day);
    }

    void prepare_csv() {
        std::string path = download_dir + "/" + asset + "_ticks.csv";
        csv_file.open(path, std::ios::app);
        if (!csv_file.is_open())
            throw std::runtime_error("Cannot open output file: " + path);
        if (csv_file.tellp() == 0)
            csv_file << "Timestamp,Ask,Bid,AskVolume,BidVolume\n";
    }

    void write_tick(const Tick& tick) {
        csv_file << tick.timestamp << ","
            << tick.ask << ","
            << tick.bid << ","
            << tick.ask_volume << ","
            << tick.bid_volume << "\n";
    }

    void log(const std::string& message) {
        if (verbose_level == 2)
            std::cout << message << std::endl;
    }

    void update_progress(int current, int total, size_t total_bytes, std::chrono::steady_clock::time_point overall_start) {
        double progress = static_cast<double>(current) / total;
        int bar_width = 50;
        int pos = static_cast<int>(progress * bar_width);
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - overall_start).count();
        long estimated_total = (progress > 0) ? static_cast<long>(elapsed_ms / progress) : 0;
        long remaining = (progress > 0) ? estimated_total - elapsed_ms : 0;
        std::cout << "\r[";
        for (int i = 0; i < bar_width; ++i)
            std::cout << (i < pos ? "#" : " ");
        std::cout << "] " << int(progress * 100) << "%, Remaining: " << remaining << " ms, Total Downloaded: " << total_bytes << " bytes" << std::flush;
    }
};

#endif // DUKASLOADER_HPP






/* 
- .bi5 are not saved on the disk
- every things goes in a csvfile with UTC
- Volume seem stuck v[n] == v[n-1] := when no volume but price mouvement
- all data come in 1 tick AskP, BidP, AskV, BidV

/!\ seem to not erase old files and stay stuck on the oldest

#include "Dukasloader.hpp"

int main() {
    DukascopyDownloader downloader("EURUSD", "2023-01-01", "2023-01-10", "C:/Users/PC/Downloads/Assets_Data/Forex", 1);
    downloader.download();
    return 0;
}

*/
