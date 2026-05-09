#include "Analytics.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <climits>

using namespace std;
using namespace chrono;

// ─── helpers ──────────────────────────────────────────────────────────────────

static string fmt(double v, int prec = 2) {
    ostringstream oss;
    oss << fixed << setprecision(prec) << v;
    return oss.str();
}

static string fmtPct(double v) {
    return fmt(v, 2) + "%";
}

static string fmtIdx(double v) {
    // Format large index values with commas
    ostringstream oss;
    oss << fixed << setprecision(2) << v;
    string s = oss.str();
    // Insert commas
    int dot = (int)s.find('.');
    if (dot == (int)string::npos) dot = (int)s.size();
    for (int i = dot - 3; i > 0; i -= 3)
        s.insert(i, ",");
    return s;
}

// ─── construction ─────────────────────────────────────────────────────────────

Analytics::Analytics(const vector<DayData>& data) : days(data), N((int)data.size()) {
    minYear = maxYear = 0;
    if (N > 0) {
        minYear = days.front().year;
        maxYear = days.back().year;
    }
    buildPSTs();
}

Analytics::~Analytics() {
    delete pst_close;
    delete pst_pct;
}

void Analytics::buildPSTs() {
    if (N == 0) return;

    // --- PST 1: close prices ---
    for (const auto& d : days) comp_close.add(d.close);
    comp_close.build();
    int M_close = comp_close.size();
    pst_close = new PersistentSegmentTree(M_close);
    for (const auto& d : days) {
        int cv = comp_close.get_compressed(d.close);
        pst_close->insert(cv);
    }

    // --- PST 2: pct_change values ---
    for (const auto& d : days) comp_pct.add(d.pct_change);
    comp_pct.build();
    int M_pct = comp_pct.size();
    pst_pct = new PersistentSegmentTree(M_pct);
    for (const auto& d : days) {
        int cv = comp_pct.get_compressed(d.pct_change);
        pst_pct->insert(cv);
    }
}

// ─── helpers ──────────────────────────────────────────────────────────────────

QueryResult Analytics::makeError(const string& title, const string& msg) const {
    QueryResult r;
    r.title = title;
    r.ok = false;
    r.error_msg = msg;
    return r;
}

bool Analytics::getWindow(int sy, int ey, int& L, int& R) const {
    // Find the DataLoader utility functions via manual binary search
    // (DataLoader::lowerBound / upperBound logic inlined here for the range)
    L = -1; R = -1;
    for (int i = 0; i < N; i++) {
        if (days[i].year >= sy) { L = i; break; }
    }
    for (int i = N - 1; i >= 0; i--) {
        if (days[i].year <= ey) { R = i; break; }
    }
    return (L >= 0 && R >= L);
}

// ─── Feature 1: Maximum Single-Day Crash ─────────────────────────────────────

QueryResult Analytics::maxCrash() const {
    QueryResult res;
    res.title = "Maximum Single-Day Crash";
    if (N < 2) return makeError(res.title, "Insufficient data");

    auto t0 = high_resolution_clock::now();

    int worst = 1;
    for (int i = 2; i < N; i++) {
        if (days[i].pct_change < days[worst].pct_change) worst = i;
    }

    auto t1 = high_resolution_clock::now();
    res.query_ms = duration<double, milli>(t1 - t0).count();

    const DayData& d = days[worst];
    res.fields = {
        {"Date",              d.date_str},
        {"Closing Index",     fmtIdx(d.close)},
        {"Change (Points)",   fmt(d.change, 2)},
        {"Change (%)",        fmtPct(d.pct_change)},
        {"Opening Index",     fmtIdx(d.open)},
        {"Day Low",           fmtIdx(d.low)},
    };
    return res;
}

// ─── Feature 2: Maximum Single-Day Rally ─────────────────────────────────────

QueryResult Analytics::maxRally() const {
    QueryResult res;
    res.title = "Maximum Single-Day Rally";
    if (N < 2) return makeError(res.title, "Insufficient data");

    auto t0 = high_resolution_clock::now();

    int best = 1;
    for (int i = 2; i < N; i++) {
        if (days[i].pct_change > days[best].pct_change) best = i;
    }

    auto t1 = high_resolution_clock::now();
    res.query_ms = duration<double, milli>(t1 - t0).count();

    const DayData& d = days[best];
    res.fields = {
        {"Date",              d.date_str},
        {"Closing Index",     fmtIdx(d.close)},
        {"Change (Points)",   "+" + fmt(d.change, 2)},
        {"Change (%)",        "+" + fmtPct(d.pct_change)},
        {"Opening Index",     fmtIdx(d.open)},
        {"Day High",          fmtIdx(d.high)},
    };
    return res;
}

// ─── Feature 3: Volatility IQR (PST range k-th query) ────────────────────────

QueryResult Analytics::volatilityIQR(int startYear, int endYear) const {
    QueryResult res;
    res.title = "Volatility Spread — IQR [" + to_string(startYear)
                + " – " + to_string(endYear) + "]";
    if (!pst_close)
        return makeError(res.title, "PST not built");

    int L, R;
    if (!getWindow(startYear, endYear, L, R))
        return makeError(res.title, "No data in range");

    int n = R - L + 1;
    if (n < 4)
        return makeError(res.title, "Too few data points for IQR");

    // Version indices for PST window [L, R]:
    //   versionL_minus_1 = L  (contains days 0..L-1)
    //   versionR         = R+1 (contains days 0..R)
    int vL = L;
    int vR = R + 1;

    auto t0 = high_resolution_clock::now();

    // Quartile ranks (1-indexed)
    int q1_rank = max(1, (n + 3) / 4);           // ceil(n/4)
    int q2_rank = max(1, (n + 1) / 2);           // ceil(n/2) — median
    int q3_rank = max(1, (3 * n + 3) / 4);       // ceil(3n/4)

    int q1_c = pst_close->query_kth(vL, vR, q1_rank);
    int q2_c = pst_close->query_kth(vL, vR, q2_rank);
    int q3_c = pst_close->query_kth(vL, vR, q3_rank);

    // Min and Max
    int min_c = pst_close->query_kth(vL, vR, 1);
    int max_c = pst_close->query_kth(vL, vR, n);

    auto t1 = high_resolution_clock::now();
    res.query_ms = duration<double, milli>(t1 - t0).count();

    double q1  = comp_close.get_original(q1_c);
    double med = comp_close.get_original(q2_c);
    double q3  = comp_close.get_original(q3_c);
    double mn  = comp_close.get_original(min_c);
    double mx  = comp_close.get_original(max_c);
    double iqr = q3 - q1;

    res.fields = {
        {"Period",            to_string(startYear) + " – " + to_string(endYear)},
        {"Trading Days",      to_string(n)},
        {"Min Close",         fmtIdx(mn)},
        {"Q1 (25th pct)",     fmtIdx(q1)},
        {"Median",            fmtIdx(med)},
        {"Q3 (75th pct)",     fmtIdx(q3)},
        {"Max Close",         fmtIdx(mx)},
        {"IQR  (Q3 – Q1)",    fmtIdx(iqr)},
        {"PST Method",        "O(log N) per quartile query"},
    };
    return res;
}

// ─── Feature 4: Days with drop > 4%  (2005–2010) ─────────────────────────────

QueryResult Analytics::drops2005to2010() const {
    QueryResult res;
    res.title = "Days with Drop > 4%  [2005 – 2010]";
    if (!pst_pct)
        return makeError(res.title, "PST not built");

    int L, R;
    if (!getWindow(2005, 2010, L, R))
        return makeError(res.title, "No data for 2005–2010");

    int n   = R - L + 1;
    int vL  = L;
    int vR  = R + 1;

    // Threshold: pct_change <= -4.0 means drop of at least 4%
    // We use get_compressed_less_equal(-4.0) to count values <= -4%
    // "drop by MORE than 4%" → pct_change < -4.0
    // Use threshold slightly below -4.0 for strict inequality
    auto t0 = high_resolution_clock::now();

    int threshold_c = comp_pct.get_compressed_less_equal(-4.0 - 1e-9);
    int count = 0;
    if (threshold_c >= 0) {
        count = pst_pct->query_count_less_equal(vL, vR, threshold_c);
    }

    auto t1 = high_resolution_clock::now();
    res.query_ms = duration<double, milli>(t1 - t0).count();

    // Also find the worst among those for context
    double worst_pct = 0.0;
    string worst_date = "N/A";
    for (int i = L; i <= R; i++) {
        if (days[i].pct_change < -4.0 && days[i].pct_change < worst_pct) {
            worst_pct = days[i].pct_change;
            worst_date = days[i].date_str;
        }
    }

    res.fields = {
        {"Period",            "2005 – 2010"},
        {"Total Trading Days",to_string(n)},
        {"Days with Drop > 4%", to_string(count)},
        {"Frequency",         fmtPct(100.0 * count / n)},
        {"Worst Single Day",  worst_date + "  (" + fmtPct(worst_pct) + ")"},
        {"PST Method",        "O(log N) count query"},
    };
    return res;
}

// ─── Feature 5: % positive vs negative in window ─────────────────────────────

QueryResult Analytics::sentiment(int startYear, int endYear) const {
    QueryResult res;
    res.title = "Market Sentiment [" + to_string(startYear)
                + " – " + to_string(endYear) + "]";
    if (!pst_pct)
        return makeError(res.title, "PST not built");

    int L, R;
    if (!getWindow(startYear, endYear, L, R))
        return makeError(res.title, "No data in range");

    int n  = R - L + 1;
    int vL = L;
    int vR = R + 1;

    auto t0 = high_resolution_clock::now();

    // Use the compressed index of exactly 0.0 as the boundary.
    // get_compressed(0.0) returns the rank of 0.0 in the sorted unique values.
    // Indices [0 .. zero_c-1] are negative values.
    // Index zero_c is exactly 0.0 (flat).
    // Indices [zero_c+1 .. M-1] are positive values.
    int zero_c = comp_pct.get_compressed(0.0);

    // Count of strictly negative days: count elements with rank < zero_c
    int neg_count  = (zero_c > 0)
                     ? pst_pct->query_count_less_equal(vL, vR, zero_c - 1)
                     : 0;

    // Count of days with pct_change <= 0 (negatives + flats)
    int nonpos_count = pst_pct->query_count_less_equal(vL, vR, zero_c);

    int flat_count = nonpos_count - neg_count;
    int pos_count  = n - nonpos_count;

    auto t1 = high_resolution_clock::now();
    res.query_ms = duration<double, milli>(t1 - t0).count();

    auto pct = [&](int x) { return fmtPct(100.0 * x / n); };

    // Average pct_change in window (brute-force, for display)
    double sum_pct = 0.0;
    for (int i = L; i <= R; i++) sum_pct += days[i].pct_change;
    double avg_pct = sum_pct / n;

    res.fields = {
        {"Period",           to_string(startYear) + " – " + to_string(endYear)},
        {"Total Days",       to_string(n)},
        {"Positive Days",    to_string(pos_count)  + "  (" + pct(pos_count)  + ")"},
        {"Negative Days",    to_string(neg_count)  + "  (" + pct(neg_count)  + ")"},
        {"Flat Days",        to_string(flat_count) + "  (" + pct(flat_count) + ")"},
        {"Avg Daily Return", fmtPct(avg_pct)},
        {"Bull/Bear Ratio",  fmt((double)pos_count / max(1, neg_count), 2) + "x"},
        {"PST Method",       "O(log N) count query"},
    };
    return res;
}
