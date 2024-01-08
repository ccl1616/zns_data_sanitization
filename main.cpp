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
    //                 0   1   2     3     4     5
    // Key Mode ./main -k  -r <exp> <KPP> <cmd> <out file name>
    // SNIA Mode ./main -s -r <exp> <KPP> <cmd> <out file name>

    //                     0   1    2     3     4     5     6
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
    if(vec[0] == "-r" || vec[0] == "-wo") ofs.open(vec[vec.size() - 2]);
    else if(vec[0] == "-rsa") ofs.open(vec[vec.size() - 3]);
    else ofs.open(vec[vec.size() - 1]);
    // insert spec
    int exp, KPP, cmd_per_group, Maxlba, RPK;    // exponent of LBA num, LBA num = 2^exp
    Mode md;
    // mode
    if(vec[1] == "-k") md = Mode::by_key;
    else if(vec[1] == "-r") md = Mode::by_rand;
    else if(vec[1] == "-t") md = Mode::by_stack;
    else if(vec[1] == "-s") md = Mode::by_size;
    else {
        cout << "input mode wrong\n";
        return 0;
    }
    // exp variable
    if(vec[0] == "-r" || vec[0] == "-rf" || vec[0] == "-rsa" || vec[0] == "-wo") {
        // request exp
        // -r -r <exp> <KPP> <RPK> <cmd>
        exp = stoi(vec[2]), KPP = stoi(vec[3]), RPK = stoi(vec[4]), cmd_per_group = stoi(vec[5]);
    }
    else {
        // md = (vec[1] == "-k") ?Mode::by_key :Mode::by_rand;
        // key exp, SNIA exp
        // -s -r <exp> <KPP> <cmd>
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
    // Tree zns(Maxlba, KPP, L);
    // zns.write_data(Maxlba + 1);
    // int db, de;
    // while(1) {
    //     cin >> db >> de;
    //     pair<int, int> data = make_pair(db, de);
    //     pair<int, int> keynum_pagenum = zns.sanitize(data, true);
    //     cout << "(" << db << ", " << de << "): " << keynum_pagenum.first << " " << keynum_pagenum.second << endl;
    // }
    // return 0;


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

        if(md != Mode::by_stack) {
            // mode == by_key or by_rand
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
        }
        else {
            cout << "stack mode\n";
            // ./main -s -t <exp> <KPP> <cmd_per_group> <outFile>
            
            map<int, int> size_table;   // command size
            int denominator = 4;    // exponential of denominator
            for(int i = pow(2, denominator - 1); i < pow(2, denominator); i ++) {
                size_table[i] = pow(2, exp - denominator) * i;
            }
            
            // draw stack result
            // cmd_per_group variable not used. each bar is one zone
            // stack sanitize result in one zone and one bar
            map<int, vector<int>> record_NoD;     // NoD: Number of data
            map<int, vector<int>> record_key_num;   // sanitize cmd size, stack results
            map<int, vector<int>> record_page_num;  // sanitize cmd size, stack results
            for(auto i: size_table) {
                Tree zns(Maxlba, KPP, L);
                // write a zone to full
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
                    // check ifs
                    if(ifs.eof()) {
                        ifs.clear();
                        ifs.seekg(0);   // use this to get back to beginning of file
                        ofs << "hit file end\n";
                    }
                }
                // perform several sanitize commands on the same zone 
                for(int j = 0; j < cmd_per_group; j++) {
                    // sanitize
                    pair<int, int> data;
                    bool valid_cmd = false;
                    int counter = 0;
                    while(!valid_cmd && counter < 1000) {
                        data = zns.cmd_gen(Mode::by_rand, size_table[i.first]);
                        valid_cmd = zns.valid_cmd(data);
                        counter ++;
                    }
                    if(counter >= 1000 && !valid_cmd) {
                        // no valid cmd can be generated
                        cout << "no valid cmd can be generated: " << j << endl;
                        return 0;
                    }
                    pair<int, int> keynum_pagenum;
                    int NoD;
                    pair<int, pair<int, int>> temp = zns.sanitize_stacked(data);
                    NoD = temp.first; keynum_pagenum = temp.second;

                    record_NoD[i.first].push_back(NoD);
                    record_key_num[i.first].push_back(keynum_pagenum.first);
                    record_page_num[i.first].push_back(keynum_pagenum.second);
                    cout << "zone " << i.first << "-" << j << " done\n";
                }

                // print result for one zone
                ofs << "sanitize_command_size = " << size_table[i.first] << "(" << i.first << "/" << pow(2, denominator) << " zone )\n";
                ofs << "ID NoD #Key #Page" << endl;
                for(int j = 0; j < cmd_per_group; j ++) {
                    ofs << j << ": " << record_NoD[i.first][j] << " " << record_key_num[i.first][j] << " " << record_page_num[i.first][j] << endl;
                }
                ofs << endl;
            }
            // // output stack result
            // // #key result
            // ofs << "UPDATED_KEYS\n";
            // ofs << "sanitize_command_size #keys\n";
            // for(auto i: record_key_num) {
            //     ofs << size_table[i.first] << " " << i.first << " ";
            //     for(auto j: i.second)
            //         ofs << j << " ";
            //     ofs << endl;
            // }
            // ofs << "\n\n";

            // // #page result
            // ofs << "UPDATED_PAGES\n";
            // ofs << "sanitize_command_size #pages\n";
            // for(auto i: record_page_num) {
            //     ofs << size_table[i.first] << " " << i.first << " ";
            //     for(auto j: i.second)
            //         ofs << j << " ";
            //     ofs << endl;
            // }
            // ofs << "\n\n";
        }
    }   // end of SNIA Mode
    //                      0   1      2     3     4     5    6         7         (vec size = 8)
    // Request Mode ./main -r -r/-k <exp> <KPP> <RPK> <cmd> <outFile> <inputfile>
    else if(vec[0] == "-r") {
        cout << "request mode" << endl;
        if(argc < 9) {
            cout << "wrong input\n";
            return 0;
        }
        // input file
        ifstream ifs;
        ifs.open(vec[7]);
        // map for recording
        map<int, int> num;  // (size, #times this size shows). need this value to calculate mean
        map<int, float> record_key_num;   // (size, sum #updated keys)
        map<int, float> record_page_num;   // (size, sum #R/W pages)
        // array for assigned sanitization sizes
        vector<int> sani_size;  // a vector with entry number = 21
        for(int i = 0; i <= 20; i ++)
            sani_size.push_back(pow(2, i));
        // start
        for(int i = 0; i < cmd_per_group; i ++) {
            for(auto s: sani_size) {
                // create one zone write to full, then perform sani smc with size s

                // write to full
                Tree_Req zns(Maxlba, KPP, L, RPK);
                float sum = 0;
                while(zns.write_pointer <= Maxlba && !ifs.eof()) {
                    // get SNIA size
                    string s;
                    float w_size;
                    ifs >> s;
                    if(s != "") {
                        w_size = stoi(s) / pow(2, 12);    // transfer unit from Byte to LBA (1LBA = 4KB = 2^12Byte)
                        if(w_size >= 1) {
                            w_size = ceil(w_size);
                            sum += w_size;
                            // cout << w_size << " " << sum << endl;   // debug, print write process
                            // write
                            int R_id = zns.add_request(w_size);
                            if(R_id == -1) break;
                            zns.write_data(R_id);
                        }
                        
                    }
                    // check ifs
                    if(ifs.eof()) {
                        ifs.clear();
                        ifs.seekg(0);   // use this to get back to beginning of file
                        cout << "hit file end\n";
                    }

                }
                // create non-data keys
                // call key manager to create parent key if needed
                set<int> page_collector;
                zns.key_manager(zns.MLI, page_collector);
                cout << "\nzone " << i << " write to full capacity" << endl;

                // sani
                // one round of sanitization
                pair<int, int> data;
                data = zns.cmd_gen(md, s);    // request id
                // debug: print sani data chunk and verify
                cout << "target_size = " << s << "; data: " << data.first << ", " << data.second << endl;
                
                pair<int, int> keynum_pagenum = zns.sanitize(data, md);
                record_key_num[s] += keynum_pagenum.first;
                record_page_num[s] += keynum_pagenum.second;

                // debug
                cout << record_key_num[s] << " " << record_page_num[s] << endl;

                if(keynum_pagenum.second == 0) {
                    cout << "0 page error\n";
                    return 0;
                }
                num[s] ++;
                // end of sanitization

                cout << "zone " << i << " sanitize by " << s << endl;
            }
            
        }
        cout << "zns done\n";

        ofs << "size #key #page #zones\n";
        for(auto k: num) {
            int size = log(k.first) / log(2);
            int n = k.second;
            float mean_key = (record_key_num[k.first] == 0) ?0:(record_key_num[k.first] / n);
            float mean_page = (record_page_num[k.first] == 0) ?0:(record_page_num[k.first] / n);
            ofs << "2^" << size << " ";
            ofs << mean_key << " " << mean_page << " " << n << endl;
        }
    }
    
    //                            0   1      2     3     4     5    6                   7         (vec size = 8)
    // Request Fixed Size ./main -rf -r/-k <exp> <KPP> <RPK> <cmd> <Write Request size> <outFile>
    // write request size unit: Byte; usually input 4096, 4096 Byte = 4KB = 1 Page
    else if(vec[0] == "-rf") { 
        int fixed_req_size = stoi(vec[6]);  // get Write Request size
        cout << "request mode, fixed size request with size: " << fixed_req_size << " Byte" << endl;
        if(argc < 9) {
            cout << "wrong input\n";
            return 0;
        }
        // no need for input file because the write size is fixed
        // ifstream ifs;
        // ifs.open("s17_01_all.txt");
        
        map<int, int> num;  // (size, #times this size shows). need this value to calculate mean
        map<int, float> record_key_num;   // (size, sum #updated keys)
        map<int, float> record_page_num;   // (size, sum #R/W pages)        
        // array for assigned sanitization sizes
        vector<int> sani_size;  // a vector with sanitize command size;
        int LB_num = fixed_req_size / pow(2,12);
        for(int i = log(LB_num) / log(2) ; i <= 20; i ++)
            sani_size.push_back(pow(2, i));
        // start
        for(int i = 0; i < cmd_per_group; i ++) {
            for(auto s: sani_size) {
                // create one zone write to full, then perform sani smc with size s

                // write to full
                Tree_Req zns(Maxlba, KPP, L, RPK);
                while(zns.write_pointer <= Maxlba) {
                    int R_id = zns.add_request(fixed_req_size / pow(2,12));
                    if(R_id == -1) {
                        cout << "R_id == -1" << endl;
                        break;
                    }
                    zns.write_data(R_id);
                }
                // create non-data keys
                // call key manager to create parent key if needed
                set<int> page_collector;
                zns.key_manager(zns.MLI, page_collector);
                cout << "\nzone " << i << " write to full capacity" << endl;
                cout << "Request table size: " << zns.Request_table.size() << endl;
                // print tree info
                cout << "print tree info: " << endl;
                for(auto t: zns.tree) {
                    cout << "  " << t.first << " " << t.second.size() << endl;
                    if(t.first == 0) {
                        // print keys at lv0
                        for(auto k: t.second) {
                            cout << k.begin << "," << k.end << k.st << endl;
                        }
                    }
                }

                // sani
                // one round of sanitization
                pair<int, int> data;
                data = zns.cmd_gen(md, s);    // request id
                // debug: print sani data chunk and verify
                cout << "target_size = " << s << "; data: " << data.first << ", " << data.second << endl;
                
                pair<int, int> keynum_pagenum = zns.sanitize(data, md);
                record_key_num[s] += keynum_pagenum.first;
                record_page_num[s] += keynum_pagenum.second;

                // debug
                cout << keynum_pagenum.first << " " << keynum_pagenum.second << endl;

                if(keynum_pagenum.second == 0) {
                    cout << "0 page error\n";
                    for(auto level: zns.tree) {
                        cout << "level " << level.first << " has " << level.second.size() << "keys\n";
                    }
                    return 0;
                }
                num[s] ++;
                // end of sanitization

                cout << "zone " << i << " sanitize by " << s << endl;
            }
            
        }
        cout << "zns done\n";

        ofs << "size #key #page #zones\n";
        for(auto k: num) {
            int size = log(k.first) / log(2);
            int n = k.second;
            float mean_key = (record_key_num[k.first] == 0) ?0:(record_key_num[k.first] / n);
            float mean_page = (record_page_num[k.first] == 0) ?0:(record_page_num[k.first] / n);
            ofs << "2^" << size << " ";
            ofs << mean_key << " " << mean_page << " " << n << endl;
        }
        
    }
    
    //                                                0    1   2     3     4     5     6        7           8
    // Request and Sanitization Assigned Size ./main -rsa -s  <exp> <KPP> <RPK> <cmd> <outFile> <inputfile> <sani_mode_id>
    else if(vec[0] == "-rsa") {
        cout << "rsa mode" << endl;
        if(argc < 10) {
            cout << "wrong input flags\n";
            return 0;
        }
        // input file
        ifstream ifs;
        ifs.open(vec[7]);
        // map for recording
        map<int, int> num;  // (size, #times this size shows). need this value to calculate mean
        map<int, float> record_key_num;   // (size, sum #updated keys)
        map<int, float> record_page_num;   // (size, sum #R/W pages)
        map<int, float> record_req_num;    // (size, sum $sani request)

        // array for assigned sanitization sizes based on sani_mode_id
        int sani_mode_id = stoi(vec[8]);
        vector<int> sani_size;
        switch(sani_mode_id) {
            case 0:
                // 4KB; 1LB
                for(int i = 0; i < 10; i ++)
                    sani_size.push_back(1);
                break;
            case 1:
                // mid; 2-31 LB
                for(int i = 0; i < 10; i ++)
                    sani_size.push_back(2);
                break;
            case 2:
                // large; >=32LB
                for(int i = 0; i < 10; i ++)
                    sani_size.push_back(32);
                break;
            case 3:
                // mostly small; 80% 4KB + 10% mid + 10% large
                for(int i = 0; i < 8; i ++)
                    sani_size.push_back(1);
                sani_size.push_back(2);
                sani_size.push_back(32);
                break;
            case 4:
                // mostly large; 80% large + 10% mid + 10% 4KB
                for(int i = 0; i < 8; i ++)
                    sani_size.push_back(32);
                sani_size.push_back(2);
                sani_size.push_back(1);
                break;
            default:
                // 4KB; 1LB
                for(int i = 0; i < 10; i ++)
                    sani_size.push_back(1);
                break;
        }

        // start
        // cmd_per_group = 100, sani_size vec size = 10 -> total 1000 zones
        for(int i = 0; i < cmd_per_group; i ++) {
            for(auto s: sani_size) {
                // create one zone write to full, then perform sani smc with size s

                // for RPK=1, manually set to RPK=KPP to make it work
                if(RPK == 1) RPK = KPP;
                // write to full
                Tree_Req zns(Maxlba, KPP, L, RPK);

                float sum = 0;
                while(zns.write_pointer <= Maxlba && !ifs.eof()) {
                    // get SNIA size
                    string s;
                    float w_size;
                    ifs >> s;
                    if(s != "") {
                        w_size = stoi(s) / pow(2, 12);    // transfer unit from Byte to LBA (1LBA = 4KB = 2^12Byte)
                        if(w_size >= 1) {
                            w_size = ceil(w_size);
                            sum += w_size;
                            // cout << w_size << " " << sum << endl;   // debug, print write process
                            // write
                            int R_id = zns.add_request(w_size);
                            if(R_id == -1) break;
                            zns.write_data(R_id);
                        }
                        
                    }
                    // check ifs
                    if(ifs.eof()) {
                        ifs.clear();
                        ifs.seekg(0);   // use this to get back to beginning of file
                        cout << "hit file end\n";
                    }

                }
                // create non-data keys
                // call key manager to create parent key if needed
                set<int> page_collector;
                zns.key_manager(zns.MLI, page_collector);
                cout << "\nzone " << i << " write to full capacity" << endl;
                // print tree info
                cout << "print tree info: " << endl;
                for(auto t: zns.tree) {
                    cout << "  " << t.first << " " << t.second.size() << endl;
                    if(t.first == 0) {
                        // print keys at lv0
                        for(auto k: t.second) {
                            cout << k.begin << "," << k.end << " " << k.st << endl;
                        }
                    }
                }
                cout << "Request num: " << zns.Request_table.size() << endl;
                // sani
                // before sani, check if there's request size == 1LB

                // one round of sanitization
                pair<int, int> data;
                data = zns.cmd_gen(md, s);    // request id
                // debug: print sani data chunk and verify
                cout << "target_size = " << s << "; data: " << data.first << ", " << data.second << endl;
                
                pair<int, int> keynum_pagenum = zns.sanitize(data, md);
                record_key_num[s] += keynum_pagenum.first;
                record_page_num[s] += keynum_pagenum.second;
                record_req_num[s] += (data.second - data.first + 1);

                // debug
                cout << keynum_pagenum.first << " " << keynum_pagenum.second << " " << (data.second - data.first + 1) << endl;

                if(keynum_pagenum.second == 0) {
                    cout << "0 page error\n";
                    return 0;
                }
                num[s] ++;
                // end of sanitization

                cout << "zone " << i << " sanitize by " << s << endl;
            }
            
        }
        cout << "zns done\n";

        ofs << "size #key #page #req #zones\n";
        for(auto k: num) {
            int size = log(k.first) / log(2);
            int n = k.second;
            float mean_key = (record_key_num[k.first] == 0) ?0:(record_key_num[k.first] / n);
            float mean_page = (record_page_num[k.first] == 0) ?0:(record_page_num[k.first] / n);
            float mean_req = (record_req_num[k.first] == 0) ?0:(record_req_num[k.first] / n);
            ofs << "2^" << size << " ";
            ofs << mean_key << " " << mean_page << " " << mean_req << " " << n << endl;
        }
    }
    
    //                    0   1   2     3     4     5      6         7           
    // write only ./main -wo -r <exp> <KPP> <RPK> <cmd> <outFile> <inputfile>
    // write only and record the runtime
    else if(vec[0] == "-wo") {
        cout << "rsa mode" << endl;
        if(argc < 9) {
            cout << "wrong input flags\n";
            return 0;
        }
        // input file
        ifstream ifs;
        ifs.open(vec[7]);
        
        for(int i = 0; i < cmd_per_group; i ++) {
            // create one zone write to full, then perform sani smc with size s

            // for RPK=1, manually set to RPK=KPP to make it work
            if(RPK == 1) RPK = KPP;
            // write to full
            Tree_Req zns(Maxlba, KPP, L, RPK);

            float sum = 0;
            while(zns.write_pointer <= Maxlba && !ifs.eof()) {
                // get SNIA size
                string s;
                float w_size;
                ifs >> s;
                if(s != "") {
                    w_size = stoi(s) / pow(2, 12);    // transfer unit from Byte to LBA (1LBA = 4KB = 2^12Byte)
                    if(w_size >= 1) {
                        w_size = ceil(w_size);
                        sum += w_size;
                        // cout << w_size << " " << sum << endl;   // debug, print write process
                        // write
                        int R_id = zns.add_request(w_size);
                        if(R_id == -1) break;
                        zns.write_data(R_id);
                    }
                    
                }
                // check ifs
                if(ifs.eof()) {
                    ifs.clear();
                    ifs.seekg(0);   // use this to get back to beginning of file
                    cout << "hit file end\n";
                }

            }
            // create non-data keys
            // call key manager to create parent key if needed
            set<int> page_collector;
            zns.key_manager(zns.MLI, page_collector);
            cout << "\nzone " << i << " write to full capacity" << endl;
            // print tree info
            cout << "print tree info: " << endl;
            for(auto t: zns.tree) {
                cout << "  " << t.first << " " << t.second.size() << endl;
                if(t.first == 0) {
                    // print keys at lv0
                    for(auto k: t.second) {
                        cout << k.begin << "," << k.end << " " << k.st << endl;
                    }
                }
            }
            cout << "Request num: " << zns.Request_table.size() << endl;
            cout << "zone " << i << endl;
        }
        cout << "zns done\n";
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
