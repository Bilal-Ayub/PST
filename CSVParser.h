#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <string>
#include <vector>

// represents one trading day with its computed daily return
struct DayRecord {
    std::string date;    
    double close;        
    double returnPct;     
};

std::vector<DayRecord> parseCSV(const std::string& filename);

#endif // CSV_PARSER_H
