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

// ─── Feature 6: Maximum Drawdown ─────────────────────────────────────────────

QueryResult Analytics::maxDrawdown(int startYear, int endYear) const {
    QueryResult res;
    res.title = "Maximum Drawdown [" + to_string(startYear)
                + " – " + to_string(endYear) + "]";
    if (N < 2) return makeError(res.title, "Insufficient data");

    int L, R;
    if (!getWindow(startYear, endYear, L, R))
        return makeError(res.title, "No data in range");
    if (R - L < 1)
        return makeError(res.title, "Need at least 2 days");

    auto t0 = high_resolution_clock::now();

    // Track running peak; record the peak→trough pair with the largest % loss.
    double peak      = days[L].close;
    int    peak_idx  = L;
    double max_dd    = 0.0;        // as a positive percentage
    int    dd_peak   = L;
    int    dd_trough = L;

    for (int i = L + 1; i <= R; i++) {
        if (days[i].close > peak) {
            peak     = days[i].close;
            peak_idx = i;
        }
        double dd = (peak - days[i].close) / peak * 100.0;
        if (dd > max_dd) {
            max_dd    = dd;
            dd_peak   = peak_idx;
            dd_trough = i;
        }
    }

    // Find the first day after dd_trough where close >= days[dd_peak].close
    // (recovery point); -1 if never recovered within the window.
    int recovery_idx = -1;
    double peak_val  = days[dd_peak].close;
    for (int i = dd_trough + 1; i <= R; i++) {
        if (days[i].close >= peak_val) { recovery_idx = i; break; }
    }

    auto t1 = high_resolution_clock::now();
    res.query_ms = duration<double, milli>(t1 - t0).count();

    string recovery = (recovery_idx >= 0)
        ? days[recovery_idx].date_str
        : "Not recovered in period";

    res.fields = {
        {"Period",          to_string(startYear) + " – " + to_string(endYear)},
        {"Max Drawdown",    "-" + fmtPct(max_dd)},
        {"Peak Date",       days[dd_peak].date_str},
        {"Peak Index",      fmtIdx(days[dd_peak].close)},
        {"Trough Date",     days[dd_trough].date_str},
        {"Trough Index",    fmtIdx(days[dd_trough].close)},
        {"Points Lost",     fmtIdx(days[dd_peak].close - days[dd_trough].close)},
        {"Recovered By",    recovery},
        {"Algorithm",       "O(N) running-peak scan"},
    };
    return res;
}

// ─── Feature 7: Streak Finder ────────────────────────────────────────────────

QueryResult Analytics::streakFinder(int startYear, int endYear) const {
    QueryResult res;
    res.title = "Streak Finder [" + to_string(startYear)
                + " – " + to_string(endYear) + "]";
    if (N < 1) return makeError(res.title, "Insufficient data");

    int L, R;
    if (!getWindow(startYear, endYear, L, R))
        return makeError(res.title, "No data in range");

    auto t0 = high_resolution_clock::now();

    int best_green = 0, green_start_best = L;
    int best_red   = 0, red_start_best   = L;
    int cur_green  = 0, cur_green_start  = L;
    int cur_red    = 0, cur_red_start    = L;

    for (int i = L; i <= R; i++) {
        double p = days[i].pct_change;
        if (p > 0.0) {
            if (cur_green == 0) cur_green_start = i;
            cur_green++;
            cur_red = 0;
            if (cur_green > best_green) {
                best_green       = cur_green;
                green_start_best = cur_green_start;
            }
        } else if (p < 0.0) {
            if (cur_red == 0) cur_red_start = i;
            cur_red++;
            cur_green = 0;
            if (cur_red > best_red) {
                best_red       = cur_red;
                red_start_best = cur_red_start;
            }
        } else {
            cur_green = 0;
            cur_red   = 0;
        }
    }

    // Compute cumulative gain/loss over each best streak.
    auto streakStats = [&](int start, int len) -> pair<double,double> {
        double sum = 0.0;
        for (int i = start; i < start + len; i++) sum += days[i].pct_change;
        return {sum, sum / max(1, len)};
    };

    auto t1 = high_resolution_clock::now();
    res.query_ms = duration<double, milli>(t1 - t0).count();

    string g_start = (best_green > 0) ? days[green_start_best].date_str : "N/A";
    string g_end   = (best_green > 0) ? days[green_start_best + best_green - 1].date_str : "N/A";
    string r_start = (best_red   > 0) ? days[red_start_best].date_str   : "N/A";
    string r_end   = (best_red   > 0) ? days[red_start_best + best_red - 1].date_str : "N/A";

    auto [g_sum, g_avg] = (best_green > 0)
        ? streakStats(green_start_best, best_green)
        : make_pair(0.0, 0.0);
    auto [r_sum, r_avg] = (best_red > 0)
        ? streakStats(red_start_best, best_red)
        : make_pair(0.0, 0.0);

    res.fields = {
        {"Period",              to_string(startYear) + " – " + to_string(endYear)},
        {"Best Green Streak",   to_string(best_green) + " days"},
        {"  From",              g_start},
        {"  To",                g_end},
        {"  Cumul. Gain",       "+" + fmtPct(g_sum)},
        {"  Avg/Day",           "+" + fmtPct(g_avg)},
        {"Best Red Streak",     to_string(best_red) + " days"},
        {"  From",              r_start},
        {"  To",                r_end},
        {"  Cumul. Loss",       fmtPct(r_sum)},
        {"  Avg/Day",           fmtPct(r_avg)},
        {"Algorithm",           "O(N) linear scan"},
    };
    return res;
}

// ─── Feature 8: Percentile Rank ──────────────────────────────────────────────

QueryResult Analytics::percentileRank() const {
    QueryResult res;
    res.title = "Percentile Rank — Current Index Level (20-yr History)";
    if (!pst_close || N == 0)
        return makeError(res.title, "No data");

    // Use the most-recent closing price as the reference level.
    double cur_close = days[N - 1].close;
    string cur_date  = days[N - 1].date_str;

    auto t0 = high_resolution_clock::now();

    // Compressed index of the largest value <= cur_close.
    // Because cur_close is in the dataset this is always valid (>= 0).
    int comp_cur = comp_close.get_compressed_less_equal(cur_close);

    // Full-period rank: how many of all N days had close <= cur_close?
    int full_count = pst_close->query_count_less_equal(0, N, comp_cur);
    double pct_full = 100.0 * full_count / N;

    // Per-era percentile (each era queried against itself for context).
    struct Era { const char* name; int sy, ey; };
    static const Era eras[] = {
        {"Pre-Crisis (2004-07)", 2004, 2007},
        {"Crisis Era (2008-12)", 2008, 2012},
        {"Bull Run   (2013-17)", 2013, 2017},
        {"Recent     (2018-24)", 2018, 2024},
    };

    auto t1 = high_resolution_clock::now();
    res.query_ms = duration<double, milli>(t1 - t0).count();

    res.fields = {
        {"Reference Date",   cur_date},
        {"Current Index",    fmtIdx(cur_close)},
        {"Full-Period Rank", fmtPct(pct_full) + "  (of all " + to_string(N) + " days)"},
    };

    // Percentile within each era uses the same PST with era-scoped versions.
    for (auto& era : eras) {
        int L, R;
        if (getWindow(era.sy, era.ey, L, R)) {
            int n_era   = R - L + 1;
            int vL = L, vR = R + 1;
            int cnt     = pst_close->query_count_less_equal(vL, vR, comp_cur);
            double pct  = 100.0 * cnt / n_era;
            res.fields.push_back({era.name, fmtPct(pct)});
        }
    }

    res.fields.push_back({"PST Method", "O(log N) count query"});
    return res;
}

// ─── Feature 9: Volatility Clustering ────────────────────────────────────────

QueryResult Analytics::volatilityClustering() const {
    QueryResult res;
    res.title = "Volatility Clustering — Days |Move| > 2%  per Year";
    res.is_chart = true;
    if (!pst_pct || N == 0)
        return makeError(res.title, "No data");

    const double thresh = 2.0;

    // Precompute compressed thresholds once (same for every year).
    // comp_lo: largest index for values strictly < -thresh
    // comp_hi: largest index for values <= +thresh
    int comp_lo = comp_pct.get_compressed_less_equal(-thresh - 1e-9);
    int comp_hi = comp_pct.get_compressed_less_equal(thresh);

    auto t0 = high_resolution_clock::now();

    int total_vol = 0;
    for (int y = minYear; y <= maxYear; y++) {
        int L, R;
        if (!getWindow(y, y, L, R)) continue;
        int n  = R - L + 1;
        int vL = L, vR = R + 1;

        // calm days: pct_change in [-thresh, +thresh]
        //   count_le(+thresh)  - count_le(-thresh - eps)
        int calm_above = (comp_hi >= 0)
            ? pst_pct->query_count_less_equal(vL, vR, comp_hi) : 0;
        int calm_below = (comp_lo >= 0)
            ? pst_pct->query_count_less_equal(vL, vR, comp_lo) : 0;
        int calm         = calm_above - calm_below;
        int volatile_cnt = n - calm;
        total_vol       += volatile_cnt;

        ChartRow row;
        row.label = to_string(y);
        row.count = volatile_cnt;
        row.total = n;
        row.pct   = 100.0 * volatile_cnt / n;
        res.chart_rows.push_back(row);
    }

    auto t1 = high_resolution_clock::now();
    res.query_ms = duration<double, milli>(t1 - t0).count();

    // Highlight years that are > 1.5× the per-year average.
    if (!res.chart_rows.empty()) {
        double avg = (double)total_vol / (int)res.chart_rows.size();
        for (auto& row : res.chart_rows)
            row.highlight = (row.count > 1.5 * avg);
    }

    double avg_pct = (double)total_vol / N * 100.0;
    res.fields = {
        {"Threshold",    "± " + fmt(thresh, 1) + "% daily move"},
        {"Total Volatile Days", to_string(total_vol) + "  (" + fmtPct(avg_pct) + " of all days)"},
        {"PST Method",   "O(log N) count query per year"},
    };
    return res;
}
