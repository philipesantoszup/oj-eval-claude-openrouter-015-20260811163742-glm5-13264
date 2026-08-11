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
const int BUCKET_COUNT = 10007;

struct Entry {
    string key;
    int value;
};

// Hash function
inline uint32_t hash_func(const string& s) {
    uint32_t h = 5381;
    for (char c : s) {
        h = ((h << 5) + h) + (unsigned char)c;
    }
    return h % BUCKET_COUNT;
}

// Initialize file
void init_file() {
    ifstream test(DATA_FILE, ios::binary);
    if (test.good()) {
        test.close();
        return;
    }
    test.close();

    ofstream f(DATA_FILE, ios::binary);
    // Header: first_free(4) + bucket offsets (BUCKET_COUNT * 4)
    uint32_t first_free = 4 + BUCKET_COUNT * 4;
    f.write((char*)&first_free, 4);

    for (int i = 0; i < BUCKET_COUNT; i++) {
        uint32_t offset = first_free + i * BLOCK_SIZE;
        f.write((char*)&offset, 4);
    }

    // Initialize bucket blocks
    for (int i = 0; i < BUCKET_COUNT; i++) {
        f.seekp(first_free + i * BLOCK_SIZE);
        uint16_t count = 0;
        f.write((char*)&count, 2);
    }

    f.close();
}

// Read bucket entries
vector<Entry> read_bucket(int bucket) {
    vector<Entry> entries;

    ifstream f(DATA_FILE, ios::binary);
    if (!f.good()) {
        f.close();
        return entries;
    }

    // Read bucket offset from directory
    uint32_t offset;
    f.seekg(4 + bucket * 4);
    f.read((char*)&offset, 4);

    // Read count
    f.seekg(offset);
    uint16_t count;
    f.read((char*)&count, 2);

    // Read entries
    for (int i = 0; i < count && f.good(); i++) {
        Entry e;
        uint8_t key_len;
        f.read((char*)&key_len, 1);
        e.key.resize(key_len);
        f.read(&e.key[0], key_len);
        f.read((char*)&e.value, 4);
        entries.push_back(e);
    }

    f.close();
    return entries;
}

// Write bucket entries
void write_bucket(int bucket, const vector<Entry>& entries) {
    fstream f(DATA_FILE, ios::in | ios::out | ios::binary);
    if (!f.good()) {
        f.close();
        return;
    }

    // Read bucket offset
    uint32_t offset;
    f.seekg(4 + bucket * 4);
    f.read((char*)&offset, 4);

    // Write count and entries
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

// Find values
vector<int> find_values(const string& key) {
    vector<int> values;

    int bucket = hash_func(key);
    auto entries = read_bucket(bucket);

    for (const auto& e : entries) {
        if (e.key == key) {
            values.push_back(e.value);
        }
    }

    sort(values.begin(), values.end());
    return values;
}

// Insert entry
void insert_entry(const string& key, int value) {
    int bucket = hash_func(key);
    auto entries = read_bucket(bucket);

    // Check duplicate
    for (const auto& e : entries) {
        if (e.key == key && e.value == value) {
            return;
        }
    }

    entries.push_back({key, value});

    // Sort for binary search
    sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b) {
            if (a.key != b.key) return a.key < b.key;
            return a.value < b.value;
        });

    write_bucket(bucket, entries);
}

// Delete entry
void delete_entry(const string& key, int value) {
    int bucket = hash_func(key);
    auto entries = read_bucket(bucket);

    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (it->key == key && it->value == value) {
            entries.erase(it);
            break;
        }
    }

    write_bucket(bucket, entries);
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
