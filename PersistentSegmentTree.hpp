#ifndef PERSISTENT_SEGMENT_TREE_H
#define PERSISTENT_SEGMENT_TREE_H

#include <vector>

using namespace std;

// Node structure for the Persistent Segment Tree
struct Node {
    int count;
    int left_child, right_child;
    
    Node() : count(0), left_child(0), right_child(0) {}
};

class PersistentSegmentTree {
private:
    vector<Node> pool;
    vector<int> roots;
    int max_val; // The maximum possible compressed value (M-1)

    // Helper functions for tree traversal
    int build(int tl, int tr);
    int insert(int prev_root, int tl, int tr, int val);
    
    // Internal query helpers
    int query_kth(int root_prev, int root_curr, int tl, int tr, int k) const;
    int query_count_less_equal(int root_prev, int root_curr, int tl, int tr, int x, int y) const;

    // Creates a new node in the pool and returns its index
    int cloneNode(int node_idx);
    int newNode();

public:
    // Initialize tree with range [0, maxValue - 1]
    PersistentSegmentTree(int maxValue);

    // Inserts a compressed value and creates a new version
    // Returns the index of the new root
    int insert(int val);

    // Get the total number of versions stored (ignoring initial base tree)
    int num_versions() const;

    // The k-th smallest element in the sub-array of versions [L, R]
    // versionL_minus_1 is the version index preceding the start of the window
    int query_kth(int versionL_minus_1, int versionR, int k) const;

    // The count of numbers <= max_val_compressed in the window window [L, R]
    int query_count_less_equal(int versionL_minus_1, int versionR, int max_val_compressed) const;
};

#endif
