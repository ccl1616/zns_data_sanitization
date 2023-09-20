#include <iostream>
#include <map>
#include <set>
#include <fstream>
using namespace std;

// ======================================================================
// =============                   Key                      =============
// ======================================================================
enum Status {valid, updated, invalid};
enum Mode {by_rand, by_key};
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
    pair<int, int> sanitize(pair<int, int> data);    // given target data, return updated keynum, pagenum
// sanitize subfunction
    void upward_update(int lv); // given current level, perform upward checks
    pair<int, int> downward_update();     // downward remove updated tags and return (#updated keys,#R/W key pages)
    void update_key_status(pair<int, int> data, int lv, Status new_status);     // update(rm and add) key in this tree

// misc
    void traverse();    // traverse this tree and print out keys
    void print_member();
};

// ======================================================================
// =============                   Misc                      ============
// ======================================================================
pair<int, int> cmd_gen(Mode md, int size, int kpp, int maxlba);  // given size, return valid data id by the chunk size
int rand_gen(int min, int max); // pure rand_gen of range [min, max]