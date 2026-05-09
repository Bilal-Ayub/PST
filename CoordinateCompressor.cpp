#include "CoordinateCompressor.hpp"

using namespace std;

void CoordinateCompressor::add(double val) {
    values.push_back(val);
}

void CoordinateCompressor::build() {
    sort(values.begin(), values.end());
    values.erase(unique(values.begin(), values.end()), values.end());
}

int CoordinateCompressor::get_compressed(double val) const {
    return lower_bound(values.begin(), values.end(), val) - values.begin();
}

int CoordinateCompressor::get_compressed_less_equal(double val) const {
    auto it = upper_bound(values.begin(), values.end(), val);
    if (it == values.begin()) return -1;
    return (int)(it - values.begin()) - 1;
}

double CoordinateCompressor::get_original(int index) const {
    if (index >= 0 && index < (int)values.size()) {
        return values[index];
    }
    return -1.0;
}

int CoordinateCompressor::size() const {
    return (int)values.size();
}
