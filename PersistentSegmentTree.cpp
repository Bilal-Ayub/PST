#include "PersistentSegmentTree.hpp"

using namespace std;

PersistentSegmentTree::PersistentSegmentTree(int maxValue) : max_val(maxValue) {
    // We reserve space to avoid costly reallocations. 
    pool.reserve(20000000); 
    
    // Node 0 represents a null/empty node for convenience.
    // However, our build function explicitly wires everything.
    pool.push_back(Node()); 
    
    if (max_val > 0) {
        int initial_root = build(0, max_val - 1);
        roots.push_back(initial_root);
    } else {
        roots.push_back(0); // empty tree case
    }
}

int PersistentSegmentTree::newNode() {
    pool.push_back(Node());
    return pool.size() - 1;
}

int PersistentSegmentTree::cloneNode(int node_idx) {
    pool.push_back(pool[node_idx]);
    return pool.size() - 1;
}

int PersistentSegmentTree::build(int tl, int tr) {
    int curr = newNode();
    if (tl == tr) {
        return curr;
    }
    int mid = tl + (tr - tl) / 2;
    pool[curr].left_child = build(tl, mid);
    pool[curr].right_child = build(mid + 1, tr);
    return curr;
}

int PersistentSegmentTree::insert(int prev_root, int tl, int tr, int val) {
    int curr = cloneNode(prev_root);
    pool[curr].count++;

    if (tl == tr) {
        return curr;
    }

    int mid = tl + (tr - tl) / 2;
    if (val <= mid) {
        pool[curr].left_child = insert(pool[curr].left_child, tl, mid, val);
    } else {
        pool[curr].right_child = insert(pool[curr].right_child, mid + 1, tr, val);
    }

    return curr;
}

int PersistentSegmentTree::insert(int val) {
    if (max_val == 0) return 0; // Guard against empty init
    int prev_root = roots.back();
    int new_root = insert(prev_root, 0, max_val - 1, val);
    roots.push_back(new_root);
    return new_root;
}

int PersistentSegmentTree::num_versions() const {
    return roots.size() - 1; // 0th is the empty base version
}

int PersistentSegmentTree::query_kth(int root_prev, int root_curr, int tl, int tr, int k) const {
    if (tl == tr) {
        return tl;
    }

    int countL_prev = pool[pool[root_prev].left_child].count;
    int countL_curr = pool[pool[root_curr].left_child].count;
    int count_in_left_subtree = countL_curr - countL_prev;

    int mid = tl + (tr - tl) / 2;
    if (count_in_left_subtree >= k) {
        return query_kth(pool[root_prev].left_child, pool[root_curr].left_child, tl, mid, k);
    } else {
        return query_kth(pool[root_prev].right_child, pool[root_curr].right_child, mid + 1, tr, k - count_in_left_subtree);
    }
}

int PersistentSegmentTree::query_kth(int versionL_minus_1, int versionR, int k) const {
    // Basic bounds check
    if (versionL_minus_1 < 0 || versionR >= roots.size()) return -1;
    
    return query_kth(roots[versionL_minus_1], roots[versionR], 0, max_val - 1, k);
}

int PersistentSegmentTree::query_count_less_equal(int root_prev, int root_curr, int tl, int tr, int x, int y) const {
    if (tl > y || tr < x) {
        return 0; // completely outside query range
    }
    if (x <= tl && tr <= y) {
        return pool[root_curr].count - pool[root_prev].count; // completely inside
    }
    int mid = tl + (tr - tl) / 2;
    return query_count_less_equal(pool[root_prev].left_child, pool[root_curr].left_child, tl, mid, x, y) +
           query_count_less_equal(pool[root_prev].right_child, pool[root_curr].right_child, mid + 1, tr, x, y);
}

int PersistentSegmentTree::query_count_less_equal(int versionL_minus_1, int versionR, int max_val_compressed) const {
    if (max_val_compressed < 0) return 0; // value is smaller than minimum
    return query_count_less_equal(roots[versionL_minus_1], roots[versionR], 0, max_val - 1, 0, max_val_compressed);
}
