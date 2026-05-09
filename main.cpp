// ============================================================
//  KSE-100 Financial Dashboard
//  CS201 Project — Persistent Segment Tree Analytics
//  Interactive TUI (ncurses)
// ============================================================

#include <ncurses.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>
#include <algorithm>

#include "DataLoader.hpp"
#include "Analytics.hpp"

using namespace std;

enum {
    CP_DEFAULT  = 1,
    CP_HEADER   = 2,
    CP_SELECTED = 3,
    CP_GREEN    = 4,
    CP_RED      = 5,
    CP_YELLOW   = 6,
    CP_CYAN     = 7,
    CP_STATUSBAR= 8,
    CP_BORDER   = 9,
    CP_DIMMED   = 10,
};

static const int MENU_W  = 30;
static const int HEADER_H = 3;
static const int FOOTER_H = 2;

struct MenuItem {
    string label;
    string hint;
    int    feature;
    bool   needsYears;
};

static const vector<MenuItem> MENU_ITEMS = {
    {"Max Single-Day Crash",   "Worst single trading day",        1, false},
    {"Max Single-Day Rally",   "Best single trading day",         2, false},
    {"Volatility Spread (IQR)","Interquartile range of closes",   3, true },
    {"Drops > 4%  [2005-10]",  "PST count query on pct_change",   4, false},
    {"Market Sentiment",       "% positive vs negative days",     5, true },
};

struct YearRange { string label; int sy, ey; };
static const vector<YearRange> RANGES = {
    {"Full Period  (2004 - 2024)", 2004, 2024},
    {"Pre-Crisis   (2004 - 2007)", 2004, 2007},
    {"Crisis Era   (2008 - 2012)", 2008, 2012},
    {"Bull Run     (2013 - 2017)", 2013, 2017},
    {"Recent       (2018 - 2024)", 2018, 2024},
};

enum class State { LOADING, MENU, YEAR_SELECT, RESULT };

static State       g_state   = State::LOADING;
static int         g_menuSel = 0;
static int         g_yearSel = 0;
static int         g_feature = 0;
static QueryResult g_result;
static int         g_totalDays = 0;
static int         g_minYear   = 0, g_maxYear = 0;

static void drawBox(int y, int x, int h, int w,
                    const string& title = "", int bcp = CP_BORDER) {
    attron(COLOR_PAIR(bcp));
    mvaddch(y,     x,     ACS_ULCORNER);
    mvaddch(y,     x+w-1, ACS_URCORNER);
    mvaddch(y+h-1, x,     ACS_LLCORNER);
    mvaddch(y+h-1, x+w-1, ACS_LRCORNER);
    for (int c = x+1; c < x+w-1; c++) {
        mvaddch(y,     c, ACS_HLINE);
        mvaddch(y+h-1, c, ACS_HLINE);
    }
    for (int r = y+1; r < y+h-1; r++) {
        mvaddch(r, x,     ACS_VLINE);
        mvaddch(r, x+w-1, ACS_VLINE);
    }
    if (!title.empty()) {
        string t = " " + title + " ";
        mvaddstr(y, x+2, t.c_str());
    }
    attroff(COLOR_PAIR(bcp));
}

static void drawHeader(int cols) {
    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    for (int r = 0; r < HEADER_H - 1; r++) mvhline(r, 0, ' ', cols);
    string t = "  KSE-100 Financial Dashboard  |  Persistent Segment Tree Analytics  ";
    mvaddstr(1, (cols - (int)t.size()) / 2, t.c_str());
    attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);
    attron(COLOR_PAIR(CP_BORDER));
    mvhline(HEADER_H - 1, 0, ACS_HLINE, cols);
    attroff(COLOR_PAIR(CP_BORDER));
}

static void drawStatus(int rows, int cols) {
    attron(COLOR_PAIR(CP_BORDER));
    mvhline(rows - FOOTER_H, 0, ACS_HLINE, cols);
    attroff(COLOR_PAIR(CP_BORDER));

    attron(COLOR_PAIR(CP_STATUSBAR));
    mvhline(rows - 1, 0, ' ', cols);

    string hints;
    if (g_state == State::MENU)
        hints = "  Arrows/1-5: Navigate   Enter: Run Query   Q: Quit";
    else if (g_state == State::YEAR_SELECT)
        hints = "  Arrows/1-5: Choose Range   Enter: Confirm   Esc: Back";
    else if (g_state == State::RESULT)
        hints = "  Any Key: Back to Menu";

    string info = "  Loaded: " + to_string(g_totalDays) + " trading days  |  "
                  + to_string(g_minYear) + " - " + to_string(g_maxYear) + "  ";

    mvaddstr(rows - 1, 0, hints.c_str());
    int ix = cols - (int)info.size() - 1;
    if (ix > (int)hints.size() + 2) mvaddstr(rows - 1, ix, info.c_str());
    attroff(COLOR_PAIR(CP_STATUSBAR));
}

static void drawMenu(int rows, int cols) {
    int ph = rows - HEADER_H - FOOTER_H;
    int py = HEADER_H;
    drawBox(py, 0, ph, MENU_W, "ANALYSIS MENU");

    int iy = py + 2;
    for (int i = 0; i < (int)MENU_ITEMS.size(); i++) {
        bool sel = (i == g_menuSel);
        if (sel) {
            attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
            mvhline(iy, 1, ' ', MENU_W - 2);
        } else {
            attron(COLOR_PAIR(CP_DEFAULT));
        }
        string lbl = "  [" + to_string(i+1) + "] " + MENU_ITEMS[i].label;
        mvaddstr(iy, 1, lbl.c_str());
        if (sel) attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
        else     attroff(COLOR_PAIR(CP_DEFAULT));

        attron(COLOR_PAIR(CP_DIMMED));
        string hint = "       " + MENU_ITEMS[i].hint;
        mvaddstr(iy + 1, 1, hint.c_str());
        attroff(COLOR_PAIR(CP_DIMMED));
        iy += 3;
    }

    attron(COLOR_PAIR(CP_RED) | A_BOLD);
    mvaddstr(py + ph - 3, 2, "  [Q] Quit");
    attroff(COLOR_PAIR(CP_RED) | A_BOLD);
}

static void drawResult(int rows, int cols) {
    int ph = rows - HEADER_H - FOOTER_H;
    int py = HEADER_H;
    int px = MENU_W;
    int pw = cols - MENU_W;
    drawBox(py, px, ph, pw, "RESULTS");

    if (g_state == State::MENU) {
        int cy = py + 2;
        attron(COLOR_PAIR(CP_CYAN) | A_BOLD);
        string w = "Welcome to the KSE-100 PST Dashboard";
        mvaddstr(cy++, px + (pw - (int)w.size()) / 2, w.c_str());
        attroff(COLOR_PAIR(CP_CYAN) | A_BOLD);
        cy++;
        attron(COLOR_PAIR(CP_DIMMED));
        vector<string> lines = {
            "This dashboard analyses 20 years of KSE-100 data",
            "using a Persistent Segment Tree (PST).",
            "",
            "PST enables O(log N) range order-statistic queries",
            "over any time window — no sorting required.",
            "",
            "Select a feature from the menu to begin.",
            "",
            "[ Data Summary ]",
        };
        for (auto& l : lines) mvaddstr(cy++, px + 4, l.c_str());
        attroff(COLOR_PAIR(CP_DIMMED));
        attron(COLOR_PAIR(CP_DEFAULT));
        mvaddstr(cy++, px + 4, ("  Total Trading Days : " + to_string(g_totalDays)).c_str());
        mvaddstr(cy++, px + 4, ("  Coverage           : " + to_string(g_minYear)
                                 + " - " + to_string(g_maxYear)).c_str());
        attroff(COLOR_PAIR(CP_DEFAULT));
        return;
    }

    if (!g_result.ok) {
        attron(COLOR_PAIR(CP_RED) | A_BOLD);
        mvaddstr(py + 2, px + 3, ("ERROR: " + g_result.error_msg).c_str());
        attroff(COLOR_PAIR(CP_RED) | A_BOLD);
        return;
    }

    int cy = py + 2;
    attron(COLOR_PAIR(CP_CYAN) | A_BOLD);
    mvaddstr(cy++, px + 3, g_result.title.c_str());
    attroff(COLOR_PAIR(CP_CYAN) | A_BOLD);
    cy++;

    int lw = 24;
    for (auto& [label, value] : g_result.fields) {
        if (cy >= py + ph - 3) break;
        attron(COLOR_PAIR(CP_YELLOW));
        string lbl = "  " + label + " :";
        while ((int)lbl.size() < lw + 2) lbl += " ";
        mvaddstr(cy, px + 2, lbl.c_str());
        attroff(COLOR_PAIR(CP_YELLOW));

        int vcp = CP_DEFAULT;
        if (label.find("Change") != string::npos ||
            label.find("Return") != string::npos) {
            vcp = (!value.empty() && value[0] == '-') ? CP_RED : CP_GREEN;
        } else if (label.find("Positive") != string::npos) vcp = CP_GREEN;
        else if (label.find("Negative") != string::npos)   vcp = CP_RED;
        else if (label.find("Worst")    != string::npos)   vcp = CP_RED;
        else if (label.find("IQR")      != string::npos ||
                 label.find("Method")   != string::npos)   vcp = CP_CYAN;

        attron(COLOR_PAIR(vcp) | A_BOLD);
        mvaddstr(cy, px + 2 + lw + 1, value.c_str());
        attroff(COLOR_PAIR(vcp) | A_BOLD);
        cy++;
    }

    cy++;
    attron(COLOR_PAIR(CP_DIMMED));
    ostringstream oss;
    oss << fixed << setprecision(3) << g_result.query_ms;
    string timing = "  Query time: " + oss.str() + " ms";
    if (cy < py + ph - 2) mvaddstr(cy, px + 2, timing.c_str());
    attroff(COLOR_PAIR(CP_DIMMED));
}

static void drawYearDialog(int rows, int cols) {
    int dh = (int)RANGES.size() + 6;
    int dw = 46;
    int dy = (rows - dh) / 2;
    int dx = (cols - dw) / 2;

    // clear dialog area
    attron(COLOR_PAIR(CP_DEFAULT));
    for (int r = dy; r < dy + dh; r++) mvhline(r, dx, ' ', dw);
    attroff(COLOR_PAIR(CP_DEFAULT));

    attron(COLOR_PAIR(CP_YELLOW) | A_BOLD);
    drawBox(dy, dx, dh, dw, "Select Time Window", CP_YELLOW);
    attroff(COLOR_PAIR(CP_YELLOW) | A_BOLD);

    int iy = dy + 2;
    attron(COLOR_PAIR(CP_CYAN));
    string feat = "  Feature: " + MENU_ITEMS[g_feature - 1].label;
    mvaddstr(iy++, dx + 1, feat.c_str());
    attroff(COLOR_PAIR(CP_CYAN));
    iy++;

    for (int i = 0; i < (int)RANGES.size(); i++) {
        bool sel = (i == g_yearSel);
        if (sel) {
            attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
            mvhline(iy, dx + 1, ' ', dw - 2);
        } else {
            attron(COLOR_PAIR(CP_DEFAULT));
        }
        string entry = "  " + to_string(i+1) + ".  " + RANGES[i].label;
        mvaddstr(iy, dx + 1, entry.c_str());
        if (sel) attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
        else     attroff(COLOR_PAIR(CP_DEFAULT));
        iy++;
    }
}

static void fullRedraw() {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    erase();
    drawHeader(cols);
    drawStatus(rows, cols);
    drawMenu(rows, cols);
    drawResult(rows, cols);
    if (g_state == State::YEAR_SELECT) drawYearDialog(rows, cols);
    refresh();
}

static void runQuery(const Analytics& A, int feature, int sy = 0, int ey = 0) {
    switch (feature) {
        case 1: g_result = A.maxCrash();              break;
        case 2: g_result = A.maxRally();              break;
        case 3: g_result = A.volatilityIQR(sy, ey);  break;
        case 4: g_result = A.drops2005to2010();       break;
        case 5: g_result = A.sentiment(sy, ey);       break;
    }
}

static void loadingScreen(const string& msg) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    erase();
    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    mvhline(0, 0, ' ', cols);
    string t = "  KSE-100 Financial Dashboard  |  Persistent Segment Tree Analytics  ";
    mvaddstr(0, (cols - (int)t.size()) / 2, t.c_str());
    attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

    attron(COLOR_PAIR(CP_CYAN) | A_BOLD);
    mvaddstr(rows/2 - 1, (cols - (int)msg.size()) / 2, msg.c_str());
    attroff(COLOR_PAIR(CP_CYAN) | A_BOLD);

    attron(COLOR_PAIR(CP_DIMMED));
    string sub = "Building Persistent Segment Trees — please wait...";
    mvaddstr(rows/2 + 1, (cols - (int)sub.size()) / 2, sub.c_str());
    attroff(COLOR_PAIR(CP_DIMMED));

    refresh();
}

int main(int argc, char* argv[]) {
    string csv_path = "KSE100-20years.csv";
    if (argc > 1) csv_path = argv[1];

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);
    set_escdelay(50);

    if (!has_colors()) { endwin(); fputs("No colour support.\n", stderr); return 1; }
    start_color();
    use_default_colors();
    init_pair(CP_DEFAULT,   COLOR_WHITE,  -1);
    init_pair(CP_HEADER,    COLOR_BLACK,  COLOR_CYAN);
    init_pair(CP_SELECTED,  COLOR_BLACK,  COLOR_WHITE);
    init_pair(CP_GREEN,     COLOR_GREEN,  -1);
    init_pair(CP_RED,       COLOR_RED,    -1);
    init_pair(CP_YELLOW,    COLOR_YELLOW, -1);
    init_pair(CP_CYAN,      COLOR_CYAN,   -1);
    init_pair(CP_STATUSBAR, COLOR_BLACK,  COLOR_BLUE);
    init_pair(CP_BORDER,    COLOR_BLUE,   -1);
    init_pair(CP_DIMMED,    8,            -1);

    loadingScreen("Loading 20 years of KSE-100 data...");

    DataLoader loader;
    if (!loader.load(csv_path)) {
        endwin();
        fprintf(stderr, "Error: cannot load '%s'\nUsage: %s [csv_file]\n",
                csv_path.c_str(), argv[0]);
        return 1;
    }

    loadingScreen("Parsing complete.  Building PSTs...");

    Analytics analytics(loader.days);
    g_totalDays = analytics.total_days();
    g_minYear   = analytics.min_year();
    g_maxYear   = analytics.max_year();
    g_state     = State::MENU;

    while (true) {
        fullRedraw();
        int ch = getch();

        if (g_state == State::MENU) {
            if (ch == 'q' || ch == 'Q') break;
            else if (ch == KEY_UP   || ch == 'k')
                g_menuSel = (g_menuSel - 1 + (int)MENU_ITEMS.size()) % (int)MENU_ITEMS.size();
            else if (ch == KEY_DOWN || ch == 'j')
                g_menuSel = (g_menuSel + 1) % (int)MENU_ITEMS.size();
            else if (ch == '\n' || ch == KEY_ENTER || ch == '\r') {
                g_feature = MENU_ITEMS[g_menuSel].feature;
                if (MENU_ITEMS[g_menuSel].needsYears) { g_yearSel = 0; g_state = State::YEAR_SELECT; }
                else { runQuery(analytics, g_feature); g_state = State::RESULT; }
            }
            else if (ch >= '1' && ch <= '5') {
                int i = ch - '1';
                if (i < (int)MENU_ITEMS.size()) {
                    g_menuSel = i;
                    g_feature = MENU_ITEMS[i].feature;
                    if (MENU_ITEMS[i].needsYears) { g_yearSel = 0; g_state = State::YEAR_SELECT; }
                    else { runQuery(analytics, g_feature); g_state = State::RESULT; }
                }
            }
        }
        else if (g_state == State::YEAR_SELECT) {
            if (ch == 27 || ch == 'q' || ch == 'Q') g_state = State::MENU;
            else if (ch == KEY_UP   || ch == 'k')
                g_yearSel = (g_yearSel - 1 + (int)RANGES.size()) % (int)RANGES.size();
            else if (ch == KEY_DOWN || ch == 'j')
                g_yearSel = (g_yearSel + 1) % (int)RANGES.size();
            else if (ch == '\n' || ch == KEY_ENTER || ch == '\r') {
                runQuery(analytics, g_feature, RANGES[g_yearSel].sy, RANGES[g_yearSel].ey);
                g_state = State::RESULT;
            }
            else if (ch >= '1' && ch <= '5') {
                int i = ch - '1';
                if (i < (int)RANGES.size()) {
                    g_yearSel = i;
                    runQuery(analytics, g_feature, RANGES[i].sy, RANGES[i].ey);
                    g_state = State::RESULT;
                }
            }
        }
        else if (g_state == State::RESULT) {
            if (ch == 'q' || ch == 'Q') break;
            g_state = State::MENU;
        }
    }

    endwin();
    return 0;
}
