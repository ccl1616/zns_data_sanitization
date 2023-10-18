#include <iostream>
#include <set>
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <fstream>

#include "tree.h"

using namespace std;

// ====================================================================================
// ====================================================================================
int main(int argc, char * argv[])
{
    //                 [0] [1] [2]   [3]   [4]   [5]
    // Key Mode ./main -k  -r  <exp> <KPP> <cmd> <out file name>
    // SNIA Mode ./main -s -r <exp> <KPP> <cmd> <out file name>
    //                     [0] [1] [2]   [3]   [4]   [5]   [6]
    // Request Mode ./main -r  -r  <exp> <KPP> <RPK> <cmd> <out file name>

    // check input
    if(argc < 7) {
        cout << "wrong input num\n";
        return 1;
    }
    // vec: <Exp mode> <Sanitize mode> <LBA exponent> <KPP> (<RPK>) <cmd>
    vector<string> vec;
    for(int i = 1; i < argc; i ++) {
        string s(argv[i]);
        vec.push_back(s);
    }

    // redirect output
    ofstream ofs;
    ofs.open(vec[vec.size() - 1]);
    // insert spec
    int exp, KPP, cmd_per_group, Maxlba, RPK;    // exponent of LBA num, LBA num = 2^exp
    Mode md;
    if(vec[0] == "-r") {
        // request exp
        // mode
        if(vec[1] == "-r") md = Mode::by_rand;
        else if(vec[1] == "-q") md = Mode::by_req;
        else if(vec[1] == "-p") md = Mode::by_partial_req;

        // -r -r <exp> <KPP> <RPK> <cmd>
        exp = stoi(vec[2]), KPP = stoi(vec[3]), RPK = stoi(vec[4]), cmd_per_group = stoi(vec[5]);
    }
    else {
        // md = (vec[1] == "-k") ?Mode::by_key :Mode::by_rand;
        // key exp, SNIA exp
        // mode: key, rand, stack
        if(vec[1] == "-k") md = Mode::by_key;
        else if(vec[1] == "-r") md = Mode::by_rand;
        else md = Mode::by_stack;

        exp = stoi(vec[2]), KPP = stoi(vec[3]), cmd_per_group = stoi(vec[4]);
    }
    // calculate system spec
    Maxlba = pow(2, exp) - 1;
    int L = ceil(exp / log2(KPP)) + 1;   // total level num
    if(Maxlba < 3) { 
        cout << "too small\n"; 
        return 0; 
    }
    // print spec to output file
    for(auto i: vec)
        ofs << i << " ";
    ofs << "\n\n";

// test code
    // ...

// Different Modes
    // Key Mode ./main -k -r/-k <exp> <KPP> <cmd> <outFile>
    if(vec[0] == "-k") {
        cout << "Key Mode\n";
        // pure key sanitization
        ofs << "size write_pn | k_mean k_min k_max | p_mean p_min p_max | #page_result\n";
        for(int i = 0; i < exp; i ++) {
            int size = pow(2, i);
            map<int, int> record_key_num;   // (#updated keys, number of repitition)
            map<int, int> record_page_num;   // (#R/W pages, number of repitition)

            float sum_key_num = 0, sum_page_num = 0;
            float write_sum_page_num = 0;   // for write
            
            for(int j = 0; j < cmd_per_group; j ++) {
                Tree zns(Maxlba, KPP, L);
                write_sum_page_num += zns.write_data(Maxlba + 1);     // write to full

                if(zns.tree[zns.MLI].size() != Maxlba + 1) {
                    cout << "Error:trying to sanitize a not full zone\n";
                    return 1;
                }
                pair<int, int> data = zns.cmd_gen(md, size);
                pair<int, int> keynum_pagenum = zns.sanitize(data);

                record_key_num[keynum_pagenum.first] ++;
                sum_key_num += keynum_pagenum.first;

                record_page_num[keynum_pagenum.second] ++;
                sum_page_num += keynum_pagenum.second;
            }
            // calculate updated key
            int min = record_key_num.begin()->first, max = record_key_num.rbegin()->first;
            float mean = sum_key_num / cmd_per_group;

            ofs << "2^" << i << " ";    // size
            ofs << write_sum_page_num / cmd_per_group;  //write_pn
            ofs << " | " << mean << " " << min << " " << max << " | ";
            // calculate R/W pages
            min = record_page_num.begin()->first, max = record_page_num.rbegin()->first;
            mean = sum_page_num / cmd_per_group;
            ofs << mean << " " << min << " " << max << " | ";

            // print out record_page_num
            for(auto k: record_page_num) {
                ofs << k.first << "*" << k.second << " ";
            }
            ofs << endl;
        }
    }   // end of Key Mode

    // SNIA Mode ./main -s -r/-k <exp> <KPP> <cmd> <outFile>
    else if(vec[0] == "-s") {
        cout << "SNIA Mode\n";
        // input file
        ifstream ifs;
        ifs.open("s17_01_all.txt");

        ofs << "size write_pn | k_mean k_min k_max | p_mean p_min p_max | #page_result\n";
        for(int i = 0; i < exp; i ++) {
            int size = pow(2, i);
            map<int, int> record_key_num;   // (#updated keys, number of repitition)
            map<int, int> record_page_num;   // (#R/W pages, number of repitition)
            // map<int, int> record_write_pn;  // write page num (# write pages, number of repitition)
            float sum_key_num = 0, sum_page_num = 0;    // for sanitize
            float write_sum_page_num = 0;   // for write

            for(int j = 0; j < cmd_per_group; j ++) {
                Tree zns(Maxlba, KPP, L);
                int counter = 0;
                // write to full
                while(zns.write_pointer <= Maxlba && !ifs.eof()) {
                    // get SNIA size
                    string s;
                    int w_size;
                    ifs >> s;
                    if(s != "") {
                        w_size = stoi(s);
                        // write
                        counter += zns.write_data(w_size);
                    }
                    // check ifs
                    if(ifs.eof()) {
                        ifs.clear();
                        ifs.seekg(0);   // use this to get back to beginning of file
                        ofs << "hit file end\n";
                    }
                }
                write_sum_page_num += counter;
                cout << "write cost: " << counter << endl;

                // sanitize
                if(zns.tree[zns.MLI].size() != Maxlba + 1) {
                    cout << "Error:trying to sanitize a not full zone\n";
                    return 1;
                }
                pair<int, int> data = zns.cmd_gen(md, size);
                pair<int, int> keynum_pagenum = zns.sanitize(data);

                record_key_num[keynum_pagenum.first] ++;
                sum_key_num += keynum_pagenum.first;

                record_page_num[keynum_pagenum.second] ++;
                sum_page_num += keynum_pagenum.second;
            }

            // output: "size write_pn | k_mean k_min k_max | p_mean p_min p_max | #page_result";
            // calculate updated key
            int min = record_key_num.begin()->first, max = record_key_num.rbegin()->first;
            float mean = sum_key_num / cmd_per_group;

            ofs << "2^" << i << " ";    // size
            ofs << write_sum_page_num / cmd_per_group;  //write_pn
            ofs << " | " << mean << " " << min << " " << max << " | ";  // k

            // calculate R/W pages
            min = record_page_num.begin()->first, max = record_page_num.rbegin()->first;
            mean = sum_page_num / cmd_per_group;
            ofs << mean << " " << min << " " << max << " | ";   // p

            // #page_result
            for(auto k: record_page_num) {
                ofs << k.first << "*" << k.second << " ";
            }
            ofs << endl;
        }
    }   // end of SNIA Mode

    // Request Mode ./main -r -q <exp> <KPP> <RPK> <cmd> <outFile>
    // *************** WARNING: segmentation fault possible
    else if(vec[0] == "-r") {
        cout << "request mode" << endl;
        if(argc < 8) {
            cout << "wrong input\n";
            return 0;
        }
        // input file
        ifstream ifs;
        ifs.open("s17_01_all.txt");

        map<int, int> num;  // (size, #times this size shows). need this value to calculate mean
        map<int, float> record_key_num;   // (size, sum #updated keys)
        map<int, float> record_page_num;   // (size, sum #R/W pages)

        for(int i = 0; i < cmd_per_group; i ++) {
            // write to full
            int counter = 0;    // count write KP
            Tree_Req zns(Maxlba, KPP, L, RPK);
            while(zns.write_pointer <= Maxlba && !ifs.eof()) {
                // get SNIA size
                string s;
                int w_size;
                ifs >> s;
                if(s != "") {
                    w_size = stoi(s);
                    // write
                    int R_id = zns.add_request(w_size);
                    if(R_id == -1) break;
                    counter += zns.write_data(R_id);
                }
                // check ifs
                if(ifs.eof()) {
                    ifs.clear();
                    ifs.seekg(0);   // use this to get back to beginning of file
                    ofs << "hit file end\n";
                }
            }
            zns.analyzer();
            
            // map<int, pair<int, int>> Record;
            for(auto e: zns.candidate_request) {
                if(e.first == 21) break;
                //choose a random one from this vector
                pair<int, int> data = e.second[rand_gen(0, e.second.size()-1)];     // md == by_req
                pair<int, int> keynum_pagenum = zns.sanitize(data, md);

                // cout << "2^" << e.first << " ";
                // cout << keynum_pagenum.first << ", " << keynum_pagenum.second << endl;
                // Record[e.first] = make_pair(keynum_pagenum.first, keynum_pagenum.second);
                record_key_num[e.first] += keynum_pagenum.first;
                record_page_num[e.first] += keynum_pagenum.second;
                num[e.first] ++;
            }
        }
        cout << "zns done\n";

        ofs << "size #key #page\n";
        for(auto k: num) {
            int size = k.first;
            int n = k.second;
            float mean_key = (record_key_num[size] == 0) ?0:(record_key_num[size] / n);
            float mean_page = (record_page_num[size] == 0) ?0:(record_page_num[size] / n);
            ofs << "2^" << size << " ";
            ofs << mean_key << " " << mean_page << endl;
        }
        
    }

    // SNIA write ./main -sw -r <exp> <KPP> <cmd> <outFile>
    // draw write key pages-data key pages bar chart
    else if(vec[0] == "-sw") {
        cout << "SNIA write\n";
        // input file
        ifstream ifs;
        ifs.open("s17_01_all.txt");
        // calculate avg zone stats
        float key_page_sum = 0, lba_page_sum = 0;
        int zns_num = 0;
        map<int, vector<double>> Record;    // id, <avg key pages, avg lba_pages>
        
        while(!ifs.eof()) {
            // write a zone to full
            Tree zns(Maxlba, KPP, L);
            while(zns.write_pointer <= Maxlba && !ifs.eof()) {
                // get SNIA size
                string s;
                int w_size;
                ifs >> s;
                if(s != "") {
                    w_size = stoi(s);
                    // write
                    zns.write_data(w_size);
                }
            }
            if(zns.write_pointer < Maxlba && ifs.eof()) break;  // last zone cannot be write to full
            
            // the zone is full now
            // record zone stats
            key_page_sum = zns.key_page_calculator();
            int lba_counter = zns.data_page_calculator();
            lba_page_sum += lba_counter;
            zns_num ++;
            cout << zns_num << ": " << key_page_sum << ", " << lba_counter << endl;

            // if meet group num, record
            if(zns_num >= cmd_per_group && zns_num % cmd_per_group == 0) {
                // record
                double key_page_mean = key_page_sum / cmd_per_group;
                double lba_page_mean = lba_page_sum / cmd_per_group;
                // Record[zns_num / cmd_per_group] = make_pair(key_page_mean, lba_page_mean);
                cout << key_page_mean << " " << lba_page_mean << endl;
                Record[zns_num / cmd_per_group].push_back(key_page_mean);
                Record[zns_num / cmd_per_group].push_back(lba_page_mean);
                // update counters
                key_page_sum = 0, lba_page_sum = 0;
            }
            if(zns_num >= 100) break;   // total zone num
        }
        // output
        ofs << "ID KP DP\n";
        for(auto i: Record) {
            ofs << i.first << " ";
            for(auto j: i.second)
                ofs << j << " ";
            ofs << endl;
        }
    }
    else cout << "wrong input mode" << endl;
    // close output file
    ofs.close();
    return 0;
}
