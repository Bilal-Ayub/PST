#include "CSVParser.h"
#include <fstream>
#include <algorithm>

// splits a csv line into fields
static std::vector<std::string> splitCSVLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;

    for (char c : line) {
        if (c == '"') {
            // toggle quote state; do not include the quote character
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            // unquoted comma = field separator
            fields.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    fields.push_back(current);
    return fields;
}

static double parseNumber(const std::string& s) {
    std::string clean;
    for (char c : s) {
        if (c != ',') {
            clean += c;
        }
    }
    return std::stod(clean);
}

std::vector<DayRecord> parseCSV(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;

    // skip the header row: Date,Open,High,Low,Close,Volume,Change
    std::getline(file, line);

    // read all data rows and extract date + close price
    std::vector<std::pair<std::string, double>> rawData;

    while (std::getline(file, line)) {
        // strip trailing carriage return (\r from windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // skip empty lines (e.g., trailing newline at end of file)
        if (line.empty()) {
            continue;
        }

        std::vector<std::string> fields = splitCSVLine(line);

        // we need at least 5 columns: Date(0), Open(1), High(2), Low(3), Close(4)
        if (fields.size() < 5) {
            continue;
        }

        std::string date = fields[0];
        double close = parseNumber(fields[4]);
        rawData.push_back({date, close});
    }

    // the csv is in reverse chronological order (newest first).
    // reverse it so index 0 = oldest trading day.
    std::reverse(rawData.begin(), rawData.end());

    // compute daily percentage returns.
    // return[i] = ((close[i] - close[i-1]) / close[i-1]) * 100.0
    
    std::vector<DayRecord> records;
    for (int i = 1; i < (int)rawData.size(); i++) {
        double prevClose = rawData[i - 1].second;
        double currClose = rawData[i].second;
        double ret = ((currClose - prevClose) / prevClose) * 100.0;
        records.push_back({rawData[i].first, currClose, ret});
    }

    return records;
}
