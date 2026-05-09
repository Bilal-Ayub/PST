#include "DataLoader.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cctype>

using namespace std;

// ── proper RFC-4180 quoted CSV tokeniser ─────────────────────────────────────
static vector<string> splitCSV(const string& line) {
    vector<string> fields;
    string cur;
    bool inQuote = false;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '"') {
            inQuote = !inQuote;          // toggle quote mode
        } else if (c == ',' && !inQuote) {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    fields.push_back(cur);
    return fields;
}

// Strip commas (thousands separators) and convert to double
double DataLoader::parseNumber(const string& s) {
    string clean;
    for (char c : s)
        if (c != ',') clean += c;
    if (clean.empty()) return 0.0;
    try { return stod(clean); } catch (...) { return 0.0; }
}

int DataLoader::monthNum(const string& mon) {
    static const char* names[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    for (int i = 0; i < 12; i++)
        if (mon == names[i]) return i + 1;
    return 0;
}

// "DD-Mon-YY" → (year, month, day)
bool DataLoader::parseDate(const string& s, int& y, int& m, int& d) {
    if (s.size() < 9) return false;
    try {
        d = stoi(s.substr(0, 2));
        m = monthNum(s.substr(3, 3));
        int yy = stoi(s.substr(7, 2));
        y = (yy <= 30) ? 2000 + yy : 1900 + yy;
        return (m > 0 && d > 0);
    } catch (...) { return false; }
}

bool DataLoader::load(const string& filename) {
    ifstream f(filename);
    if (!f.is_open()) return false;

    string line;
    if (!getline(f, line)) return false;   // skip header

    vector<DayData> raw;
    while (getline(f, line)) {
        // Strip \r\n
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        if (line.empty()) continue;

        auto cols = splitCSV(line);
        if (cols.size() < 7) continue;

        DayData d;
        d.date_str = cols[0];
        if (!parseDate(d.date_str, d.year, d.month, d.day)) continue;

        d.open   = parseNumber(cols[1]);
        d.high   = parseNumber(cols[2]);
        d.low    = parseNumber(cols[3]);
        d.close  = parseNumber(cols[4]);
        d.volume = parseNumber(cols[5]);
        try { d.change = stod(cols[6]); } catch (...) { d.change = 0.0; }
        d.pct_change = 0.0;

        // Skip rows where close == 0 (bad/missing data)
        if (d.close < 1.0) continue;

        raw.push_back(d);
    }
    if (raw.empty()) return false;

    // Sort chronologically — oldest first
    sort(raw.begin(), raw.end(), [](const DayData& a, const DayData& b) {
        if (a.year  != b.year)  return a.year  < b.year;
        if (a.month != b.month) return a.month < b.month;
        return a.day < b.day;
    });

    // Compute pct_change: Change / PrevClose * 100
    // PrevClose = Close - Change  (since Change = Close - PrevClose)
    raw[0].pct_change = 0.0;
    for (int i = 1; i < (int)raw.size(); i++) {
        double prev = raw[i].close - raw[i].change;
        raw[i].pct_change = (fabs(prev) > 1e-6) ? (raw[i].change / prev) * 100.0 : 0.0;
    }

    days = move(raw);
    return true;
}

int DataLoader::lowerBound(int year, int month, int day) const {
    int lo = 0, hi = (int)days.size();
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        const auto& d = days[mid];
        bool before = (d.year < year) ||
                      (d.year == year && d.month < month) ||
                      (d.year == year && d.month == month && d.day < day);
        if (before) lo = mid + 1; else hi = mid;
    }
    return lo;
}

int DataLoader::upperBound(int year, int month, int day) const {
    int lo = -1, hi = (int)days.size() - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        const auto& d = days[mid];
        bool after = (d.year > year) ||
                     (d.year == year && d.month > month) ||
                     (d.year == year && d.month == month && d.day > day);
        if (after) hi = mid - 1; else lo = mid;
    }
    return lo;
}
