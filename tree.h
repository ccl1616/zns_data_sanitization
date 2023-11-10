#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <fstream>
using namespace std;

// ======================================================================
// =============                   Key                      =============
// ======================================================================
enum Status {valid, updated, invalid};
enum Mode {by_rand, by_key, by_req, by_partial_req, by_stack};
//         -r       -k      -q      (-pq)           -t
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

// ======================================================================
// =============                   Tree                      ============
// ======================================================================
class Tree
{
public:
    map<int, set<Key>> tree;
    int write_pointer;  // the LBA that can do the next append write;;; [MLI].size == LBA num
    int cur_key_page_id, cur_key_page_capacity;
    int Maxlba, KPP;
    int L, MLI;
    Tree(int max_lba, int kpp, int l) :
        write_pointer(0), cur_key_page_id(0), cur_key_page_capacity(0), 
        Maxlba(max_lba), KPP(kpp), L(l), MLI(l - 1) {}
    Tree() : 
        write_pointer(0), cur_key_page_id(0), cur_key_page_capacity(0), Maxlba(0), KPP(0), L(0), MLI(0) {}
// write
    int write_data(int size);  // write append by input data size. return # updated key pages
    void write_full(); // if size == fullsize, write_data() == write_full()
    int key_manager(int lv, set<int> &page_collector); // sub function for write_data. create parent keys if needed. return # updated key pages
    int add_one_key(int begin, int end, int lv, Status s);     // call this function to add a new key and update key_page info, return key page_id

// sanitize
    void mark_data(pair<int, int> data); // mark data chunk as invalid
    set<Key> mark_data_stack(pair<int, int> data);  // return data that are still valid in the input
    pair<int, int> sanitize(pair<int, int> data);    // given target data, return updated keynum, pagenum
    pair<int, pair<int, int>> sanitize_stacked(pair<int, int> data);    // sanitize function especially for stacked type of experiment
    // sanitize subfunction
    void upward_update(int lv); // given current level, perform upward checks
    void upward_update_targeted(set<Key> target, set<Key> &updated);    // update targeted key's parents only
    pair<int, int> downward_update(bool all_to_valid);     // downward remove updated tags and return (#updated keys,#R/W key pages)
    bool update_key_status(pair<int, int> data, int lv, Status new_status);     // update(rm and add) key in this tree

// misc
    void traverse();    // traverse this tree and print out keys
    void print_member();
    pair<int, int> cmd_gen(Mode md, int size);
    int data_page_calculator();   // return # pages that the data occupied
    int key_page_calculator();
    int key_page_calculator_given_set(set<Key> S);    // given set S, count #key pages in S
    Key return_parent_key_info(Key k);  // calculate parent of key k
    bool valid_cmd(pair<int, int> data);    // for Stack experiment; check if the sanitize cmd is valid by checking its data status
};

// ======================================================================
// =============                Tree Req                     ============
// ======================================================================
class Tree_Req : public Tree
{
public:
    int RPK;    // request per key
    // info tables
    map<int, pair<int, int>> Request_table;   // Request Id: (first LBA, Request size)
    map<int, int> LBA_2_Request;    // LBA id - Request id
    map<int, int> size_table;   // level id - size of one key in this level
    map<int, vector<pair<int, int>>> candidate_request;     // candidate chunk of requests for sanitize; log(size)-(R id start, R id end)

    Tree_Req(int max_lba, int kpp, int l, int rpk) : 
        Tree(max_lba, kpp, l),
        RPK(rpk) 
        {
            create_size_table();
        }
// add a request
    int add_request(int size);  // given request (write) size, add it to table, return request id
// write
    int write_data(int R_id);   // write data by table
    int key_manager(int lv, set<int> &page_collector);
    void create_size_table();   // create size table by definition of tree; key manager need this info
// sanitize
    pair<int, int> cmd_gen(Mode md, int size);    // based on md, return a chunk of key id that need to be updated into invalid
    pair<int, int> sanitize(pair<int, int> data, Mode md);
    void upward_update(int lv);
// misc
    void analyzer();
    int acculumator(int start, int end);    // accumulcate data size from Req[start] to Req[end]
    Key return_parent_key_info(Key k);
};

// ======================================================================
// =============                   Misc                      ============
// ======================================================================
int rand_gen(int min, int max); // pure rand_gen of range [min, max]
int range_checker(int x, int target);  // check if log2(target)-1 < log2(x) < log2(target)+1