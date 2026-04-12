#ifndef COORDINATE_COMPRESSOR_H
#define COORDINATE_COMPRESSOR_H

#include <vector>
#include <algorithm>

using namespace std;

class CoordinateCompressor {
private:
    vector<float> values;

public:
    // Add raw values to be compressed
    void add(float val);

    // Build the unique sorted array
    void build();

    // Get the exact compressed index (rank) for a given value
    int get_compressed(float val) const;

    // Get the highest compressed index that is <= the given value
    int get_compressed_less_equal(float val) const;

    // Get the original value for a given compressed rank
    float get_original(int index) const;

    // Get the total number of unique values
    int size() const;
};

#endif
