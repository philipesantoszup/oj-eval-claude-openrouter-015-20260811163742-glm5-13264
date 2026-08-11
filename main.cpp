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
const int BUCKET_COUNT = 10007;  // Prime number for better distribution

// Hash function
uint32_t hash_func(const string& s) {
    uint32_t h = 5381;
    for (char c : s) {
        h = ((h << 5) + h) + (unsigned char)c;
    }
    return h % BUCKET_COUNT;
}

struct Entry {
    string key;
    int value;
};

// Initialize file with bucket headers
void init_file() {
    ifstream test(DATA_FILE, ios::binary);
    if (test.good()) {
        test.close();
        return;
    }
    test.close();

    ofstream f(DATA_FILE, ios::binary);

    // Header: first_free_block(4)
    uint32_t first_free = BUCKET_COUNT * BLOCK_SIZE + 4;
    f.write((char*)&first_free, 4);

    // Initialize all bucket blocks as empty
    for (int i = 0; i < BUCKET_COUNT; i++) {
        f.seekp(4 + i * BLOCK_SIZE);
        uint16_t count = 0;
        f.write((char*)&count, 2);
    }

    f.close();
}

// Read entries from a bucket
vector<Entry> read_bucket(int bucket) {
    vector<Entry> entries;

    ifstream f(DATA_FILE, ios::binary);
    if (!f.good()) {
        f.close();
        return entries;
    }

    uint32_t offset = 4 + bucket * BLOCK_SIZE;
    f.seekg(offset);

    uint16_t count;
    f.read((char*)&count, 2);

    for (int i = 0; i < count && f.good(); i++) {
        Entry e;
        uint8_t key_len;
        f.read((char*)&key_len, 1);
        if (!f.good()) break;
        e.key.resize(key_len);
        f.read(&e.key[0], key_len);
        f.read((char*)&e.value, 4);
        entries.push_back(e);
    }

    f.close();
    return entries;
}

// Write entries to a bucket (single block)
void write_bucket(int bucket, const vector<Entry>& entries) {
    fstream f(DATA_FILE, ios::in | ios::out | ios::binary);
    if (!f.good()) {
        f.close();
        return;
    }

    uint32_t offset = 4 + bucket * BLOCK_SIZE;
    f.seekp(offset);

    uint16_t count = entries.size();
    f.write((char*)&count, 2);

    for (const auto& e : entries) {
        uint8_t key_len = e.key.size();
        f.write((char*)&key_len, 1);
        f.write(e.key.c_str(), key_len);
        f.write((char*)&e.value, 4);
    }

    f.close();
}

// Binary search for lower bound of key in sorted entries
int lower_bound_key(const vector<Entry>& entries, const string& key) {
    int lo = 0, hi = entries.size();
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (entries[mid].key < key) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

// Find values for a key using binary search
vector<int> find_values(const string& key) {
    vector<int> values;

    int bucket = hash_func(key);
    auto entries = read_bucket(bucket);

    // Binary search for the key
    int pos = lower_bound_key(entries, key);
    while (pos < (int)entries.size() && entries[pos].key == key) {
        values.push_back(entries[pos].value);
        pos++;
    }

    // Values are already sorted because entries are sorted
    return values;
}

// Check if entry exists using binary search
bool entry_exists(const vector<Entry>& entries, const string& key, int value) {
    int pos = lower_bound_key(entries, key);
    while (pos < (int)entries.size() && entries[pos].key == key) {
        if (entries[pos].value == value) return true;
        if (entries[pos].value > value) break;  // Values are sorted
        pos++;
    }
    return false;
}

// Insert entry maintaining sorted order
void insert_entry(const string& key, int value) {
    int bucket = hash_func(key);
    auto entries = read_bucket(bucket);

    // Check duplicate
    if (entry_exists(entries, key, value)) {
        return;
    }

    // Find insertion point
    Entry new_entry{key, value};
    auto it = lower_bound(entries.begin(), entries.end(), new_entry,
        [](const Entry& a, const Entry& b) {
            if (a.key != b.key) return a.key < b.key;
            return a.value < b.value;
        });
    entries.insert(it, new_entry);

    write_bucket(bucket, entries);
}

// Delete entry
void delete_entry(const string& key, int value) {
    int bucket = hash_func(key);
    auto entries = read_bucket(bucket);

    // Find and delete using binary search
    int pos = lower_bound_key(entries, key);
    while (pos < (int)entries.size() && entries[pos].key == key) {
        if (entries[pos].value == value) {
            entries.erase(entries.begin() + pos);
            write_bucket(bucket, entries);
            return;
        }
        if (entries[pos].value > value) break;  // Values are sorted
        pos++;
    }
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
            insert_entry(index, value);
        } else if (cmd == "delete") {
            string index;
            int value;
            cin >> index >> value;
            delete_entry(index, value);
        } else if (cmd == "find") {
            string index;
            cin >> index;
            auto values = find_values(index);
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
