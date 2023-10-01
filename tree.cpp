#include <iostream>
#include <set>
#include <cmath>
#include <random>
#include <algorithm>    // swap()
#include <vector>
#include <string>
#include <map>
#include <fstream>

#include "tree.h"

// ======================================================================
// =============                   Key                      =============
// ======================================================================
// output overload
ostream& operator << (ostream &out, const Key &i)
{
    std::cout << "(" << i.begin << "," << i.end << ")" << "- " << i.level << "|" << i.st << "|" << i.page_id;
    return out;
}

// ======================================================================
// =============                   Tree                      ============
// ======================================================================
// write ================================================================
int Tree::write_data(int size)
{
    if(write_pointer > Maxlba) {
        cout << "disk is full, no more write\n";
        return -1;
    }
    // at level [MLI], add n data
    set<int> page_collector;    // save updated key pages id
    for(int i = 0; i < size; i ++) {
        page_collector.insert(add_one_key(write_pointer, write_pointer, MLI, Status::valid));
        write_pointer ++;
        if(write_pointer > Maxlba)
            break;
    }
    // call key_manager to manage keys
    key_manager(MLI, page_collector);
    // return # updated key pages
    {
        // remove unwanted key page id
        set<int>::iterator it = page_collector.find(-1);
        page_collector.erase(*it);  // remove page -1 if it exists
    }
    
    return page_collector.size();
}
void Tree::write_full()
{
    cur_key_page_id = 0;
    cur_key_page_capacity = 0;
    // build tree
    for(int i = MLI; i >= 0; i --) {
        // size: #lba this key covered
        // offset: offset = j-i for k(i,j)
        int size = pow(KPP, MLI - i);
        int offset = size - 1;
        set<Key> tree_level;
        if(i == MLI) {
            for(int j = 0; j <= Maxlba; j += size)
                tree_level.insert(Key(j, j+offset, i, Status::valid, -1));
        }
        else {
            for(int j = 0; j <= Maxlba; j += size) {
                tree_level.insert(Key(j, j+offset, i, Status::valid, cur_key_page_id));
                cur_key_page_capacity ++;
                if(cur_key_page_capacity >= KPP) {
                    cur_key_page_capacity = 0;
                    cur_key_page_id ++;
                }
            }
        }
        tree[i] = tree_level;
    }
    // if cur capacity == 0, then modify to the last keypage
    if(cur_key_page_capacity == 0) {
        cur_key_page_id --;
        cur_key_page_capacity = KPP;
    }
    write_pointer = Maxlba;
}
int Tree::key_manager(int lv, set<int> &page_collector)
{
    if(lv == 0) {
        // page_collector.insert(add_one_key(0, Maxlba, 0, Status::valid));
        return 0;
    } 
    else if(write_pointer == 2) {
        // write cmd. device key special handler
        // if device key is not yet exist when data size==2, create a device key
        set<Key>::iterator it = tree[0].find(Key(0, Maxlba, 0));
        if(it == tree[0].end())
            page_collector.insert(add_one_key(0, Maxlba, 0, Status::valid));
        // proceed to further key adding logic
    }
    else if(tree[lv].size() / KPP < 1) return 0;

    // if KPP consecutive keys exist, create their parent key
    int size = pow(KPP, MLI - lv);
    int lba_begin = 0;
    int count = 0;
    int par_begin = 0, par_end;
    for(auto key: tree[lv]) {
        if(key.begin == lba_begin && (key.end - key.begin == (size - 1))) {
            count ++;
            lba_begin += size;
            if(count == KPP) {
                // create a parent key
                par_end = key.begin + size - 1;
                page_collector.insert(add_one_key(par_begin, par_end, lv - 1, Status::valid));
                // post adding
                par_begin = par_end + 1;
                count = 0;
            }
        }
        else break;
    }
    // check previous level
    key_manager(lv - 1, page_collector);
    return page_collector.size();
}

int Tree::add_one_key(int begin, int end, int lv, Status s)
{
    set<Key>::iterator it = tree[lv].find(Key(begin, end, lv));
    if(it != tree[lv].end()) {
        // if no add key, return key page -1
        return -1;
    }

    if(cur_key_page_capacity == KPP) {
        cur_key_page_capacity = 0;
        cur_key_page_id ++;
    }
    tree[lv].insert(Key(begin, end, lv, s, cur_key_page_id));
    cur_key_page_capacity ++;
    // if(cur_key_page_capacity >= KPP) {
    //     cur_key_page_capacity = 0;
    //     cur_key_page_id ++;
    // } assume KPP > 1
    return cur_key_page_id;
}

// sanitize =============================================================
void Tree::mark_data(pair<int, int> data)
{
    // mark input chunk of LBA as invalid
    for(int i = data.first; i <= data.second; i ++) {
        update_key_status(make_pair(i, i), MLI, Status::invalid);
    }
}
pair<int, int> Tree::sanitize(pair<int, int> data)
{
    // sanitize data and send report to ofs
    mark_data(data);
    upward_update(MLI);
    pair<int, int> keynum_pagenum = downward_update();
    return keynum_pagenum;
}
// sanitize subfunction
void Tree::upward_update(int lv)
{
    // recursively perform sanitize algorithm on level l
    if(lv == 0) {
        // update_key_status(tree, make_pair(0, Maxlba), 0, Status::updated);
        return;
    }
// algorithm
    int size_parent = pow(KPP, MLI - (lv - 1));
    int num_parent = (Maxlba + 1) / size_parent;  // Maxlba/size = #key per level
    int size = pow(KPP, MLI - lv); 
    bool modification = false;
    for(int i = 0; i < num_parent; i ++) {
        // check on level l
        vector<int> record(3, 0);   // record number for subkeys status
        // id of first subkey
        pair<int, int> data = make_pair(i * size_parent, i * size_parent + (size - 1));
        for(int j = 0; j < KPP; j ++) {
            // check KPP keys
            set<Key>::iterator it = tree[lv].find(Key(data.first, data.second, lv));
            if(it == tree[lv].end()) {
                cout << "cant find in level " << lv << endl;
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
        // update parent based on result
        if(record[Status::invalid] == KPP) {
            // update parent as invalid
            update_key_status(data, lv - 1, Status::invalid);
            modification = true;
        }
        else if(record[Status::valid] == KPP) {
            // keep parent as valid
        }
        else {
            // mark as updated
            update_key_status(data, lv - 1, Status::updated);
            modification = true;
        }
    }
    if(modification) upward_update(lv - 1);
    return;
}
pair<int, int> Tree::downward_update()
{
    // record #updated keys and #updated key pages. alter updated keys to valid keys
    int total_updated_keys = 0;
    set<int> page_collector;
    for(auto i: tree) {
        for(auto key: i.second) {
            if(key.st == Status::updated) {
                page_collector.insert(key.page_id);
                total_updated_keys ++;
                update_key_status(make_pair(key.begin, key.end), key.level, Status::valid);
            }
        }
    }
    // check each page id
    cout << "page_collector: ";
    for(auto i: page_collector)
        cout << i << " ";
    cout << endl;

    return make_pair(total_updated_keys, page_collector.size());
}
void Tree::update_key_status(pair<int, int> data, int lv, Status new_status)
{
    auto it = tree[lv].find(Key(data.first, data.second, lv));
    tree[lv].erase(it);
    tree[lv].insert(Key(data.first, data.second, lv, new_status, it->page_id));
}

// ======================================================================
// =============                Tree Req                     ============
// ======================================================================
int Tree_Req::add_request(int size)
{
    int id = Request_table.size();
    // if the size is too big to current disk, reduce write command size into available #LBA
    int read_add_size = (write_pointer <= Maxlba) ?size :(Maxlba - write_pointer + 1);
    // save request info; Request_table
    Request_table[id] = make_pair(write_pointer, read_add_size);
    return id;
}
int Tree_Req::write_data(int R_id)
{
    // given request, perform write based on RPK
    if(write_pointer > Maxlba) {
        cout << "disk is full, no more write\n";
        return -1;
    }
    // perform actual write, update write pointer
    // at level MLI, add a key named with R_id
    set<int> page_collector;    // save updated key pages id
    page_collector.insert(add_one_key(R_id, R_id, MLI, Status::valid));
    write_pointer += Request_table[R_id].second;  // move wp to next available LBA

    // save request info; LBA_2_Request
    /*
        for a request
        [first LBA, first LBA + size - 1] LBA are occupied by this request
    */
    for(int i = Request_table[R_id].first; i <= Request_table[R_id].first + Request_table[R_id].second - 1; i ++)
        LBA_2_Request[i] = R_id;

    // call key manager to create key if needed
    // todo ...

    // return # updated key pages
    {
        // remove unwanted key page id
        set<int>::iterator it = page_collector.find(-1);
        page_collector.erase(*it);  // remove page -1 if it exists
    }
    
    return page_collector.size();
}
int Tree_Req::key_manager(int lv, set<int> &page_collector)
{
    if(lv == 0) {

    }
    else if(lv == MLI) {
        // request key adding logic
        int R_id_begin = 0, R_id_end = 0, count = 0;     // starting R id, current count num
        for(auto key: tree[MLI]) {
            if(key.begin == R_id_begin) {
                // starts one new round
                count = 1;
            }
            else if(key.begin == R_id_end + 1) {
                count ++;
                R_id_end ++;
            }
            else break;     // non-consecutive key. shouldn't happen

            if(count == RPK) {
                // create one parent key
                // todo...

                // update for next round
                R_id_begin += RPK;
                count = 0;
            }
        }
    }
    else if(lv == MLI - 1) {

    }
    else {
        // key-key adding logic
    }
}


// ======================================================================
// =============                   Misc                      ============
// ======================================================================
void Tree::traverse()
{
    // print tree, top-bottom
    for(auto i: tree) {
        if(i.second.size() == 0)
            continue;
        cout << "level: " << i.first << endl;
        // skip last level
        // if(i.first == L - 1) {
        //     cout << "skip\n";
        //     break;
        // }
        // print each key
        for(auto j: i.second)
            cout << j << endl;
    }
    cout << "wp: " << write_pointer << "\n\n";
}
void Tree::print_member()
{
    cout << 
    write_pointer << " " << cur_key_page_id << " " << cur_key_page_capacity << " "
    << Maxlba << " " << KPP << " "
    << L << " " << MLI << endl;
}

pair<int, int> cmd_gen(Mode md, int size, int kpp, int maxlba)
{
    if(md == Mode::by_rand) {
        int a, b = -1;
        while(b == -1) {
            a = rand_gen(0, maxlba);   // gen a valid data id
            if( (a + size - 1) <= maxlba) {
                b = a + size - 1;
                return make_pair(a, b);
            }
        }
    }
    else {
        // generate a lba of the beginning of a key
        int a, b = -1;
        while(b == -1) {
            a = kpp * rand_gen(0, maxlba / kpp);
            if(a + size - 1 <= maxlba) {
                b = a + size - 1;
                return make_pair(a, b);
            }
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