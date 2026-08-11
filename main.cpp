#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>

using namespace std;

const string DATA_FILE = "storage.dat";
const int BLOCK_SIZE = 4096;
const int MAX_KEY_SIZE = 64;

// B+ tree order - can fit ~40 keys per internal node, ~30 entries per leaf
const int ORDER = 32;

struct Entry {
    string key;
    int value;

    bool operator<(const Entry& other) const {
        if (key != other.key) return key < other.key;
        return value < other.value;
    }
    bool operator==(const Entry& other) const {
        return key == other.key && value == other.value;
    }
};

// Initialize empty file with a single leaf root
void init_file() {
    ifstream test(DATA_FILE, ios::binary);
    if (test.good()) {
        test.close();
        return;
    }
    test.close();

    ofstream f(DATA_FILE, ios::binary);

    // Header: root(4) + next_free(4)
    uint32_t root = 8;
    uint32_t next_free = 8 + BLOCK_SIZE;
    f.write((char*)&root, 4);
    f.write((char*)&next_free, 4);

    // Empty root leaf node
    // Format: type(1) + count(2) + entries...
    f.seekp(8);
    uint8_t type = 0;  // 0 = leaf
    f.write((char*)&type, 1);
    uint16_t count = 0;
    f.write((char*)&count, 2);

    f.close();
}

// Read root offset
uint32_t get_root() {
    ifstream f(DATA_FILE, ios::binary);
    uint32_t root;
    f.read((char*)&root, 4);
    f.close();
    return root;
}

// Write root offset
void set_root(uint32_t root) {
    fstream f(DATA_FILE, ios::in | ios::out | ios::binary);
    f.seekp(0);
    f.write((char*)&root, 4);
    f.close();
}

// Get next free block and increment
uint32_t alloc_block() {
    fstream f(DATA_FILE, ios::in | ios::out | ios::binary);
    uint32_t root, next_free;
    f.read((char*)&root, 4);
    f.read((char*)&next_free, 4);
    uint32_t allocated = next_free;
    next_free += BLOCK_SIZE;
    f.seekp(4);
    f.write((char*)&next_free, 4);
    f.close();
    return allocated;
}

// Read entry from stream
Entry read_entry(istream& f) {
    Entry e;
    uint8_t len;
    f.read((char*)&len, 1);
    e.key.resize(len);
    f.read(&e.key[0], len);
    f.read((char*)&e.value, 4);
    return e;
}

// Write entry to stream
void write_entry(ostream& f, const Entry& e) {
    uint8_t len = e.key.size();
    f.write((char*)&len, 1);
    f.write(e.key.c_str(), len);
    f.write((char*)&e.value, 4);
}

int entry_size(const string& key) {
    return 1 + key.size() + 4;
}

// Leaf node structure: type(1) + count(2) + entries...
// Internal node: type(1) + count(2) + child0(4) + key0 + child1 + key1 + ...

// Read leaf entries
vector<Entry> read_leaf(uint32_t offset) {
    vector<Entry> entries;
    ifstream f(DATA_FILE, ios::binary);
    f.seekg(offset);
    uint8_t type;
    f.read((char*)&type, 1);
    if (type != 0) return entries;

    uint16_t count;
    f.read((char*)&count, 2);

    for (int i = 0; i < count; i++) {
        entries.push_back(read_entry(f));
    }
    f.close();
    return entries;
}

// Write leaf entries
void write_leaf(uint32_t offset, const vector<Entry>& entries) {
    fstream f(DATA_FILE, ios::in | ios::out | ios::binary);
    f.seekp(offset);
    uint8_t type = 0;
    f.write((char*)&type, 1);
    uint16_t count = entries.size();
    f.write((char*)&count, 2);
    for (const auto& e : entries) {
        write_entry(f, e);
    }
    f.close();
}

// Internal node operations
struct InternalKey {
    string key;
    int value;  // sentinel, always INT_MIN for internal keys
};

struct InternalNode {
    vector<uint32_t> children;
    vector<string> keys;
};

InternalNode read_internal(uint32_t offset) {
    InternalNode node;
    ifstream f(DATA_FILE, ios::binary);
    f.seekg(offset);
    uint8_t type;
    f.read((char*)&type, 1);
    if (type != 1) return node;

    uint16_t count;
    f.read((char*)&count, 2);

    for (int i = 0; i <= count; i++) {
        uint32_t child;
        f.read((char*)&child, 4);
        node.children.push_back(child);
    }

    for (int i = 0; i < count; i++) {
        uint8_t len;
        f.read((char*)&len, 1);
        string key(len, '\0');
        f.read(&key[0], len);
        node.keys.push_back(key);
    }
    f.close();
    return node;
}

void write_internal(uint32_t offset, const InternalNode& node) {
    fstream f(DATA_FILE, ios::in | ios::out | ios::binary);
    f.seekp(offset);
    uint8_t type = 1;
    f.write((char*)&type, 1);
    uint16_t count = node.keys.size();
    f.write((char*)&count, 2);

    for (uint32_t child : node.children) {
        f.write((char*)&child, 4);
    }

    for (const string& key : node.keys) {
        uint8_t len = key.size();
        f.write((char*)&len, 1);
        f.write(key.c_str(), len);
    }
    f.close();
}

// Create new leaf node
uint32_t create_leaf(const vector<Entry>& entries) {
    uint32_t offset = alloc_block();
    write_leaf(offset, entries);
    return offset;
}

// Create new internal node
uint32_t create_internal(const vector<uint32_t>& children, const vector<string>& keys) {
    uint32_t offset = alloc_block();
    InternalNode node{children, keys};
    write_internal(offset, node);
    return offset;
}

// Check if offset points to leaf
bool is_leaf(uint32_t offset) {
    ifstream f(DATA_FILE, ios::binary);
    f.seekg(offset);
    uint8_t type;
    f.read((char*)&type, 1);
    f.close();
    return type == 0;
}

// Find lower bound position for key in leaf entries
int leaf_lower_bound(const vector<Entry>& entries, const string& key) {
    int lo = 0, hi = entries.size();
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (entries[mid].key < key) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

// Find child index in internal node
int find_child(const InternalNode& node, const string& key) {
    int idx = 0;
    for (int i = 0; i < (int)node.keys.size(); i++) {
        if (key >= node.keys[i]) idx = i + 1;
        else break;
    }
    return idx;
}

// Find values for key by traversing tree
vector<int> find_values(uint32_t root, const string& key) {
    vector<int> values;
    uint32_t current = root;

    while (true) {
        if (is_leaf(current)) {
            auto entries = read_leaf(current);
            int pos = leaf_lower_bound(entries, key);
            while (pos < (int)entries.size() && entries[pos].key == key) {
                values.push_back(entries[pos].value);
                pos++;
            }
            break;
        } else {
            auto node = read_internal(current);
            int idx = find_child(node, key);
            current = node.children[idx];
        }
    }

    sort(values.begin(), values.end());
    return values;
}

// Insert into leaf, handling split
// Returns {new_key, new_leaf_offset} if split occurred, {"", 0} otherwise
pair<string, uint32_t> insert_into_leaf(uint32_t offset, const Entry& entry) {
    auto entries = read_leaf(offset);

    // Check duplicate
    for (const auto& e : entries) {
        if (e.key == entry.key && e.value == entry.value) {
            return {"", 0};  // Duplicate, no action needed
        }
    }

    // Insert sorted
    auto it = lower_bound(entries.begin(), entries.end(), entry);
    entries.insert(it, entry);

    // Check if we need to split
    int total_size = 3;  // header
    for (const auto& e : entries) {
        total_size += entry_size(e.key);
    }

    if (total_size <= BLOCK_SIZE) {
        write_leaf(offset, entries);
        return {"", 0};
    }

    // Split the leaf
    int mid = entries.size() / 2;
    vector<Entry> left(entries.begin(), entries.begin() + mid);
    vector<Entry> right(entries.begin() + mid, entries.end());

    write_leaf(offset, left);
    uint32_t new_offset = create_leaf(right);

    return {right[0].key, new_offset};
}

// Insert into internal node recursively
pair<string, uint32_t> insert_into_internal(uint32_t offset, const Entry& entry) {
    auto node = read_internal(offset);
    int idx = find_child(node, entry.key);

    uint32_t child = node.children[idx];
    pair<string, uint32_t> split_result;

    if (is_leaf(child)) {
        split_result = insert_into_leaf(child, entry);
    } else {
        split_result = insert_into_internal(child, entry);
    }

    if (split_result.second == 0) {
        return {"", 0};  // No split occurred
    }

    // Child was split, insert new key and child
    node.keys.insert(node.keys.begin() + idx, split_result.first);
    node.children.insert(node.children.begin() + idx + 1, split_result.second);

    // Check if internal node needs to split
    int total_size = 3 + 4;  // header + first child
    for (size_t i = 0; i < node.keys.size(); i++) {
        total_size += 4;  // child pointer
        total_size += 1 + node.keys[i].size();  // key
    }

    if (total_size <= BLOCK_SIZE) {
        write_internal(offset, node);
        return {"", 0};
    }

    // Split internal node
    int mid = node.keys.size() / 2;
    string mid_key = node.keys[mid];

    vector<string> left_keys(node.keys.begin(), node.keys.begin() + mid);
    vector<uint32_t> left_children(node.children.begin(), node.children.begin() + mid + 1);

    vector<string> right_keys(node.keys.begin() + mid + 1, node.keys.end());
    vector<uint32_t> right_children(node.children.begin() + mid + 1, node.children.end());

    write_internal(offset, InternalNode{left_children, left_keys});
    uint32_t new_offset = create_internal(right_children, right_keys);

    return {mid_key, new_offset};
}

// Insert entry into tree
void insert_entry(uint32_t root, const Entry& entry) {
    pair<string, uint32_t> split_result;

    if (is_leaf(root)) {
        split_result = insert_into_leaf(root, entry);
    } else {
        split_result = insert_into_internal(root, entry);
    }

    if (split_result.second != 0) {
        // Root was split, create new root
        uint32_t new_root = create_internal({root, split_result.second}, {split_result.first});
        set_root(new_root);
    }
}

// Find values helper
vector<int> find_values_helper(const string& key) {
    return find_values(get_root(), key);
}

// Insert helper
void insert_helper(const string& key, int value) {
    insert_entry(get_root(), Entry{key, value});
}

// Delete from leaf
// Returns true if entry was found and deleted
bool delete_from_leaf(uint32_t offset, const string& key, int value) {
    auto entries = read_leaf(offset);

    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (it->key == key && it->value == value) {
            entries.erase(it);
            write_leaf(offset, entries);
            return true;
        }
    }
    return false;
}

// Delete from tree (simple version - doesn't rebalance)
bool delete_from_tree(uint32_t offset, const string& key, int value) {
    if (is_leaf(offset)) {
        return delete_from_leaf(offset, key, value);
    }

    auto node = read_internal(offset);
    int idx = find_child(node, key);

    return delete_from_tree(node.children[idx], key, value);
}

// Delete helper
void delete_helper(const string& key, int value) {
    delete_from_tree(get_root(), key, value);
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    init_file();

    int n;
    cin >> n;

    while (n--) {
        string cmd;
        cin >> cmd;

        if (cmd == "insert") {
            string index;
            int value;
            cin >> index >> value;
            insert_helper(index, value);
        } else if (cmd == "delete") {
            string index;
            int value;
            cin >> index >> value;
            delete_helper(index, value);
        } else if (cmd == "find") {
            string index;
            cin >> index;
            auto values = find_values_helper(index);
            if (values.empty()) {
                cout << "null\n";
            } else {
                for (size_t i = 0; i < values.size(); i++) {
                    if (i > 0) cout << ' ';
                    cout << values[i];
                }
                cout << '\n';
            }
        }
    }

    return 0;
}
