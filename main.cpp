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
int main()
{
    // redirect output
    ofstream ofs;
    ofs.open("output.txt");

    // insert spec
    int exp, cmd_per_group;    // exponent of LBA num, LBA num = 2^exp
    int Maxlba, KPP;
    cout << "input exp, KPP, cmd_per_group\n";
    cin >> exp >> KPP >> cmd_per_group;
    
    Maxlba = pow(2, exp) - 1;
    int L = ceil(exp / log2(KPP)) + 1;   // total level num
    if(Maxlba < 3) { 
        cout << "too small\n"; 
        return 0; 
    }
// test
    Tree zns(Maxlba, KPP, L);
    zns.traverse();
    zns.write_data(17);
    cout << "after write\n";
    zns.traverse();

    zns.write_data(10);
    cout << "\nafter write\n";
    zns.traverse();
    return 0;

// chart mode: make chart automatically
    // data size for each group: 2^i
    ofs << "size k_mean k_min k_max | p_mean p_min p_max | #page_result\n";
    for(int i = 0; i < exp; i ++) {
        int size = pow(2, i);
        map<int, int> record_key_num;   // (#updated keys, number of repitition)
        map<int, int> record_page_num;   // (#R/W pages, number of repitition)

        float sum_key_num = 0;
        float sum_page_num = 0;
        // for this size, do several cmd
        for(int j = 0; j < cmd_per_group; j ++) {
            pair<int, int> data = cmd_gen(size, Maxlba);

            Tree zns(Maxlba, KPP, L);
            zns.write_full();
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

    // close output file
    ofs.close();
    return 0;
}
