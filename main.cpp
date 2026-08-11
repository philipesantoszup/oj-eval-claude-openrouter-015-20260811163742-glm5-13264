#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>

using namespace std;

const string DATA_FILE = "storage.dat";
const int BUCKET_COUNT = 10007;

// Hash function
inline uint32_t hash_func(const string& s) {
    uint32_t h = 5381;
    for (char c : s) h = ((h << 5) + h) + (unsigned char)c;
    return h % BUCKET_COUNT;
}

// Initialize file
void init_file() {
    ifstream test(DATA_FILE, ios::binary);
    if (test.good()) { test.close(); return; }
    test.close();

    ofstream f(DATA_FILE, ios::binary);
    // Directory: BUCKET_COUNT offsets, each pointing to end of that bucket's data
    // Initially all buckets start empty (offset = end of directory)
    uint32_t dir_end = BUCKET_COUNT * 4;
    for (int i = 0; i < BUCKET_COUNT; i++) {
        uint32_t offset = 0;  // 0 means empty
        f.write((char*)&offset, 4);
    }
    f.close();
}

// Read all entries for a bucket
vector<pair<string, int>> read_bucket(int bucket) {
    vector<pair<string, int>> entries;

    ifstream f(DATA_FILE, ios::binary);
    if (!f.good()) { f.close(); return entries; }

    uint32_t offset;
    f.seekg(bucket * 4);
    f.read((char*)&offset, 4);

    if (offset == 0) { f.close(); return entries; }

    // Read number of entries
    uint16_t count;
    f.seekg(offset);
    f.read((char*)&count, 2);

    for (int i = 0; i < count && f.good(); i++) {
        uint8_t key_len;
        f.read((char*)&key_len, 1);
        string key(key_len, '\0');
        f.read(&key[0], key_len);
        int32_t value;
        f.read((char*)&value, 4);
        entries.push_back({key, value});
    }

    f.close();
    return entries;
}

// Write entries to a bucket
void write_bucket(int bucket, const vector<pair<string, int>>& entries) {
    fstream f(DATA_FILE, ios::in | ios::out | ios::binary);
    if (!f.good()) { f.close(); return; }

    // Read old offset
    uint32_t old_offset;
    f.seekg(bucket * 4);
    f.read((char*)&old_offset, 4);

    // Calculate new data size
    size_t data_size = 2;  // count
    for (const auto& e : entries) {
        data_size += 1 + e.first.size() + 4;
    }

    // Find end of file for new data
    f.seekg(0, ios::end);
    uint32_t new_offset = f.tellg();

    // Write new data
    f.seekp(new_offset);
    uint16_t count = entries.size();
    f.write((char*)&count, 2);
    for (const auto& e : entries) {
        uint8_t key_len = e.first.size();
        f.write((char*)&key_len, 1);
        f.write(e.first.c_str(), key_len);
        int32_t value = e.second;
        f.write((char*)&value, 4);
    }

    // Update directory
    f.seekp(bucket * 4);
    f.write((char*)&new_offset, 4);

    f.close();
}

// Binary search
int lower_bound_key(const vector<pair<string, int>>& entries, const string& key) {
    int lo = 0, hi = entries.size();
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (entries[mid].first < key) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

// Find values
vector<int> find_values(const string& key) {
    int bucket = hash_func(key);
    auto entries = read_bucket(bucket);

    // Sort for binary search
    sort(entries.begin(), entries.end());

    int pos = lower_bound_key(entries, key);
    vector<int> values;
    while (pos < (int)entries.size() && entries[pos].first == key) {
        values.push_back(entries[pos].second);
        pos++;
    }
    return values;
}

// Insert
void insert_entry(const string& key, int value) {
    int bucket = hash_func(key);
    auto entries = read_bucket(bucket);

    // Sort for binary search
    sort(entries.begin(), entries.end());

    pair<string, int> target{key, value};
    auto it = lower_bound(entries.begin(), entries.end(), target);

    if (it != entries.end() && *it == target) return;  // Duplicate

    entries.insert(it, target);
    write_bucket(bucket, entries);
}

// Delete
void delete_entry(const string& key, int value) {
    int bucket = hash_func(key);
    auto entries = read_bucket(bucket);

    sort(entries.begin(), entries.end());

    pair<string, int> target{key, value};
    auto it = lower_bound(entries.begin(), entries.end(), target);

    if (it != entries.end() && *it == target) {
        entries.erase(it);
        write_bucket(bucket, entries);
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
