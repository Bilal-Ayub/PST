#pragma once
#include "DataLoader.hpp"
#include "CoordinateCompressor.hpp"
#include "PersistentSegmentTree.hpp"
#include <string>
#include <vector>
#include <utility>

// One row in a bar-chart result (used by Volatility Clustering)
struct ChartRow {
    std::string label;   // e.g. "2008"
    int  count   = 0;   // raw count for this bucket
    int  total   = 0;   // total days in this bucket (for pct)
    double pct   = 0.0; // count / total * 100
    bool highlight = false; // true for crisis / outlier years
};

// Result of a single analytics query
struct QueryResult {
    std::string title;
    std::vector<std::pair<std::string, std::string>> fields; // label → value
    double query_ms = 0.0; // wall-clock time for the PST portion
    bool   ok       = true;
    std::string error_msg;
    // Bar-chart mode (used by Volatility Clustering)
    bool is_chart = false;
    std::vector<ChartRow> chart_rows;
};

class Analytics {
public:
    // Constructs and builds both PSTs from the loaded data
    explicit Analytics(const std::vector<DayData>& data);
    ~Analytics();

    // Feature 1: Maximum Single-Day Crash
    QueryResult maxCrash() const;

    // Feature 2: Maximum Single-Day Rally
    QueryResult maxRally() const;

    // Feature 3: Volatility Spread (IQR of close prices) for a year range
    QueryResult volatilityIQR(int startYear, int endYear) const;

    // Feature 4: Days with drop > 4% between 2005 and 2010 (hardcoded per spec)
    QueryResult drops2005to2010() const;

    // Feature 5: % positive vs negative days in a year range
    QueryResult sentiment(int startYear, int endYear) const;

    // Feature 6: Maximum drawdown (peak-to-trough worst loss) in a year range
    QueryResult maxDrawdown(int startYear, int endYear) const;

    // Feature 7: Longest consecutive green / red streak in a year range
    QueryResult streakFinder(int startYear, int endYear) const;

    // Feature 8: Percentile rank of the most-recent close across all 20 years
    QueryResult percentileRank() const;

    // Feature 9: Volatility clustering — volatile days per year as a bar chart
    QueryResult volatilityClustering() const;

    int total_days() const { return N; }
    int min_year()   const { return minYear; }
    int max_year()   const { return maxYear; }

private:
    const std::vector<DayData>& days;
    int N;
    int minYear, maxYear;

    // PST 1: built on close prices (for IQR, feature 3)
    CoordinateCompressor comp_close;
    PersistentSegmentTree* pst_close = nullptr;

    // PST 2: built on pct_change values (for features 4 & 5)
    CoordinateCompressor comp_pct;
    PersistentSegmentTree* pst_pct = nullptr;

    void buildPSTs();

    // Convert a day index to PST version:
    //   versionOf(i) = i+1  (roots[0]=empty, roots[i+1] = after inserting day i)
    // For a window [L, R] (inclusive day indices):
    //   versionL_minus_1 = L,  versionR = R+1
    int versionOf(int dayIndex) const { return dayIndex + 1; }

    // Find the window bounds in the days array for a year range
    // Returns false if the range is invalid or empty
    bool getWindow(int startYear, int endYear, int& L, int& R) const;

    QueryResult makeError(const std::string& title,
                          const std::string& msg) const;
};
