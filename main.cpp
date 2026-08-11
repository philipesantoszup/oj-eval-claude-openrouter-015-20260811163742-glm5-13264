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
const int BUCKET_COUNT = 1009; // Prime number for better hash distribution
const int MAX_KEY_SIZE = 64;

// Hash function (djb2)
uint32_t hash_func(const string& s) {
    uint32_t hash = 5381;
    for (char c : s) {
        hash = ((hash << 5) + hash) + (unsigned char)c;
    }
    return hash % BUCKET_COUNT;
}

struct Entry {
    string key;
    int value;
};

// Read all entries from a bucket
vector<Entry> read_bucket(int bucket) {
    vector<Entry> entries;

    ifstream file(DATA_FILE, ios::binary);
    if (!file) return entries;

    // Bucket offset = header_size + bucket * BLOCK_SIZE
    streamoff header_end = BUCKET_COUNT * 4;
    streamoff bucket_block_start = header_end + (streamoff)bucket * BLOCK_SIZE;

    file.seekg(bucket_block_start);

    // Read number of entries in this block
    uint16_t count;
    file.read((char*)&count, sizeof(count));
    if (!file) {
        file.close();
        return entries;
    }

    // Read next block offset (0 if no next)
    uint32_t next_block;
    file.read((char*)&next_block, sizeof(next_block));

    // Read entries
    for (int i = 0; i < count; i++) {
        uint8_t key_len;
        file.read((char*)&key_len, sizeof(key_len));
        if (!file) break;

        string key(key_len, '\0');
        file.read(&key[0], key_len);
        if (!file) break;

        int32_t value;
        file.read((char*)&value, sizeof(value));
        if (!file) break;

        entries.push_back({key, value});
    }

    // Read overflow blocks
    while (next_block != 0) {
        file.seekg(next_block);

        file.read((char*)&count, sizeof(count));
        if (!file) break;
        file.read((char*)&next_block, sizeof(next_block));
        if (!file) break;

        for (int i = 0; i < count; i++) {
            uint8_t key_len;
            file.read((char*)&key_len, sizeof(key_len));
            if (!file) break;

            string key(key_len, '\0');
            file.read(&key[0], key_len);
            if (!file) break;

            int32_t value;
            file.read((char*)&value, sizeof(value));
            if (!file) break;

            entries.push_back({key, value});
        }
    }

    file.close();
    return entries;
}

// Clear a bucket block (write zeros)
void clear_block(streamoff block_start) {
    fstream file(DATA_FILE, ios::in | ios::out | ios::binary);
    if (!file) return;

    file.seekp(block_start);

    // Write count = 0
    uint16_t count = 0;
    file.write((char*)&count, sizeof(count));

    // Write next_block = 0
    uint32_t next_block = 0;
    file.write((char*)&next_block, sizeof(next_block));

    file.close();
}

// Write entries to a bucket
void write_bucket(int bucket, const vector<Entry>& entries) {
    streamoff header_end = BUCKET_COUNT * 4;
    streamoff bucket_block_start = header_end + (streamoff)bucket * BLOCK_SIZE;

    // First, clear the initial block
    clear_block(bucket_block_start);

    if (entries.empty()) {
        return; // Nothing more to do
    }

    fstream file(DATA_FILE, ios::in | ios::out | ios::binary);
    if (!file) return;

    size_t idx = 0;
    streamoff current_block = bucket_block_start;

    while (idx < entries.size()) {
        file.seekp(current_block);
        file.clear();

        // Count entries that fit in this block
        size_t start_idx = idx;
        size_t bytes_used = 2 + 4; // count + next_block
        size_t count = 0;

        while (idx < entries.size()) {
            size_t entry_size = 1 + entries[idx].key.size() + 4;
            if (bytes_used + entry_size > (size_t)BLOCK_SIZE) break;
            bytes_used += entry_size;
            idx++;
            count++;
        }

        // Write count
        uint16_t cnt = count;
        file.write((char*)&cnt, sizeof(cnt));

        // Write next block (0 for now, will update if needed)
        uint32_t next_block = 0;
        streamoff next_block_pos = file.tellp();
        file.write((char*)&next_block, sizeof(next_block));

        // Write entries
        for (size_t i = start_idx; i < start_idx + count; i++) {
            uint8_t key_len = entries[i].key.size();
            file.write((char*)&key_len, sizeof(key_len));
            file.write(entries[i].key.c_str(), key_len);
            file.write((char*)&entries[i].value, sizeof(entries[i].value));
        }

        // If more entries, allocate next block
        if (idx < entries.size()) {
            // Find end of file for next block
            file.seekp(0, ios::end);
            streamoff end_pos = file.tellp();

            // Align to BLOCK_SIZE
            streamoff next_pos = ((end_pos + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
            next_block = (uint32_t)next_pos;

            // Write next_block pointer
            file.seekp(next_block_pos);
            file.write((char*)&next_block, sizeof(next_block));

            current_block = next_pos;
        }
    }

    file.close();
}

// Initialize file if it doesn't exist
void init_file() {
    ifstream test(DATA_FILE, ios::binary);
    if (test.good()) {
        test.close();
        return;
    }
    test.close();

    ofstream file(DATA_FILE, ios::binary);

    // Write header: bucket block offsets
    for (int i = 0; i < BUCKET_COUNT; i++) {
        uint32_t offset = BUCKET_COUNT * 4 + i * BLOCK_SIZE;
        file.write((char*)&offset, sizeof(offset));
    }

    // Initialize each bucket block
    for (int i = 0; i < BUCKET_COUNT; i++) {
        streamoff pos = BUCKET_COUNT * 4 + (streamoff)i * BLOCK_SIZE;
        file.seekp(pos);

        uint16_t count = 0;
        file.write((char*)&count, sizeof(count));

        uint32_t next_block = 0;
        file.write((char*)&next_block, sizeof(next_block));
    }

    file.close();
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

            int bucket = hash_func(index);

            // Read existing entries
            auto entries = read_bucket(bucket);

            // Check if already exists
            bool exists = false;
            for (const auto& e : entries) {
                if (e.key == index && e.value == value) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                entries.push_back({index, value});
                write_bucket(bucket, entries);
            }
        } else if (cmd == "delete") {
            string index;
            int value;
            cin >> index >> value;

            int bucket = hash_func(index);

            auto entries = read_bucket(bucket);

            // Find and remove entry
            bool found = false;
            auto it = entries.begin();
            while (it != entries.end()) {
                if (it->key == index && it->value == value) {
                    it = entries.erase(it);
                    found = true;
                    break;
                } else {
                    ++it;
                }
            }

            if (found) {
                write_bucket(bucket, entries);
            }
        } else if (cmd == "find") {
            string index;
            cin >> index;

            int bucket = hash_func(index);

            auto entries = read_bucket(bucket);

            vector<int> values;
            for (const auto& e : entries) {
                if (e.key == index) {
                    values.push_back(e.value);
                }
            }

            sort(values.begin(), values.end());

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
