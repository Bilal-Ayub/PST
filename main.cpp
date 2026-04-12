#include <iostream>
#include <vector>
#include "CoordinateCompressor.h"
#include "PersistentSegmentTree.h"

using namespace std;

// Quick verification program
int main() {
    cout << "Testing Persistent Segment Tree..." << endl;

    // Simulate some PSX closing prices
    vector<float> prices = { 101.5, 98.2, 110.0, 105.3, 101.5, 99.0, 120.5 };
    // Sorted: 98.2, 99.0, 101.5, 101.5, 105.3, 110.0, 120.5
    
    // 1. Coordinate Compression
    CoordinateCompressor compressor;
    for (float p : prices) {
        compressor.add(p);
    }
    compressor.build();

    int M = compressor.size();
    cout << "Unique prices count (M) = " << M << endl;

    // 2. Initialize PST
    PersistentSegmentTree pst(M);

    // 3. Insert prices dynamically simulating daily updates
    // Version 0 is the initial empty state.
    for (float p : prices) {
        int compressed_val = compressor.get_compressed(p);
        pst.insert(compressed_val);
    }

    cout << "Total versions after insertion: " << pst.num_versions() << endl;

    // Q1: What was the 2nd lowest closing price from day 2 to day 5?
    // Days are 1-based intuitively, but our prices array is 0-based index.
    // Let's take day 2 to 5 -> indices [1, 4] -> prices: 98.2, 110.0, 105.3, 101.5
    // Sorted window: 98.2, 101.5, 105.3, 110.0
    // 2nd lowest should be 101.5
    
    int version_prev = 1; // Prior to start of window index 1 (meaning it contains only prices[0])
    int version_curr = 5; // End of window, contains up to prices[4]
    
    int k = 2;
    int kth_compressed = pst.query_kth(version_prev, version_curr, k);
    float kth_price = compressor.get_original(kth_compressed);
    
    cout << "2nd lowest price between day 2 and 5: " << kth_price << " (Expected: 101.5)" << endl;

    // Q2: How many days had a price <= 102.0 in the whole range?
    // versions 0 to 7. Window length = 7.
    // Elements <= 102.0 in total array: 101.5, 98.2, 101.5, 99.0 (total 4 elements)
    int max_val_compressed = compressor.get_compressed_less_equal(102.0); 
    
    int count_leq = pst.query_count_less_equal(0, pst.num_versions(), max_val_compressed);
    
    cout << "Days with price <= 102.0: " << count_leq << " (Expected: 4)" << endl;

    return 0;
}
