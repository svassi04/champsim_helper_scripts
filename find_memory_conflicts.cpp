#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std;

/*
g++ -std=c++17 -O2 -o find_memory_matches_flags find_memory_block_matches.cpp
*/

static vector<string> tokenizeWS(const string& line) {
    vector<string> out;
    istringstream iss(line);
    string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

static string basenameNoDirs(const string& path) {
    try { return fs::path(path).filename().string(); }
    catch (...) {
        auto pos = path.find_last_of("/\\");
        return (pos == string::npos) ? path : path.substr(pos + 1);
    }
}

static string prefixFromFirst(const string& path) {
    string base = basenameNoDirs(path);
    auto pos = base.find('_');
    if (pos == string::npos) return base;
    return base.substr(0, pos);
}

static string joinWithSpaces(const vector<string>& v) {
    string s;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s.push_back(' ');
        s += v[i];
    }
    return s;
}

static string addSuffixBeforeExtension(const string& path,
                                            const string& suffix) {
    fs::path p(path);
    fs::path parent = p.parent_path();
    string stem = p.stem().string();        // "memc_block_train_debug"
    string ext  = p.extension().string();   // ".csv" (or "")
    fs::path newname = parent / (stem + suffix + ext);
    return newname.string();
}


static void printUsage(const char* prog){
    cerr<<"Usage: "<<prog<<" [options] <file1> <file2> ...\n"
        <<"Options:\n"
        <<"  -s, --shift <bits>   Cache-line shift (default 6 => 64-byte blocks)\n"
        <<"  -o, --out   <file>   Output CSV filename (default <prefix>_source_dest_block_matches.csv)\n"
        <<"  -B, --breakdown      Also write per-file breakdown to <out>_breakdown\n"
        <<"  -h, --help           Show this help and exit\n";
}


int main(int argc, char** argv) {

    ios::sync_with_stdio(false);

    // ---- Parse flags ----
    int shift_size = 6;
    string outName; 
    bool outSet=false;
    bool makeBreakdown=false;
    vector<string> files;

    for (int i=1;i<argc;++i){
        string arg = argv[i];
        if (arg=="-h"||arg=="--help"){ printUsage(argv[0]); return 0; }
        else if ((arg=="-s"||arg=="--shift") && i+1<argc){
            try { shift_size = stoi(argv[++i]); } catch (...) { cerr<<"Invalid shift size\n"; return 1; }
            if (shift_size<0 || shift_size>=64){ cerr<<"Shift size must be in [0,63]\n"; return 1; }
        } else if ((arg=="-o"||arg=="--out") && i+1<argc){
            outName = argv[++i]; outSet=true;
        } else if (arg=="-B"||arg=="--breakdown"){
            makeBreakdown = true;
        } else if (!arg.empty() && arg[0]=='-'){
            cerr<<"Unknown option: "<<arg<<"\n"; printUsage(argv[0]); return 1;
        } else {
            files.push_back(arg);
        }
    }
    if (files.empty()){ printUsage(argv[0]); return 1; }

    if (!outSet){
        string prefix = prefixFromFirst(files.front());
        outName = prefix + "_source_dest_block_matches.csv";
    }

    // ---- pass 1: collect destination owners per block + dest line counts ----
    unordered_map<uint64_t, unordered_set<string>> destOwners; // block -> {owner basenames}
    unordered_set<uint64_t> destSet;
    unordered_map<string, size_t> perFileDestLines;            // basename -> #destination lines

    for (const auto& path : files){
        ifstream in(path);
        if (!in){ cerr<<"Warning: could not open "<<path<<" (skipping)\n"; continue; }
        const string base = basenameNoDirs(path);
        string line;
        while (getline(in,line)){
            auto t = tokenizeWS(line);
            if (t.size()>=3 && t[1]=="destination_memory:"){
                uint64_t addr=0; try{ addr = stoull(t[2], nullptr, 16);} catch(...){ continue; }
                uint64_t blk = (shift_size==0)? addr : ((addr>>shift_size)<<shift_size);
                destOwners[blk].insert(base);
                destSet.insert(blk);
                perFileDestLines[base]++; // raw destination lines
            }
        }
    }

    // ---- write matches CSV & gather breakdown metrics ----
    ofstream out(outName);
    if (!out){ cerr<<"Error: cannot write "<<outName<<"\n"; return 1; }
    out<<"SourceFile,SourceAddress,DestinationFiles\n";

    unordered_map<string, size_t> perFileSrcLines;             // basename -> #source lines
    unordered_map<string, size_t> perFileMatchesWithSelf;      // basename -> emitted rows where owners include file
    unordered_map<string, size_t> perFileMatchesWithoutSelf;   // basename -> emitted rows where owners exclude file

    // Optionally avoid duplicate rows (same file+addr+owners)
    unordered_set<string> emitted;

    for (const auto& path : files){
        ifstream in(path);
        if (!in) continue;
        const string base = basenameNoDirs(path);
        string line;

        while (getline(in,line)){
            auto t = tokenizeWS(line);
            if (t.size()>=3 && t[1]=="source_memory:"){
                perFileSrcLines[base]++; // raw source lines

                const string& addrStr = t[2];
                uint64_t addr=0; try{ addr = stoull(addrStr, nullptr, 16);} catch(...){ continue; }
                uint64_t blk = (shift_size==0)? addr : ((addr>>shift_size)<<shift_size);

                if (!destSet.count(blk)) continue;
                auto it = destOwners.find(blk);
                if (it==destOwners.end() || it->second.empty()) continue;

                // determine owners list + self membership
                vector<string> owners(it->second.begin(), it->second.end());
                sort(owners.begin(), owners.end());
                const bool isSelf = binary_search(owners.begin(), owners.end(), base);
                const string ownersJoined = joinWithSpaces(owners);

                // Apply same emission rule as before:
                if (!isSelf || (isSelf && owners.size()>1)){
                    const string key = base + "|" + addrStr + "|" + ownersJoined;
                    if (emitted.insert(key).second){
                        out<<base<<','<<addrStr<<", "<<ownersJoined<<'\n';
                        if (isSelf) perFileMatchesWithSelf[base]++;
                        else        perFileMatchesWithoutSelf[base]++;
                    }
                }
            }
        }
    }
    out.close();

    // ---- optional breakdown ----
    if (makeBreakdown){
        const string breakdownName = addSuffixBeforeExtension(outName, "_breakdown");
        ofstream bout(breakdownName);
        if (!bout){ cerr<<"Error: cannot write "<<breakdownName<<"\n"; return 1; }

        bout<<"File,MatchesWithSelf,MatchesWithoutSelf,TotalSources,TotalDestinations\n";
        for (const auto& path : files){
            const string base = basenameNoDirs(path);
            size_t withSelf  = perFileMatchesWithSelf[base];
            size_t withoutSelf = perFileMatchesWithoutSelf[base];
            size_t srcCnt    = perFileSrcLines[base];
            size_t dstCnt    = perFileDestLines[base];
            bout<<base<<','<<withSelf<<','<<withoutSelf<<','<<srcCnt<<','<<dstCnt<<'\n';
        }
        bout.close();
        cout<<"Breakdown written to "<<breakdownName<<"\n";
    }

    cout<<"Done. Results: "<<outName<<"\n";
    return 0;
}
