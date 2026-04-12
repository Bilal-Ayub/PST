#include "CoordinateCompressor.h"

using namespace std;

void CoordinateCompressor::add(float val) {
    values.push_back(val);
}

void CoordinateCompressor::build() {
    sort(values.begin(), values.end());
    values.erase(unique(values.begin(), values.end()), values.end());
}

int CoordinateCompressor::get_compressed(float val) const {
    return lower_bound(values.begin(), values.end(), val) - values.begin();
}

int CoordinateCompressor::get_compressed_less_equal(float val) const {
    auto it = upper_bound(values.begin(), values.end(), val);
    if (it == values.begin()) return -1;
    return (it - values.begin()) - 1;
}

float CoordinateCompressor::get_original(int index) const {
    if (index >= 0 && index < values.size()) {
        return values[index];
    }
    return -1.0f; // basic error fallback
}

int CoordinateCompressor::size() const {
    return values.size();
}
