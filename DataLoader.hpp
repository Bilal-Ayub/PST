#pragma once
#include <vector>
#include <string>

// Represents one trading day's data
struct DayData {
    std::string date_str;   // original string, e.g. "02-Aug-24"
    int year, month, day;   // parsed date
    double open, high, low, close;
    double volume;
    double change;          // Close - PrevClose (from CSV)
    double pct_change;      // change as % of previous close (computed)
};

class DataLoader {
public:
    std::vector<DayData> days; // sorted chronologically: oldest first

    // Load and parse the CSV; returns true on success
    bool load(const std::string& filename);

    // Returns index of the first day with date >= (year, month, day)
    // Returns N (past-the-end) if none found
    int lowerBound(int year, int month = 1, int day = 1) const;

    // Returns index of the last day with date <= (year, month, day)
    // Returns -1 if none found
    int upperBound(int year, int month = 12, int day = 31) const;

private:
    static double parseNumber(const std::string& s);
    static bool parseDate(const std::string& s, int& y, int& m, int& d);
    static int monthNum(const std::string& mon);
};
