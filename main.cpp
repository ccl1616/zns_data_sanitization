#include <iostream>
#include <set>
#include <cmath>
#include <random>
#include <algorithm>    // swap()
#include <vector>
#include <string>
#include <map>
#include <fstream>

using namespace std;
int Maxlba;
int KPP;    // Key Per Page

// Data Structure
enum Status {valid, updated, invalid};
class Key
{
public:
    int begin, end, level;  // lba begin & end that the key take care of
    int page_id;
    Status st;
    Key(int x, int y, int l, Status s, int pid) :
        begin(x),
        end(y),
        level(l),
        st(s),
        page_id(pid) {}
    Key(int x, int y, int l, Status s) :
        begin(x),
        end(y),
        level(l),
        st(s) {}
    Key(int x, int y, int l) :
        begin(x),
        end(y),
        level(l) {}
    Key(int x, int y) : begin(x), end(y) {}
    Key() : begin(0), end(0), level(0) {}

    bool operator<(const Key& other) const {
        if (level != other.level) return level < other.level;
        if (begin != other.begin) return begin < other.begin;
        return end < other.end;
    }

    bool operator==(const Key& other) const {
        return begin == other.begin && end == other.end && level == other.level;
    }
};
// cout overload
ostream& operator << (ostream &out, const Key &i)
{
    cout << "(" << i.begin << "," << i.end << ")" << "- " << i.level << "|" << i.st << "|" << i.page_id;
    return out;
}
// ======================================================================
// ======================================================================
// Functions
int construct_tree(map<int, set<Key>> &tree);  // given maxlba id, push keys into set tree; return cur_key_page_id
void traverse_tree(map<int, set<Key>> m);  // traverse and print x
// void key2root(int x, int y, int l, set<Key> &k);   // given Key(x,y), push all its upper keys(<= level2) into set k
pair<int, int> cmd_gen(int size);  // given size, return valid data id by the chunk size
int rand_gen(int min, int max); // pure rand_gen of range [min, max]
pair<int, int> find_min_max(vector<int> row);   // given a row, return <min, max> value in a pair
void mark_data(pair<int, int> data, map<int, set<Key>> &tree);
void upward_update(int l, map<int, set<Key>> &tree);
int downward_update(map<int, set<Key>> &tree);     // downward remove updated tags and return #updated keys
void update_key_status(map<int, set<Key>> &tree, pair<int, int> data, int l, Status new_status);

// structure
int construct_tree(map<int, set<Key>> &tree)
{
    int L = ceil(log2(Maxlba+1) / log2(KPP)) + 1;   // total level num
    int MLI = L - 1;    // max level id
    int cur_page_id = 0;
    int cur_page_key_num = 0;
    // build tree
    for(int i = MLI; i >= 0; i --) {
        // size: #lba this key covered
        // offset: offset for begin & end of one key
        int size = pow(KPP, MLI - i);
        int offset = size - 1;
        set<Key> tree_level;
        if(i == MLI) {
            for(int j = 0; j <= Maxlba; j += size)
                tree_level.insert(Key(j, j+offset, i, Status::valid, -1));
        }
        else {
            for(int j = 0; j <= Maxlba; j += size) {
                tree_level.insert(Key(j, j+offset, i, Status::valid, cur_page_id));
                cur_page_key_num ++;
                if(cur_page_key_num == KPP) {
                    cur_page_id ++;
                    cur_page_key_num = 0;
                }
            }
        }
        tree[i] = tree_level;
    }
    return (cur_page_key_num == 0) ?(cur_page_id - 1) : cur_page_id;
}
void traverse_tree(map<int, set<Key>> tree)
{
    int L = ceil(log2(Maxlba+1) / log2(KPP)) + 1;   // total level num
    // print tree, top-bottom
    for(auto i: tree) {
        cout << "level: " << i.first << endl;
        // skip last level
        if(i.first == L - 1) {
            cout << "skip\n";
            break;
        }
        // print each key
        for(auto j: i.second)
            cout << j << endl;
        cout << endl;
    }
}
// void key2root(int x, int y, int l, set<Key> &k)
// {
//     if(x < 0 || y < 0 || l < 0 || x > Maxlba|| y > Maxlba || l == 1) return;
//     else {
//         k.insert(Key(x, y, l));
//         // buttom up recursive
//         // calculate parent's key id
//         int N = ceil(log2(Maxlba+1) / log2(KPP)) + 1;   // total level num
//         int p_size = pow(KPP, N - (l - 1)), p_offset = p_size - 1;
//         int p_x = x / p_size;
//         key2root(p_x * p_size, p_x * p_size + p_offset, l - 1, k);
//         return;
//     }
//     return;
// }

// cmd
pair<int, int> cmd_gen(int size)
{
    int a, b = -1;
    while(b == -1) {
        a = rand_gen(0, Maxlba);   // gen a valid data id
        if( (a + size - 1) <= Maxlba) {
            b = a + size - 1;
            return make_pair(a, b);
        }
    }
    cout << "rand_gen fail\n";
    return make_pair(0, 0);
}
int rand_gen(int min, int max)
{
// uniform distribution
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(min, max); // generated by gen into an int in [a, b]
    return abs(distrib(gen));
// normal distribution
    
}
pair<int, int> find_min_max(vector<int> row)
{
    int min = INT_MAX, max = -1;
    for(auto i: row) {
        if(i > max) max = i;
        if(i < min) min = i;
    }
    return make_pair(min, max);
}

// pre-sanitiza
void mark_data(pair<int, int> data, map<int, set<Key>> &tree)
{
    // mark input chunk of LBA as invalid
    int L = ceil(log2(Maxlba+1) / log2(KPP)) + 1;   // total level num
    int MLI = L - 1;    // max level id

    for(int i = data.first; i <= data.second; i ++) {
        update_key_status(tree, make_pair(i, i), MLI, Status::invalid);
    }
}

// sanitize
void upward_update(int l, map<int, set<Key>> &tree)
{
    // recursively perform sanitize algorithm on level l
    if(l == 0) return;

// algorithm
    int L = ceil(log2(Maxlba+1) / log2(KPP)) + 1;   // total level num
    int MLI = L - 1;    // max level id
    int size_parent = pow(KPP, MLI - (l - 1));
    int num_parent = (Maxlba + 1) / size_parent;  // Maxlba/size = #key per level
    int size = pow(KPP, MLI - l); 
    bool modification = false;
    for(int i = 0; i < num_parent; i ++) {
        // check on level l
        vector<int> record(3, 0);   // record number for subkeys status
        // id of first subkey
        pair<int, int> data = make_pair(i * size_parent, i * size_parent + (size - 1));
        for(int j = 0; j < KPP; j ++) {
            // check KPP keys
            set<Key>::iterator it = tree[l].find(Key(data.first, data.second, l));
            if(it == tree[l].end()) {
                cout << "cant find in level " << l << endl;
                break;
            }
            record[it->st] ++;
            // data_begin += size;
            pair<int, int> data_next = make_pair(data.first + size, data.first + size + (size - 1));
            swap(data, data_next);
        }
        // if all N keys are invalid, mark parent as invalid
        // else N keys includes mixed status with valid key, mark parent as updated
        // (if no any updated, keep it as valid)
        data = make_pair(i * size_parent, i * size_parent + (size_parent - 1));
        if(record[Status::invalid] == KPP) {
            // update parent as invalid
            update_key_status(tree, data, l - 1, Status::invalid);
            modification = true;
        }
        else if(record[Status::valid] == KPP) {
            // keep parent as valid
        }
        else {
            // mark as updated
            update_key_status(tree, data, l - 1, Status::updated);
            modification = true;
        }
    }
    if(modification) upward_update(l - 1, tree);
    return;
}
int downward_update(map<int, set<Key>> &tree)
{
    // return #updated keys and alter updated keys -> valid keys
    int total_updated_keys = 0;
    for(auto i: tree) {
        for(auto key: i.second) {
            if(key.st == Status::updated) {
                total_updated_keys ++;
                update_key_status(tree, make_pair(key.begin, key.end), key.level, Status::valid);
            }
        }
    }
    return total_updated_keys;
}
void update_key_status(map<int, set<Key>> &tree, pair<int, int> data, int l, Status new_status)
{
    auto it = tree[l].find(Key(data.first, data.second, l));
    tree[l].erase(it);
    tree[l].insert(Key(data.first, data.second, l, new_status));
}

// ======================================================================
int main()
{
    // redirect output
    ofstream ofs;
    ofs.open("output.txt");

    // insert spec
    int exp, cmd_per_group;    // exponent of LBA num, LBA num = 2^exp
    cout << "input exp, KPP, cmd_per_group\n";
    cin >> exp >> KPP >> cmd_per_group;
    
    Maxlba = pow(2, exp) - 1;
    if(Maxlba < 3) { 
        cout << "too small\n"; 
        return 0; 
    }
    int L = ceil(log2(Maxlba+1) / log2(KPP)) + 1;   // total level num
    int MLI = L - 1;    // max level id    

// chart mode: make chart automatically
    // data size for each group: 2^i
    ofs << "size mean min max\n";
    for(int i = 0; i < exp; i ++) {
        int size = pow(2, i);
        map<int, int> record;   // (#updated keys, number of repitition)
        float sum = 0;
        // for this size, do several cmd
        for(int j = 0; j < cmd_per_group; j ++) {
            pair<int, int> data = cmd_gen(size);
            // cout << size << " " << data.first << " " << data.second << endl;

            map<int, set<Key>> tree;
            construct_tree(tree);
            mark_data(make_pair(data.first, data.second), tree);
            upward_update(MLI, tree);

            int key_num = downward_update(tree);
            record[key_num] ++;
            sum += key_num;
        }
        // calculate min, mean, max
        auto it_min = record.begin();
        int min = it_min->first;
        auto it_max = record.rbegin();
        int max = it_max->first;
        float mean = sum / cmd_per_group;

        ofs << "2^" << i << " " << mean << " " << min << " " << max << " | ";
        for(auto j: record) {
            for(int k = 0; k < j.second; k ++)
                ofs << j.first << " ";
        }
        ofs << endl;
    }

    // close output file
    ofs.close();
    return 0;
}
