#ifndef COORDINATE_COMPRESSOR_H
#define COORDINATE_COMPRESSOR_H

#include <vector>
#include <algorithm>

using namespace std;

class CoordinateCompressor {
private:
    vector<double> values;

public:
    void add(double val);
    void build();
    int get_compressed(double val) const;
    int get_compressed_less_equal(double val) const;
    double get_original(int index) const;
    int size() const;
};

#endif
