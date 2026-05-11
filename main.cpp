// ============================================================
//  KSE-100 Financial Dashboard
//  CS201 Project — Persistent Segment Tree Analytics
//  HTTP Server backend (replaces ncurses TUI)
// ============================================================

#define CPPHTTPLIB_NO_EXCEPTIONS
#include "httplib.h"
#include "nlohmann/json.hpp"

#include "DataLoader.hpp"
#include "Analytics.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

using namespace std;
using json = nlohmann::json;

// ─── helpers ─────────────────────────────────────────────────────────────────

// Convert a QueryResult to JSON
static json resultToJson(const QueryResult& r) {
    json j;
    j["ok"]       = r.ok;
    j["title"]    = r.title;
    j["query_ms"] = r.query_ms;

    if (!r.ok) {
        j["error"] = r.error_msg;
        return j;
    }

    // Key-value fields (used by most features)
    json fields = json::array();
    for (auto& [k, v] : r.fields) {
        json row;
        row["key"]   = k;
        row["value"] = v;
        fields.push_back(row);
    }
    j["fields"] = fields;

    // Bar-chart data (Feature 9: volatility clustering)
    j["is_chart"] = r.is_chart;
    if (r.is_chart) {
        json rows = json::array();
        for (auto& cr : r.chart_rows) {
            json row;
            row["label"]     = cr.label;
            row["count"]     = cr.count;
            row["total"]     = cr.total;
            row["pct"]       = cr.pct;
            row["highlight"] = cr.highlight;
            rows.push_back(row);
        }
        j["chart_rows"] = rows;
    }

    return j;
}

// Add CORS headers so the HTML file can fetch from localhost
static void addCors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

static void sendJson(httplib::Response& res, const json& j) {
    addCors(res);
    res.set_content(j.dump(), "application/json");
}

// ─── main ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    string csv_path = "KSE100-20years.csv";
    if (argc > 1) csv_path = argv[1];

    cout << "Loading " << csv_path << " ...\n";

    DataLoader loader;
    if (!loader.load(csv_path)) {
        cerr << "Error: cannot load '" << csv_path << "'\n";
        cerr << "Usage: " << argv[0] << " [csv_file]\n";
        return 1;
    }

    cout << "Loaded " << loader.days.size() << " trading days.\n";
    cout << "Building Persistent Segment Trees...\n";

    Analytics analytics(loader.days);

    cout << "PSTs ready.  Server starting on http://localhost:8080\n";
    cout << "Open dashboard.html in your browser.\n";
    cout << "Press Ctrl+C to stop.\n\n";

    httplib::Server svr;

    // ── GET /api/info ──────────────────────────────────────────────────────
    svr.Get("/api/info", [&](const httplib::Request&, httplib::Response& res) {
        json j;
        j["total_days"] = analytics.total_days();
        j["min_year"]   = analytics.min_year();
        j["max_year"]   = analytics.max_year();
        sendJson(res, j);
    });

    // ── GET /api/crash ─────────────────────────────────────────────────────
    svr.Get("/api/crash", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, resultToJson(analytics.maxCrash()));
    });

    // ── GET /api/rally ─────────────────────────────────────────────────────
    svr.Get("/api/rally", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, resultToJson(analytics.maxRally()));
    });

    // ── GET /api/iqr?start=YYYY&end=YYYY ──────────────────────────────────
    svr.Get("/api/iqr", [&](const httplib::Request& req, httplib::Response& res) {
        int sy = 2004, ey = 2024;
        if (req.has_param("start")) sy = stoi(req.get_param_value("start"));
        if (req.has_param("end"))   ey = stoi(req.get_param_value("end"));
        sendJson(res, resultToJson(analytics.volatilityIQR(sy, ey)));
    });

    // ── GET /api/drops?start=YYYY&end=YYYY&thresh=N.N ─────────────────────
    svr.Get("/api/drops", [&](const httplib::Request& req, httplib::Response& res) {
        int    sy  = 2005, ey  = 2010;
        double thr = 4.0;
        if (req.has_param("start"))  sy  = stoi(req.get_param_value("start"));
        if (req.has_param("end"))    ey  = stoi(req.get_param_value("end"));
        if (req.has_param("thresh")) thr = stod(req.get_param_value("thresh"));
        sendJson(res, resultToJson(analytics.drops(sy, ey, thr)));
    });

    // ── GET /api/sentiment?start=YYYY&end=YYYY ────────────────────────────
    svr.Get("/api/sentiment", [&](const httplib::Request& req, httplib::Response& res) {
        int sy = 2004, ey = 2024;
        if (req.has_param("start")) sy = stoi(req.get_param_value("start"));
        if (req.has_param("end"))   ey = stoi(req.get_param_value("end"));
        sendJson(res, resultToJson(analytics.sentiment(sy, ey)));
    });

    // ── GET /api/drawdown?start=YYYY&end=YYYY ────────────────────────────
    svr.Get("/api/drawdown", [&](const httplib::Request& req, httplib::Response& res) {
        int sy = 2004, ey = 2024;
        if (req.has_param("start")) sy = stoi(req.get_param_value("start"));
        if (req.has_param("end"))   ey = stoi(req.get_param_value("end"));
        sendJson(res, resultToJson(analytics.maxDrawdown(sy, ey)));
    });

    // ── GET /api/streak?start=YYYY&end=YYYY ──────────────────────────────
    svr.Get("/api/streak", [&](const httplib::Request& req, httplib::Response& res) {
        int sy = 2004, ey = 2024;
        if (req.has_param("start")) sy = stoi(req.get_param_value("start"));
        if (req.has_param("end"))   ey = stoi(req.get_param_value("end"));
        sendJson(res, resultToJson(analytics.streakFinder(sy, ey)));
    });

    // ── GET /api/percentile ───────────────────────────────────────────────
    svr.Get("/api/percentile", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, resultToJson(analytics.percentileRank()));
    });

    // ── GET /api/volatility ───────────────────────────────────────────────
    svr.Get("/api/volatility", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, resultToJson(analytics.volatilityClustering()));
    });

    // ── OPTIONS (preflight CORS) ──────────────────────────────────────────
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        addCors(res);
        res.status = 204;
    });

    svr.listen("0.0.0.0", 8080);
    return 0;
}
