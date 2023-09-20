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
    // key mode ./main -k -r <exp> <KPP> <cmd>
    // snia mode ./main -s -r <exp> <KPP> <cmd>
    // vec: -k -r <exp> <KPP> <cmd>
    if(argc != 6) {
        cout << "wrong input num\n";
        return 1;
    }
    vector<string> vec;
    for(int i = 1; i < argc; i ++) {
        string s(argv[i]);
        vec.push_back(s);
    }

    // redirect output
    ofstream ofs;
    ofs.open("output.txt");

    // insert spec
    int exp, KPP, cmd_per_group, Maxlba;    // exponent of LBA num, LBA num = 2^exp
    exp = stoi(vec[2]), KPP = stoi(vec[3]), cmd_per_group = stoi(vec[4]);
    Mode md = (vec[1] == "-k") ?Mode::by_key :Mode::by_rand;

    // calculation
    Maxlba = pow(2, exp) - 1;
    int L = ceil(exp / log2(KPP)) + 1;   // total level num
    if(Maxlba < 3) { 
        cout << "too small\n"; 
        return 0; 
    }
// test code
    
// Different Modes
    // Key Mode
    if(vec[0] == "-k") {
        // pure key sanitization
        ofs << "size k_mean k_min k_max | p_mean p_min p_max | #page_result\n";
        for(int i = 0; i < exp; i ++) {
            int size = pow(2, i);
            map<int, int> record_key_num;   // (#updated keys, number of repitition)
            map<int, int> record_page_num;   // (#R/W pages, number of repitition)

            float sum_key_num = 0;
            float sum_page_num = 0;
            // for this size, do several cmd
            for(int j = 0; j < cmd_per_group; j ++) {
                Tree zns(Maxlba, KPP, L);
                zns.write_data(Maxlba + 1);     // write to full

                if(zns.tree[zns.MLI].size() != Maxlba + 1) {
                    cout << "Error:trying to sanitize a not full zone\n";
                    return 1;
                }
                pair<int, int> data = cmd_gen(md, size, KPP, Maxlba);
                pair<int, int> keynum_pagenum = zns.sanitize(data);

                record_key_num[keynum_pagenum.first] ++;
                sum_key_num += keynum_pagenum.first;

                record_page_num[keynum_pagenum.second] ++;
                sum_page_num += keynum_pagenum.second;
            }
            // calculate updated key
            int min = record_key_num.begin()->first, max = record_key_num.rbegin()->first;
            float mean = sum_key_num / cmd_per_group;
            ofs << "2^" << i << " " << mean << " " << min << " " << max << " | ";
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
    }
    // SNIA Mode ./main -s -r <exp> <KPP> <cmd>
    else if(vec[0] == "-s") {
        cout << "snia\n";
        // input file
        ifstream ifs;
        ifs.open("s17_01_all.txt");
        ofs << "W_KPN\n";

        for(int i = 0; i < 3; i ++) {
            Tree zns(Maxlba, KPP, L);
            int write_page_num = 0;
            // SNIA write to full
            while(zns.write_pointer <= Maxlba) {
                // check ifs
                if(ifs.eof()) {
                    ifs.clear();
                    ifs.seekg(0);   // use this to get back to beginning of file
                    cout << "hit file end\n";
                }
                // get SNIA size
                string s;
                int size;
                ifs >> s;
                if(s == "") continue;
                size = stoi(s);
                // write
                write_page_num += zns.write_data(size);
            }
            ofs << "wp: " << zns.write_pointer << " pn: " << write_page_num << endl;
        }
        
        ofs << endl;
    }
    // print spec
    for(auto i: vec)
        ofs << i << " ";
    ofs << endl;
    // close output file
    ofs.close();
    return 0;
}
