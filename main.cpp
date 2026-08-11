#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <unordered_map>

using namespace std;

const string DATA_FILE = "storage.dat";
const int BLOCK_SIZE = 4096;
const int BUCKET_COUNT = 10007;

// LRU cache for buckets
class BucketCache {
    static const int CACHE_SIZE = 256;
    unordered_map<int, vector<pair<string, int>>> cache;
    vector<int> lru_order;

public:
    bool get(int bucket, vector<pair<string, int>>& entries) {
        auto it = cache.find(bucket);
        if (it != cache.end()) {
            entries = it->second;
            // Move to front of LRU
            lru_order.erase(remove(lru_order.begin(), lru_order.end(), bucket), lru_order.end());
            lru_order.insert(lru_order.begin(), bucket);
            return true;
        }
        return false;
    }

    void put(int bucket, const vector<pair<string, int>>& entries) {
        if (cache.size() >= CACHE_SIZE) {
            // Evict least recently used
            int evict = lru_order.back();
            lru_order.pop_back();
            cache.erase(evict);
        }
        cache[bucket] = entries;
        lru_order.insert(lru_order.begin(), bucket);
    }

    void invalidate(int bucket) {
        cache.erase(bucket);
        lru_order.erase(remove(lru_order.begin(), lru_order.end(), bucket), lru_order.end());
    }
};

BucketCache cache;

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
    uint32_t first_free = 4 + BUCKET_COUNT * 4;
    f.write((char*)&first_free, 4);

    for (int i = 0; i < BUCKET_COUNT; i++) {
        uint32_t offset = first_free + i * BLOCK_SIZE;
        f.write((char*)&offset, 4);
    }

    for (int i = 0; i < BUCKET_COUNT; i++) {
        f.seekp(first_free + i * BLOCK_SIZE);
        uint16_t count = 0;
        f.write((char*)&count, 2);
    }

    f.close();
}

// Read bucket entries
vector<pair<string, int>> read_bucket_raw(int bucket) {
    vector<pair<string, int>> entries;

    ifstream f(DATA_FILE, ios::binary);
    if (!f.good()) {
        f.close();
        return entries;
    }

    uint32_t offset;
    f.seekg(4 + bucket * 4);
    f.read((char*)&offset, 4);

    f.seekg(offset);
    uint16_t count;
    f.read((char*)&count, 2);

    for (int i = 0; i < count && f.good(); i++) {
        string key;
        int value;
        uint8_t key_len;
        f.read((char*)&key_len, 1);
        key.resize(key_len);
        f.read(&key[0], key_len);
        f.read((char*)&value, 4);
        entries.push_back({key, value});
    }

    f.close();
    return entries;
}

// Write bucket entries
void write_bucket_raw(int bucket, const vector<pair<string, int>>& entries) {
    fstream f(DATA_FILE, ios::in | ios::out | ios::binary);
    if (!f.good()) {
        f.close();
        return;
    }

    uint32_t offset;
    f.seekg(4 + bucket * 4);
    f.read((char*)&offset, 4);

    f.seekp(offset);
    uint16_t count = entries.size();
    f.write((char*)&count, 2);

    for (const auto& [key, value] : entries) {
        uint8_t key_len = key.size();
        f.write((char*)&key_len, 1);
        f.write(key.c_str(), key_len);
        f.write((char*)&value, 4);
    }

    f.close();
    cache.invalidate(bucket);
}

// Get bucket entries with caching
vector<pair<string, int>> get_bucket(int bucket) {
    vector<pair<string, int>> entries;
    if (cache.get(bucket, entries)) {
        return entries;
    }
    entries = read_bucket_raw(bucket);
    cache.put(bucket, entries);
    return entries;
}

// Save bucket entries
void save_bucket(int bucket, const vector<pair<string, int>>& entries) {
    write_bucket_raw(bucket, entries);
    cache.put(bucket, entries);
}

// Binary search for lower bound
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
    vector<int> values;

    int bucket = hash_func(key);
    auto entries = get_bucket(bucket);

    int pos = lower_bound_key(entries, key);
    while (pos < (int)entries.size() && entries[pos].first == key) {
        values.push_back(entries[pos].second);
        pos++;
    }

    return values;
}

// Insert entry
void insert_entry(const string& key, int value) {
    int bucket = hash_func(key);
    auto entries = get_bucket(bucket);

    // Find position
    pair<string, int> target{key, value};
    auto it = lower_bound(entries.begin(), entries.end(), target);

    // Check duplicate
    if (it != entries.end() && it->first == key && it->second == value) {
        return;
    }

    entries.insert(it, target);
    save_bucket(bucket, entries);
}

// Delete entry
void delete_entry(const string& key, int value) {
    int bucket = hash_func(key);
    auto entries = get_bucket(bucket);

    pair<string, int> target{key, value};
    auto it = lower_bound(entries.begin(), entries.end(), target);

    if (it != entries.end() && it->first == key && it->second == value) {
        entries.erase(it);
        save_bucket(bucket, entries);
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
