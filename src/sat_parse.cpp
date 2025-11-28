#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cerrno>

#ifdef _WIN32
    #include <direct.h>
#else
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

using namespace std;

// ===================== 共用工具 =====================

void ensure_folder(const string &folder) {
#ifdef _WIN32
    int ret = _mkdir(folder.c_str());
    if (ret != 0 && errno != EEXIST) {
        cerr << "[ERROR] cannot create folder: " << folder
             << ", errno=" << errno << "\n";
        exit(1);
    }
#else
    int ret = mkdir(folder.c_str(), 0777);
    if (ret != 0 && errno != EEXIST) {
        cerr << "[ERROR] cannot create folder: " << folder
             << ", errno=" << errno << "\n";
        exit(1);
    }
#endif
}

int safe_stoi(const char* s, const string& tag) {
    if (!s) {
        cerr << "[ERROR] stoi null (" << tag << ")\n";
        exit(1);
    }
    try {
        return stoi(string(s));
    } catch (...) {
        cerr << "[ERROR] stoi failed (" << tag << "): " << s << "\n";
        exit(1);
    }
}

vector<string> split_tokens(const string& line) {
    vector<string> t;
    string cur;
    for (char c : line) {
        if (c==' ' || c=='\t' || c=='\r' || c=='\n') {
            if (!cur.empty()) {
                t.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) t.push_back(cur);
    return t;
}

// ===================== 讀 CNF 的 p cnf 行 =====================

int read_maxVar_from_cnf(const string& cnfPath) {
    cout << "[CHK] read_maxVar_from_cnf: " << cnfPath << "\n";

    FILE* fp = fopen(cnfPath.c_str(), "r");
    if (!fp) {
        cerr << "[ERROR] cannot open CNF file: " << cnfPath << "\n";
        exit(1);
    }

    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        if (buf[0] == 'p') {
            string line(buf);
            auto t = split_tokens(line);
            if (t.size() >= 4) {
                int vars = stoi(t[2]);
                if (vars <= 0) {
                    cerr << "[ERROR] vars<=0 in p-line: " << line << "\n";
                    fclose(fp);
                    exit(1);
                }
                cout << "[CHK] p-line found, vars=" << vars << "\n";
                fclose(fp);
                return vars;
            } else {
                cerr << "[ERROR] malformed p-line: " << line << "\n";
                fclose(fp);
                exit(1);
            }
        }
    }

    fclose(fp);
    cerr << "[ERROR] no p-line in CNF file: " << cnfPath << "\n";
    exit(1);
}

// ===================== 解析 SAT 檔 =====================

bool parse_sat_file(const string& satPath,
                    vector<int>& model,
                    bool &isSAT)
{
    cout << "[CHK] parse_sat_file: " << satPath << "\n";

    FILE* fp = fopen(satPath.c_str(), "r");
    if (!fp) {
        cerr << "[ERROR] cannot open SAT file: " << satPath << "\n";
        return false;
    }

    isSAT = false;
    model.clear();
    bool seen_status = false;

    char buf[65536];
    while (fgets(buf, sizeof(buf), fp)) {
        string line(buf);
        if (line.empty()) continue;

        if (line[0] == 's') {
            seen_status = true;
            if (line.find("UNSAT") != string::npos) {
                isSAT = false;
                cout << "[INFO] SAT result: UNSAT\n";
            } else if (line.find("SATISFIABLE") != string::npos) {
                isSAT = true;
                cout << "[INFO] SAT result: SATISFIABLE\n";
            }
        }

        if (line[0] == 'v') {
            auto toks = split_tokens(line.substr(1));
            for (auto &s : toks) {
                int v = stoi(s);
                if (v == 0) break;
                model.push_back(v);
            }
        }
    }

    fclose(fp);

    if (!seen_status) {
        cerr << "[ERROR] no status line (s ...) in SAT file.\n";
        return false;
    }

    return true;
}

// ===================== 讀 CNF 全部行 =====================

bool read_cnf_lines(const string& cnfPath, vector<string>& lines) {
    cout << "[CHK] read_cnf_lines: " << cnfPath << "\n";

    FILE* fp = fopen(cnfPath.c_str(), "r");
    if (!fp) {
        cerr << "[ERROR] cannot open CNF file: " << cnfPath << "\n";
        return false;
    }

    lines.clear();
    char buf[65536];
    while (fgets(buf, sizeof(buf), fp)) {
        string ln(buf);
        if (!ln.empty() && ln.back()=='\n') ln.pop_back();
        lines.push_back(ln);
    }
    fclose(fp);

    cout << "[CHK] CNF lines = " << lines.size() << "\n";
    if (lines.empty()) {
        cerr << "[ERROR] CNF file is empty.\n";
        return false;
    }
    return true;
}

// ===================== p cnf clause+1 =====================

bool bump_clause_count(vector<string>& lines) {
    for (auto &ln : lines) {
        if (!ln.empty() && ln[0] == 'p') {
            auto t = split_tokens(ln);
            if (t.size() < 4) {
                cerr << "[ERROR] malformed p-line (bump): " << ln << "\n";
                return false;
            }
            int vars = stoi(t[2]);
            int cls  = stoi(t[3]);
            cls++;
            ln = "p cnf " + to_string(vars) + " " + to_string(cls);
            cout << "[CHK] bump clause: vars=" << vars
                 << ", clauses=" << cls << "\n";
            return true;
        }
    }
    cerr << "[ERROR] no p-line when bumping clause.\n";
    return false;
}

// ===================== 由 model 產生 blocking clause =====================

vector<int> make_block_clause(const vector<int>& model, int maxVar) {
    vector<int> block;
    block.reserve(model.size());
    for (int lit : model) {
        int v = abs(lit);
        if (v >= 1 && v <= maxVar) {
            block.push_back(-lit);
        }
    }
    cout << "[CHK] block clause size = " << block.size() << "\n";
    return block;
}

// ===================== 原子方式寫 CNF + blocking clause =====================

bool write_cnf_with_block_atomic(const string& cnfPath,
                                 const vector<string>& lines,
                                 const vector<int>& blockClause)
{
    string tmpPath = cnfPath + ".tmp";
    cout << "[CHK] write_cnf_with_block_atomic: " << tmpPath << " -> " << cnfPath << "\n";

    FILE* fp = fopen(tmpPath.c_str(), "w");
    if (!fp) {
        cerr << "[ERROR] cannot write temp CNF file: " << tmpPath << "\n";
        return false;
    }

    for (auto &ln : lines) {
        fprintf(fp, "%s\n", ln.c_str());
    }
    for (int v : blockClause) fprintf(fp, "%d ", v);
    fprintf(fp, "0\n");

    fclose(fp);

    remove(cnfPath.c_str());

    if (rename(tmpPath.c_str(), cnfPath.c_str()) != 0) {
        cerr << "[ERROR] rename temp CNF failed ("
             << tmpPath << " -> " << cnfPath
             << "), errno=" << errno << "\n";
        return false;
    }

    return true;
}

// ===================== main =====================

int main(int argc, char** argv) {

    cout << "[CHK] sat_parse main start, argc=" << argc << "\n";

    if (argc != 2) {
        cerr << "Usage: ./sat_parse <L>\n";
        return 1;
    }

    int L = safe_stoi(argv[1], "L");
    cout << "[CHK] L = " << L << "\n";

    ensure_folder("results");
    string satDir = "results/" + to_string(L) + "_SAT";
    ensure_folder(satDir);

    string satPath = satDir + "/" + to_string(L) + "_SAT.txt";
    string cnfPath = satDir + "/" + to_string(L) + "_CNF.txt";
    string solPath = satDir + "/" + to_string(L) + "_sat_solution.txt";

    cout << "[INFO] SAT input   = " << satPath << "\n";
    cout << "[INFO] CNF file    = " << cnfPath << "\n";
    cout << "[INFO] solutions   = " << solPath << "\n";

    int maxVar = read_maxVar_from_cnf(cnfPath);
    cout << "[CHK] maxVar = " << maxVar << "\n";

    // 1) 解析 SAT 檔案
    vector<int> model;
    bool isSAT = false;

    if (!parse_sat_file(satPath, model, isSAT)) {
        cerr << "[ERROR] parse SAT file failed.\n";
        return 1;
    }

    if (!isSAT) {
        cout << "[INFO] result is UNSAT, nothing to block.\n";
        return 0;
    }

    if (model.empty()) {
        cerr << "[ERROR] SAT says SATISFIABLE but model is empty.\n";
        return 1;
    }

    cout << "[CHK] model size = " << model.size() << "\n";

    // 2) 記錄這組 model
    FILE* fpSol = fopen(solPath.c_str(), "a");
    if (!fpSol) {
        cerr << "[ERROR] cannot open solution file: " << solPath << "\n";
        return 1;
    }
    fprintf(fpSol, "v");
    for (int lit : model) fprintf(fpSol, " %d", lit);
    fprintf(fpSol, " 0\n");
    fclose(fpSol);

    // 3) 讀 CNF、+1 clause、加 blocking clause、原子覆寫
    vector<string> cnfLines;
    if (!read_cnf_lines(cnfPath, cnfLines)) {
        cerr << "[ERROR] read CNF failed.\n";
        return 1;
    }
    if (!bump_clause_count(cnfLines)) {
        cerr << "[ERROR] bump clause count failed.\n";
        return 1;
    }

    vector<int> block = make_block_clause(model, maxVar);
    if (block.empty()) {
        cerr << "[WARN] blocking clause empty? skip CNF update.\n";
        return 0;
    }

    if (!write_cnf_with_block_atomic(cnfPath, cnfLines, block)) {
        cerr << "[ERROR] write CNF with block failed.\n";
        return 1;
    }

    cout << "[INFO] sat_parse finished normally.\n";
    return 0;
}
