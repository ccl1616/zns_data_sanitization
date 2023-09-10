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

// output overload
ostream& operator << (ostream &out, const Key &i)
{
    std::cout << "(" << i.begin << "," << i.end << ")" << "- " << i.level << "|" << i.st << "|" << i.page_id;
    return out;
}

// write =================================================
void Tree::write_data(int size)
{}
void Tree::write_full()
{
    cur_key_page_id = 0;
    cur_key_page_capacity = 0;
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
                tree_level.insert(Key(j, j+offset, i, Status::valid, cur_key_page_id));
                cur_key_page_capacity ++;
                if(cur_key_page_capacity >= KPP) {
                    cur_key_page_id ++;
                    cur_key_page_capacity = 0;
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
}
// write append by input data size
// if size == fullsize, then this is a write all command.
void Tree::key_manager(){}
// sub function for write_data. create parent keys if needed.

// sanitize =================================================
void Tree::mark_data(pair<int, int> data){}
void Tree::sanitize(ofstream ofs){}
// sanitize subfunction
void Tree::upward_update(int lv){}
// pair<int, int> Tree::downward_update(){}
void Tree::update_key_status(pair<int, int> data, int lv, Status new_status){}

// misc =================================================
void Tree::traverse()
{
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
