#ifndef DUKASLOADER_HPP
#define DUKASLOADER_HPP

#include <system_error>
#define NOMINMAX // curl include <windows.h> which use Min Max macro
#include <curl/curl.h>
#include <filesystem>
#include <libpq-fe.h>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <string>
#include <vector>
#include <limits>
#include <chrono>
#include <lzma.h>
#include <ctime>
#include <cmath>
#include <regex>
#include <thread>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

class DukascopyDownloader {
public:
    DukascopyDownloader(
        const std::string& asset,
        const std::string& start_date,
        const std::string& end_date,
        const std::string& download_dir = "",
        const std::string& timeframe = "",
        const std::string& pg_url = "",
        int verbose_level = 2
    ) : asset(asset), download_dir(download_dir), verbose_level(verbose_level), pg_url(pg_url)
    {
        if (!timeframe.empty()) {
            parse_timeframe(timeframe);
            aggregation_enabled = true;
        }
        else {
            aggregation_enabled = false;
        }
        std::cout << "Asset: " << asset << " || Range: " << start_date << " -> " << end_date << std::endl;
        init_curl();
        parse_dates(start_date, end_date);
        try {
            double point = determine_scaling(asset);
            std::cout << "Asset: " << asset << ", Point: " << point << std::endl;
        }
        catch (const std::invalid_argument& e) {
            std::cerr << e.what() << std::endl;
        }

        bool update_mode = false;
        bool livestream_mode = false;
        std::chrono::system_clock::time_point last_data_point;
        bool use_last_data_point = false;

        if (!pg_url.empty()) {
            connect_to_postgres();

            {
                std::string set_timezone = "SET TIME ZONE 'UTC';";
                PGresult* res = PQexec(pg_conn, set_timezone.c_str());
                PQclear(res);
            }

            std::string table_name = sanitize_identifier(asset) +
                (aggregation_enabled ? "_" + sanitize_identifier(timeframe) : "_tickdata");

            std::string check_sql = "SELECT COUNT(*) FROM \"" + table_name + "\";";
            PGresult* res = PQexec(pg_conn, check_sql.c_str());
            bool table_exists = false;
            if (PQresultStatus(res) == PGRES_TUPLES_OK) {
                std::string count_str = PQgetvalue(res, 0, 0);
                table_exists = (std::stoi(count_str) > 0);
            }
            PQclear(res);

            if (table_exists) {
                std::cout << "Table \"" << table_name << "\" already exists. Update (u) or Restart (r)? ";
                char response;
                std::cin >> response;

                if (response == 'u' || response == 'U') {
                    update_mode = true;
                    
                    std::string last_sql = aggregation_enabled
                        ? "SELECT to_char(max(interval_start) AT TIME ZONE 'UTC', 'YYYY-MM-DD HH24:MI:SS.US TZH') FROM \"" + table_name + "\";"
                        : "SELECT to_char(max(timestamp) AT TIME ZONE 'UTC', 'YYYY-MM-DD HH24:MI:SS.US TZH') FROM \"" + table_name + "\";";

                    PGresult* res_last = PQexec(pg_conn, last_sql.c_str());
                    if (PQresultStatus(res_last) == PGRES_TUPLES_OK) {
                        char* last_ts = PQgetvalue(res_last, 0, 0);
                        if (last_ts && !std::string(last_ts).empty()) {
                            std::tm tm = {};
                            std::istringstream ss(last_ts);
                            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S.%3NZ");
                            if (!ss.fail()) {
                                last_data_point = std::chrono::system_clock::from_time_t(_mkgmtime(&tm));
                                
                                std::cout << "Last data point: " << last_ts << std::endl;
                                std::cout << "Download from last data point to: End date (e), Now (n)? ";
                                char update_option;
                                std::cin >> update_option;
                                
                                if (update_option == 'n' || update_option == 'N') {
                                    end_time = std::chrono::system_clock::now();
                                    
                                    std::cout << "Download mode: Fixed point (f), Livestream (l)? ";
                                    char stream_option;
                                    std::cin >> stream_option;
                                    
                                    if (stream_option == 'l' || stream_option == 'L') {
                                        livestream_mode = true;
                                    }
                                }
                                
                                start_time = last_data_point + std::chrono::hours(1);
                                use_last_data_point = true;
                                
                                if (verbose_level == 1)
                                    std::cout << "Resuming from: " << last_ts << std::endl;
                            }
                        }
                    }
                    PQclear(res_last);
                }
                else {
                    std::string drop_sql = "DROP TABLE IF EXISTS \"" + table_name + "\";";
                    PQexec(pg_conn, drop_sql.c_str());
                    create_table();
                }
            }
            else {
                create_table();
            }
            if (verbose_level == 1)
                std::cout << "PostgreSQL ready for: " << table_name << std::endl;
        }

        // CSV handling
        if (!download_dir.empty()) {
            std::string csv_path = getOutputFilePath(download_dir, asset, aggregation_enabled);
            if (std::filesystem::exists(csv_path)) {
                std::cout << "CSV file " << csv_path << " already exists. Update (u) or Restart (r)? ";
                char response;
                std::cin >> response;
                if (response == 'u' || response == 'U') {
                    update_mode = true;
                    std::ifstream infile(csv_path);
                    std::string line, last;
                    while (std::getline(infile, line)) {
                        if (!line.empty())
                            last = line;
                    }
                    if (!last.empty()) {
                        std::istringstream iss(last);
                        std::string ts;
                        if (std::getline(iss, ts, ',')) {
                            std::tm tm = {};
                            std::istringstream ss(ts);
                            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                            if (!ss.fail()) {
                                last_data_point = std::chrono::system_clock::from_time_t(_mkgmtime(&tm));
                                
                                if (!use_last_data_point) {
                                    std::cout << "Last data point: " << ts << std::endl;
                                    std::cout << "Download from last data point to: End date (e), Now (n)? ";
                                    char update_option;
                                    std::cin >> update_option;
                                    
                                    if (update_option == 'n' || update_option == 'N') {
                                        end_time = std::chrono::system_clock::now();
                                        
                                        std::cout << "Download mode: Fixed point (f), Livestream (l)? ";
                                        char stream_option;
                                        std::cin >> stream_option;
                                        
                                        if (stream_option == 'l' || stream_option == 'L') {
                                            livestream_mode = true;
                                        }
                                    }
                                }
                                
                                start_time = last_data_point + std::chrono::hours(1);
                                
                                if (verbose_level == 1)
                                    std::cout << "Updated start date from CSV: " << ts << std::endl;
                            }
                        }
                    }
                }
                else {
                    std::error_code ec;
                    std::filesystem::remove(csv_path, ec);
                }
            }
            prepare_csv();
        }
        
        this->livestream_mode = livestream_mode;
    }
    ~DukascopyDownloader() {
        if (csv_file.is_open())
            csv_file.close();
        if (pg_conn) {
            PQfinish(pg_conn);
            pg_conn = nullptr;
        }
        curl_global_cleanup();
    }
    double determine_scaling(const std::string& asset) {
        if (asset == "BTCUSD")
            return 1e4;
        else if (asset == "USDRUB" || asset == "XAGUSD" || asset == "XAUUSD")
            return 1e3;
        else if (asset == "EURUSD" || asset == "GBPUSD" || asset == "AUDUSD" || asset == "NZDUSD")
            return 1e5;
        else if (asset == "USDJPY" || asset == "EURJPY" || asset == "GBPJPY" || asset == "AUDJPY" || asset == "NZDJPY")
            return 1e2;
        else if (asset == "SPX500" || asset == "NAS100" || asset == "UK100" || asset == "GER30" || asset == "FRA40")
            return 1e1;
        else if (asset == "BRENT" || asset == "WTI")
            return 1e2;
        else {
            std::cerr << "ERROR: Unknown asset: " + asset << std::endl;
            throw std::invalid_argument("Unknown asset: " + asset + "\n    -> Unknown magnitude for tick translation from bi5(full int) to double\n      -> Line ~633");
            return -10; // unreachable
        }
    }



    std::string getOutputFilePath(const std::string& downloadDir, const std::string& asset, bool aggregationEnabled) {
        return downloadDir + "/" + asset + (aggregationEnabled ? "_aggregated.csv" : "_ticks.csv");
    }

    bool handleExistingFile(const std::string& downloadDir, const std::string& asset, bool aggregationEnabled) {
        std::string outputFile = getOutputFilePath(downloadDir, asset, aggregationEnabled);
        if (std::filesystem::exists(outputFile)) {
            std::cout << "File " << outputFile << " already exists. Remove it? (y/n): ";
            char response;
            std::cin >> response;
            if (response == 'y' || response == 'Y') {
                std::error_code ec;
                if (std::filesystem::remove(outputFile, ec))
                    return true;
                else {
                    std::cout << "Failed to remove file: " << ec.message() << std::endl;
                    return false;
                }
            }
            return false;
        }
        return true;
    }
    void download() {
        auto current_time = start_time;
        int total_hours = std::chrono::duration_cast<std::chrono::hours>(end_time - start_time).count();
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
            oss_url << "https://datafeed.dukascopy.com/datafeed/" << asset << "/"
                << std::put_time(&tm_base, "%Y/")
                << std::setw(2) << std::setfill('0') << tm_base.tm_mon << "/"
                << std::put_time(&tm_base, "%d/")
                << std::put_time(&tm_base, "%H") << "h_ticks.bi5";
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
            
            if (livestream_mode && (current_time >= end_time)) {
                std::cout << "\nCaught up to current time. Waiting for new data..." << std::endl;
                std::this_thread::sleep_for(std::chrono::minutes(1));
                
                end_time = std::chrono::system_clock::now();
                
                if (_kbhit()) {
                    char ch = _getch();
                    if (ch == 'q' || ch == 'Q') {
                        std::cout << "\nLivestream stopped by user." << std::endl;
                        break;
                    }
                }
                
                total_hours = std::chrono::duration_cast<std::chrono::hours>(end_time - start_time).count();
            }
        }
        if (has_current_bar) {
            write_aggregated_bar(current_bar);
            has_current_bar = false;
        }
        if (verbose_level == 1)
            std::cout << std::endl;
    }

private:
    struct Tick {
        std::string timestamp;
        std::chrono::system_clock::time_point timepoint;
        double ask;
        double bid;
        double ask_volume;
        double bid_volume;
    };

    struct AggregatedBar {
        std::chrono::system_clock::time_point interval_start;
        double open_ask;
        double open_bid;
        double high_ask;
        double low_ask;
        double close_ask;
        double close_bid;
        double high_bid;
        double low_bid;
        double total_ask_volume;
        double total_bid_volume;
    };

    std::string asset;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::string download_dir;
    int verbose_level;
    std::ofstream csv_file;
    std::chrono::seconds aggregation_interval;
    bool aggregation_enabled = false;
    std::string timeframe;
    PGconn* pg_conn = nullptr;
    std::string pg_url;
    AggregatedBar current_bar;
    bool has_current_bar = false;
    bool livestream_mode = false;

    void parse_timeframe(const std::string& tf) {
        std::regex pattern(R"(^(\d+)([smhd])$)");
        std::smatch matches;
        if (!std::regex_match(tf, matches, pattern))
            throw std::invalid_argument("Invalid timeframe format. Use formats like '30s', '45m', '2h', '1d'");
        int value = std::stoi(matches[1]);
        char unit = matches[2].str()[0];
        switch (unit) {
        case 's': aggregation_interval = std::chrono::seconds(value); break;
        case 'm': aggregation_interval = std::chrono::minutes(value); break;
        case 'h': aggregation_interval = std::chrono::hours(value); break;
        case 'd': aggregation_interval = std::chrono::hours(value * 24); break;
        default:
            throw std::invalid_argument("Invalid timeframe unit. Use 's', 'm', 'h', or 'd'");
        }
        timeframe = tf;
    }



    void connect_to_postgres() {
        pg_conn = PQconnectdb(pg_url.c_str());
        if (PQstatus(pg_conn) != CONNECTION_OK)
            throw std::runtime_error("PostgreSQL connection failed: " + std::string(PQerrorMessage(pg_conn)));
        if (verbose_level == 1)
            std::cout << "Connected to PostgreSQL at: " << pg_url << std::endl;
    }

    std::string sanitize_identifier(const std::string& identifier) {
        std::string sanitized;
        for (char c : identifier)
            sanitized += (isalnum(c) || c == '_') ? c : '_';
        return sanitized;
    }

    void create_table() {
        std::string table_name;
        std::string sanitized_asset = sanitize_identifier(asset);
        if (aggregation_enabled)
            table_name = sanitized_asset + "_" + sanitize_identifier(timeframe);
        else
            table_name = sanitized_asset + "_tickdata";
        std::string create_sql;
        if (aggregation_enabled) {
            create_sql = "CREATE TABLE IF NOT EXISTS \"" + table_name + "\" ("
                "interval_start TIMESTAMP(3) WITHOUT TIME ZONE PRIMARY KEY,"
                "open_ask DOUBLE PRECISION,"
                "high_ask DOUBLE PRECISION,"
                "low_ask DOUBLE PRECISION,"
                "close_ask DOUBLE PRECISION,"
                "open_bid DOUBLE PRECISION,"
                "high_bid DOUBLE PRECISION,"
                "low_bid DOUBLE PRECISION,"
                "close_bid DOUBLE PRECISION,"
                "total_ask_volume DOUBLE PRECISION,"
                "total_bid_volume DOUBLE PRECISION);";
        }
        else {
            create_sql = "CREATE TABLE IF NOT EXISTS \"" + table_name + "\" ("
                "timestamp TIMESTAMP(3) WITHOUT TIME ZONE,"
                "ask DOUBLE PRECISION,"
                "bid DOUBLE PRECISION,"
                "ask_volume DOUBLE PRECISION,"
                "bid_volume DOUBLE PRECISION);";
        }
        PGresult* res = PQexec(pg_conn, create_sql.c_str());
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            PQclear(res);
            throw std::runtime_error("Failed to create table: " + std::string(PQerrorMessage(pg_conn)));
        }
        PQclear(res);
        if (verbose_level == 1)
            std::cout << "Table \"" << table_name << "\" is ready." << std::endl;
    }

    void write_aggregated_bar(const AggregatedBar& bar) {
        std::tm time_tm;
        if (csv_file.is_open()) {
            std::time_t t = std::chrono::system_clock::to_time_t(bar.interval_start);
            gmtime_s(&time_tm, &t);
            csv_file << std::put_time(&time_tm, "%Y-%m-%d %H:%M:%S") << ","
                << bar.open_ask << "," << bar.high_ask << "," << bar.low_ask << "," << bar.close_ask << ","
                << bar.open_bid << "," << bar.high_bid << "," << bar.low_bid << "," << bar.close_bid << ","
                << bar.total_ask_volume << "," << bar.total_bid_volume << "\n";
        }
        if (pg_conn) {
            std::string table_name = sanitize_identifier(asset) + (aggregation_enabled ? "_" + sanitize_identifier(timeframe) : "_tickdata");
            std::ostringstream oss;
            oss << "INSERT INTO \"" << table_name << "\" VALUES ("
                << "'" << std::put_time(&time_tm, "%Y-%m-%d %H:%M:%S") << "',"
                << bar.open_ask << "," << bar.high_ask << "," << bar.low_ask << "," << bar.close_ask << ","
                << bar.open_bid << "," << bar.high_bid << "," << bar.low_bid << "," << bar.close_bid << ","
                << bar.total_ask_volume << "," << bar.total_bid_volume << ");";
            PGresult* res = PQexec(pg_conn, oss.str().c_str());
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                PQclear(res);
                throw std::runtime_error("Failed to insert bar: " + std::string(PQerrorMessage(pg_conn)));
            }
            PQclear(res);
        }
    }

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


    std::chrono::system_clock::time_point parse_date(const std::string& date_str) {
        std::tm tm = {};
        std::istringstream ss(date_str);
        ss.imbue(std::locale::classic());
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail())
            throw std::invalid_argument("Invalid date format, use YYYY-MM-DD");
        tm.tm_isdst = -1;
        time_t t = _mkgmtime(&tm);
        if (t == -1)
            throw std::invalid_argument("Invalid date");
        return std::chrono::system_clock::from_time_t(t);
    }

    void parse_dates(const std::string& start_date, const std::string& end_date) {
        start_time = parse_date(start_date);
        end_time = parse_date(end_date);
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
        time_t display_seconds = base_seconds + (offset_ms / 1000);
        unsigned ms = offset_ms % 1000;
        std::tm display_tm;
        gmtime_s(&display_tm, &display_seconds);
        std::ostringstream oss;
        oss << std::put_time(&display_tm, "%Y-%m-%d %H:%M:%S")
            << "." << std::setw(3) << std::setfill('0') << ms;
        return oss.str();
    }


    Tick parse_tick20(const uint8_t* data, const std::tm& base_tm) {
        uint32_t offset = (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | data[3];
        uint32_t ask_raw = (uint32_t(data[4]) << 24) | (uint32_t(data[5]) << 16) | (uint32_t(data[6]) << 8) | data[7];
        uint32_t bid_raw = (uint32_t(data[8]) << 24) | (uint32_t(data[9]) << 16) | (uint32_t(data[10]) << 8) | data[11];
        float ask_vol_raw = read_be_float(data + 12);
        float bid_vol_raw = read_be_float(data + 16);
        double point = determine_scaling(asset); // TODO: do it automatically

        double ask = static_cast<double>(ask_raw) / point;
        double bid = static_cast<double>(bid_raw) / point;

        double ask_volume = ask_vol_raw;
        double bid_volume = bid_vol_raw;
        std::string ts = format_tick_timestamp(base_tm, offset);
        time_t base_seconds = _mkgmtime(const_cast<std::tm*>(&base_tm));
        auto base_timepoint = std::chrono::system_clock::from_time_t(base_seconds);
        auto timepoint = base_timepoint + std::chrono::milliseconds(offset);
        return { ts, timepoint, ask, bid, ask_volume, bid_volume };
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
            if (aggregation_enabled) {
                auto aggregation_duration = aggregation_interval;
                auto remainder = tick.timepoint.time_since_epoch() % aggregation_duration;
                auto interval_start = tick.timepoint - remainder;
                if (has_current_bar) {
                    if (interval_start != current_bar.interval_start) {
                        write_aggregated_bar(current_bar);
                        has_current_bar = false;
                    }
                }
                if (!has_current_bar) {
                    current_bar.interval_start = interval_start;
                    current_bar.open_ask = tick.ask;
                    current_bar.open_bid = tick.bid;
                    current_bar.high_ask = tick.ask;
                    current_bar.low_ask = tick.ask;
                    current_bar.close_ask = tick.ask;
                    current_bar.high_bid = tick.bid;
                    current_bar.low_bid = tick.bid;
                    current_bar.close_bid = tick.bid;
                    current_bar.total_ask_volume = tick.ask_volume;
                    current_bar.total_bid_volume = tick.bid_volume;
                    has_current_bar = true;
                }
                else {
                    current_bar.high_ask = std::max(current_bar.high_ask, tick.ask);
                    current_bar.low_ask = std::min(current_bar.low_ask, tick.ask);
                    current_bar.close_ask = tick.ask;
                    current_bar.high_bid = std::max(current_bar.high_bid, tick.bid);
                    current_bar.low_bid = std::min(current_bar.low_bid, tick.bid);
                    current_bar.close_bid = tick.bid;
                    current_bar.total_ask_volume += tick.ask_volume;
                    current_bar.total_bid_volume += tick.bid_volume;
                }
            }
            else {
                write_tick(tick);
            }
        }
        if (has_current_bar) {
            write_aggregated_bar(current_bar);
        }
        log("Completed processing ticks");
    }

    std::string format_date_hour(const std::tm& tm) {
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:00:00");
        return oss.str();
    }

    std::chrono::system_clock::time_point skip_day(const std::chrono::system_clock::time_point& current) {
        std::time_t t = std::chrono::system_clock::to_time_t(current);
        std::tm tm;
        gmtime_s(&tm, &t);
        tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
        tm.tm_mday += 1;
        time_t next_day = _mkgmtime(&tm);
        return std::chrono::system_clock::from_time_t(next_day);
    }


    void prepare_csv() {
        std::string path;
        if (aggregation_enabled) {
            path = download_dir + "/" + asset + "_aggregated.csv";
        } else {
            path = download_dir + "/" + asset + "_ticks.csv";
        }
        
        // Ensure directory exists
        std::filesystem::path dir = std::filesystem::path(path).parent_path();
        std::error_code ec;
        if (!dir.empty() && !std::filesystem::exists(dir, ec)) {
            std::filesystem::create_directories(dir, ec);
        }
        
        csv_file.open(path, std::ios::app);
        if (!csv_file.is_open())
            throw std::runtime_error("Cannot open output file: " + path);
        if (csv_file.tellp() == 0) {
            if (aggregation_enabled)
                csv_file << "Timestamp,OpenAsk,HighAsk,LowAsk,CloseAsk,OpenBid,HighBid,LowBid,CloseBid,TotalAskVolume,TotalBidVolume\n";
            else 
                csv_file << "Timestamp,Ask,Bid,AskVolume,BidVolume\n";
        }
    }

    /*
    void prepare_csv() {
        std::string path = download_dir + "/" + asset + (aggregation_enabled ? "_aggregated.csv" : "_ticks.csv");
        csv_file.open(path, std::ios::app);
        if (!csv_file.is_open())
            throw std::runtime_error("Cannot open output file: " + path);
        if (csv_file.tellp() == 0) {
            if (aggregation_enabled)
                csv_file << "Timestamp,OpenAsk,HighAsk,LowAsk,CloseAsk,OpenBid,HighBid,LowBid,CloseBid,TotalAskVolume,TotalBidVolume\n";
            else
                csv_file << "Timestamp,Ask,Bid,AskVolume,BidVolume\n";
        }
    }
    
    */
    

    void write_tick(const Tick& tick) {
        csv_file << tick.timestamp << "," << tick.ask << "," << tick.bid << ","
            << tick.ask_volume << "," << tick.bid_volume << "\n";
        if (pg_conn) {
            std::string table_name = sanitize_identifier(asset) + "_tickdata";
            std::ostringstream oss;
            oss << "INSERT INTO \"" << table_name << "\" VALUES ("
                << '\'' << tick.timestamp << "',"
                << tick.ask << ","
                << tick.bid << ","
                << tick.ask_volume << ","
                << tick.bid_volume << ");";
            PGresult* res = PQexec(pg_conn, oss.str().c_str());
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                PQclear(res);

                throw std::runtime_error("Failed to insert tick: " + std::string(PQerrorMessage(pg_conn)));
            }
            PQclear(res);
        }
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
        long remaining_ms = (progress > 0) ? estimated_total - elapsed_ms : 0;
        std::string time_remaining;
        if (remaining_ms < 1000)
            time_remaining = std::to_string(remaining_ms) + " ms";
        else if (remaining_ms < 60000)
            time_remaining = std::to_string(remaining_ms / 1000.0) + " sec";
        else {
            long minutes = remaining_ms / 60000;
            long seconds = (remaining_ms % 60000) / 1000;
            time_remaining = std::to_string(minutes) + " min " + std::to_string(seconds) + " sec";
        }
        std::cout << "\r[";
        for (int i = 0; i < bar_width; ++i)
            std::cout << (i < pos ? "#" : " ");
        std::cout << "] " << int(progress * 100) << "%, Remaining: " << time_remaining
            << ", Total Downloaded: " << total_bytes << " bytes" << std::flush;
    }

#ifndef _WIN32
    // Cross-platform equivalent of _kbhit for non-Windows systems
    bool _kbhit() {
        struct termios oldt, newt;
        int ch;
        int oldf;
        
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
        
        ch = getchar();
        
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, oldf);
        
        if(ch != EOF) {
            ungetc(ch, stdin);
            return true;
        }
        
        return false;
    }
    
    // Cross-platform equivalent of _getch for non-Windows systems
    char _getch() {
        char buf = 0;
        struct termios old = {0};
        
        if (tcgetattr(0, &old) < 0)
            return 0;
            
        old.c_lflag &= ~ICANON;
        old.c_lflag &= ~ECHO;
        old.c_cc[VMIN] = 1;
        old.c_cc[VTIME] = 0;
        
        if (tcsetattr(0, TCSANOW, &old) < 0)
            return 0;
            
        if (read(0, &buf, 1) < 0)
            return 0;
            
        old.c_lflag |= ICANON;
        old.c_lflag |= ECHO;
        
        if (tcsetattr(0, TCSADRAIN, &old) < 0)
            return 0;
            
        return buf;
    }
#endif

};

#endif // DUKASLOADER_HPP


/*
#include "Dukasloader.hpp"

int main() {
    DukascopyDownloader downloader("EURUSD", "2023-01-01", "2023-01-10", "PATH", "30s", "POSTGRE_URL", 1); // 1: Progress Bar, 2: Verbose
    downloader.download();
    return 0;
}

*/
