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
    out << "(" << i.begin << "," << i.end << ")" << "- " << i.level << "|" << i.st << "|" << i.page_id;
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
        // if device key not exists, create device key
        set<Key>::iterator it = tree[0].find(Key(0, Maxlba, 0));
        if(it == tree[0].end())
            page_collector.insert(add_one_key(0, Maxlba, 0, Status::valid));
        return 0;
    } 
    else if(write_pointer >= 2) {
        // write cmd. device key special handler
        // if device key is not yet exist when data size==2, create a device key
        set<Key>::iterator it = tree[0].find(Key(0, Maxlba, 0));
        if(it == tree[0].end())
            page_collector.insert(add_one_key(0, Maxlba, 0, Status::valid));
        // proceed to further key adding logic
    }
    else if(tree[lv].size() / KPP < 1) return 0;

    // if KPP consecutive keys exist, create their parent key
    int size = pow(KPP, MLI - lv);  // current lv size
    int lba_begin = 0;
    int count = 0;
    int par_begin = 0, par_end;
    for(auto key: tree[lv]) {
        if(key.begin == lba_begin && (key.end - key.begin == (size - 1))) {
            count ++;
            lba_begin += size;  // move lba_begin to next candidate
        }
        else break;     // non-consecutive

        if(count == KPP) {
            // create a parent key
            par_end = key.begin + size - 1;
            page_collector.insert(add_one_key(par_begin, par_end, lv - 1, Status::valid));
            // post adding
            par_begin = par_end + 1;
            count = 0;
        }
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
    pair<int, int> keynum_pagenum = downward_update(false);
    return keynum_pagenum;
}
// sanitize subfunction
void Tree::upward_update(int lv)
{
    // recursively perform sanitize algorithm on level l
    if(lv == 0) {
        // Tree::update_key_status(make_pair(0, Maxlba), 0, Status::updated);
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
                cout << "cant find in level " << lv  << ": " << data.first << " " << data.second << endl;
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
pair<int, int> Tree::downward_update(bool alter_invalid)
{
    // record #updated keys and #updated key pages. alter updated keys to valid keys
    // if alter_invalid id true, then also alter invalid to valid
    int total_updated_keys = 0;
    set<int> page_collector;
    for(auto i: tree) {
        for(auto key: i.second) {
            if(key.st == Status::updated) {
                page_collector.insert(key.page_id);
                total_updated_keys ++;
                update_key_status(make_pair(key.begin, key.end), key.level, Status::valid);
            }
            else if(key.st == Status::updated && alter_invalid) {
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
bool Tree::update_key_status(pair<int, int> data, int lv, Status new_status)
{
    auto it = tree[lv].find(Key(data.first, data.second, lv));
    if(it == tree[lv].end()) return false;
    tree[lv].erase(it);
    tree[lv].insert(Key(data.first, data.second, lv, new_status, it->page_id));
    return true;
}
pair<int, int> Tree::cmd_gen(Mode md, int size)
{
    if(md == Mode::by_rand) {
        int a, b = -1;
        while(b == -1) {
            a = rand_gen(0, Maxlba);   // gen a valid data id
            if( (a + size - 1) <= Maxlba) {
                b = a + size - 1;
                return make_pair(a, b);
            }
        }
    }
    else {
        // generate a lba of the beginning of a key
        int a, b = -1;
        while(b == -1) {
            a = KPP * rand_gen(0, Maxlba / KPP);
            if(a + size - 1 <= Maxlba) {
                b = a + size - 1;
                return make_pair(a, b);
            }
        }
    }
    cout << "rand_gen fail\n";
    return make_pair(0, 0);
}
int Tree::data_page_calculator() 
{
    set<int> s;
    for(auto i: tree[MLI]) {
        s.insert(i.page_id);
    }
    return s.size();
}
int Tree::key_page_calculator() 
{
    set<int> s;
    for(auto l: tree) {
        if(l.first == MLI) break;
        for(auto k: l.second)
            s.insert(k.page_id);
    }
    return s.size();
}

// ======================================================================
// ======================================================================
// ======================================================================
// ======================================================================
// ======================================================================
// =============                Tree Req                     ============
// ======================================================================
// ======================================================================
// ======================================================================
// ======================================================================



int Tree_Req::add_request(int size)
{
    if(write_pointer > Maxlba) return -1;   // no more adding is allowed
    int id = Request_table.size();
    // if the size is too big to current disk, reduce write command size into available #LBA
    int read_add_size = (write_pointer + size - 1 <= Maxlba) ?size :(Maxlba - write_pointer + 1);
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

    // call key manager to create parent key if needed
    key_manager(MLI, page_collector);

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
        // if device key not exists, create device key
        set<Key>::iterator it = tree[0].find(Key(0, Maxlba, 0));
        if(it == tree[0].end())
            page_collector.insert(add_one_key(0, Maxlba, 0, Status::valid));
        return 0;
    }
    else if(tree[lv].size() / KPP < 1) return 0;
    else if(lv == MLI) {
        // request-key adding logic
        int R_begin = 0, R_end = 0, count = 0;     // starting R id, current count num
        bool debug = false;
        for(auto key: tree[MLI]) {
            if(key.begin == R_begin) {
                // starts one new round
                count = 1;
                if(debug) cout << key.begin << endl;
            }
            else if(key.begin == R_end + 1) {
                count ++;
                R_end ++;
                if(debug) cout << key.begin << endl;
            }
            else break;     // non-consecutive key. shouldn't happen

            if(count == RPK) {
                if(debug) cout << R_begin << ", " << R_end << " ";
                // create one parent key
                page_collector.insert(add_one_key(R_begin, R_end, lv - 1, Status::valid));
                // update for next round
                R_begin += RPK, R_end = R_begin;
                count = 0;

                if(debug) cout << R_begin << " " << R_end << endl;
            }
        }
    }
    else{
        // key-key adding logic
        // if KPP consecutive keys exist, create their parent key
        int size = size_table[lv];  // current lv size
        int lba_begin = 0;
        int count = 0;
        int par_begin = 0, par_end;
        for(auto key: tree[lv]) {
            if(key.begin == lba_begin && (key.end - key.begin == (size - 1))) {
                count ++;
                lba_begin += size;  // move lba_begin to next candidate
            }
            else break;     // non-consecutive

            if(count == KPP) {
                // create a parent key
                par_end = key.begin + size - 1;
                page_collector.insert(add_one_key(par_begin, par_end, lv - 1, Status::valid));
                // post adding
                par_begin = par_end + 1;
                count = 0;
            }
        }
    }
    // MLI-1 level key num >= 2, then create device key
    if(tree[MLI - 1].size() >= 2) {
        set<Key>::iterator it = tree[0].find(Key(0, Maxlba, 0));
        if(it == tree[0].end())
            page_collector.insert(add_one_key(0, Maxlba, 0, Status::valid));
    }
    // check previous level
    key_manager(lv - 1, page_collector);
    return page_collector.size();

}
void Tree_Req::create_size_table()
{
    size_table[MLI] = 1;
    size_table[MLI - 1] = RPK;
    for(int i = MLI - 2; i >= 0; i --) {
        size_table[i] = size_table[i + 1] * KPP;
    }
}
pair<int, int> Tree_Req::cmd_gen(Mode md, int size)
{
    if(md == Mode::by_rand) {
        // rand LBA
        int a, b = -1;
        while(b == -1) {
            a = rand_gen(0, Maxlba);   // gen a valid data id
            if( (a + size - 1) <= Maxlba) {
                b = a + size - 1;
                return make_pair(a, b);     // return LBA id
            }
        }
    }
    // else if(md == Mode::by_req){
    //     // gen cmd based on request
    //     // rand a Request id
    //     // add up size till targeted size
    //     // if failed, pick another Request id
    //     int req_begin = -1, req_end, cur_size = 0;
    //     // TEMP ------------------------------------------------------------------- UNUSE
    //     while(req_begin == -1) {
    //         // an iteration to gen cmd
    //         req_begin = rand_gen(0, Request_table.size()-1); // pick a request as starting point 
    //         req_end = req_begin;
    //         cur_size = Request_table[req_begin].second;  
    //         int checker = range_checker(cur_size, size);    // 0 = smaller than size; 1 = perfect fit; 2 = oversize         
    //         if(checker == 1) {
    //             // find a perfect request, return result
    //             return make_pair(req_begin, req_begin);
    //         }
    //         else if(checker == 2) {
    //             // size too big, change another request to begin
    //             req_begin = -1;
    //             continue;
    //         }
    //         else {
    //             // add up
    //             while( cur_size < size && req_end < (Request_table.size()-1) ) {
    //                 req_end ++;
    //                 if(req_end >= Request_table.size()) {
    //                     req_begin = -1;
    //                     break;  // invalid req id
    //                 }
    //                 else cur_size += Request_table[req_end].second;

    //                 // check if end
    //                 int checker = range_checker(cur_size, size);
    //                 if(checker == 1) return make_pair(req_begin, req_end);  // current chunk is perfect
    //                 else if(checker == 2) {
    //                     // too big. pick another start Request id
    //                     req_begin = -1;
    //                     break;
    //                 }
    //                 // else keep add up
    //             }
    //         }
    //     }

    //     // return Request id
    // }
    return make_pair(0,0);
}
pair<int, int> Tree_Req::sanitize(pair<int, int> data, Mode md)
{
    pair<int, int> target;
    if(md == Mode::by_rand) {
        // perform key conversion
        // lba -> actual key id
        target = make_pair(LBA_2_Request[data.first], LBA_2_Request[data.second]);
    }
    else {
        // data is Request id, which is the key id
        target = data;
    }

    // call original sanitize
    // sanitize data and send report to ofs
    mark_data(target);
    upward_update(MLI);
    pair<int, int> keynum_pagenum = downward_update(md == Mode::by_req);
    return keynum_pagenum;
}
void Tree_Req::upward_update(int lv)
{
    if(lv == 0) {
        Tree::update_key_status(make_pair(0, Maxlba), 0, Status::updated);
        return;
    }
// algorithm. bottom-up
    else {
        int chunk_size = (lv == MLI) ?RPK : KPP;
        int quo = tree[MLI].size() / chunk_size;
        int rem = tree[MLI].size() % chunk_size;
        bool modification = false;
        // check each unit of keys and decide their parent status
        auto it = tree[lv].begin();
        // check chunks
        for(int i = 0; i < quo; i ++) {
            vector<int> record(3, 0);   // record number for key status
            for(int j = 0; j < chunk_size; j ++) {
                record[it->st] ++;
                it ++;
            }
            
            // mark parent status based on result
            if(record[Status::invalid] == chunk_size) {
                pair<int, int> key = make_pair(i * size_table[lv - 1], (i+1) * size_table[lv - 1] - 1);
                if(update_key_status(key, lv - 1, Status::invalid) == true)
                    modification = true;
            }
            else if(record[Status::valid] != chunk_size) {
                // should update
                pair<int, int> key = make_pair(i * size_table[lv - 1], (i+1) * size_table[lv - 1] - 1);
                if( update_key_status(key, lv - 1, Status::updated) == true)
                    modification = true;
            }
        }
        // ignore remaining keys. these keys dont have direct parent. just set device key as updated
        if(rem != 0)
            Tree::update_key_status(make_pair(0, Maxlba), 0, Status::updated);

        if(modification) upward_update(lv - 1);
        return;
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
        if(i.first == L - 1) {
            cout << "skip\n";
            break;
        }
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
int rand_gen(int min, int max)
{
// uniform distribution
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(min, max); // generated by gen into an int in [a, b]
    return abs(distrib(gen));
// normal distribution
    
}
int range_checker(int x, int target)
{
    // check if log2(target)-1 < log2(x) < log2(target)+1
    // based on relationship, return 0 | 1 | 2
    float low = (log(target) / log(2)) - 1;
    float high = (log(target) / log(2)) + 1;
    float log_x = log(x) / log(2);

    if( log_x <= low ) return 0;
    else if(log_x >= high) return 2;
    else return 1;
}

// testing
void Tree_Req::analyzer()
{
    int n = Request_table.size() - 1;
    for(int i = 0; i <= n; i ++) {
        for(int j = i; j <= n; j ++) {
            int sum = acculumator(i, j);
            candidate_request[ log(sum)/log(2) ].push_back(make_pair(i, j));
        }
    }
}
int Tree_Req::acculumator(int start, int end)
{
    int sum = 0;
    if(start == end) sum = Request_table[start].second;
    else {
        for(int i = start; i <= end; i ++)
            sum += Request_table[i].second;
    }
    return sum;
}