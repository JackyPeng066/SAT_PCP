#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <direct.h>
#include <sys/stat.h>

using namespace std;

// 建資料夾（存在就略過）
void ensure_folder(const string &folder) {
#ifdef _WIN32
    int ret = _mkdir(folder.c_str());
    if (ret != 0) {
        if (errno != EEXIST) {
            cerr << "[ERROR] cannot create folder: " << folder
                 << ", errno = " << errno << "\n";
            exit(1);
        }
    }
#else
    int ret = mkdir(folder.c_str(), 0777);
    if (ret != 0 && errno != EEXIST) {
        cerr << "[ERROR] cannot create folder: " << folder
             << ", errno = " << errno << "\n";
        exit(1);
    }
#endif
}

// 安全 stoi
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

// 切 token
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

// 轉成整數
vector<int> parse_ints(const string& line) {
    vector<int> out;
    auto toks = split_tokens(line);
    for (auto &s : toks) {
        try {
            out.push_back(stoi(s));
        } catch (...) {
            cerr << "[ERROR] parse_ints: bad token '" << s
                 << "' in line: '" << line << "'\n";
            exit(1);
        }
    }
    return out;
}

// 方便 push 子句
void add_clause(vector<vector<int>>& clauses, const vector<int>& lits) {
    clauses.push_back(lits);
}

// Sinz sequential counter：AtMost(k)
void encode_at_most_seq(const vector<int>& lits, int k,
                        int &nextVar,
                        vector<vector<int>>& clauses)
{
    int n = (int)lits.size();
    if (k >= n) return;
    if (k < 0)  return;

    if (k == 0) {
        for (int l : lits) add_clause(clauses, { -l });
        return;
    }

    vector<vector<int>> s(n, vector<int>(k+1, 0));
    for (int i = 1; i <= n-1; ++i)
        for (int j = 1; j <= k; ++j)
            s[i][j] = nextVar++;

    add_clause(clauses, { -lits[0], s[1][1] });

    for (int j = 2; j <= k; ++j)
        add_clause(clauses, { -s[1][j] });

    for (int i = 2; i <= n-1; ++i) {
        int xi = lits[i-1];
        add_clause(clauses, { -xi, s[i][1] });
        add_clause(clauses, { -s[i-1][1], s[i][1] });

        for (int j = 2; j <= k; ++j) {
            add_clause(clauses, { -xi, -s[i-1][j-1], s[i][j] });
            add_clause(clauses, { -s[i-1][j], s[i][j] });
        }

        add_clause(clauses, { -xi, -s[i-1][k] });
    }

    int xn = lits[n-1];
    add_clause(clauses, { -xn, -s[n-1][k] });
}

// Exactly(t)
void encode_exactly(const vector<int>& lits, int t,
                    int &nextVar,
                    vector<vector<int>>& clauses)
{
    int n = (int)lits.size();
    if (t < 0 || t > n) {
        cerr << "[ERROR] encode_exactly: impossible t=" << t
             << " for n=" << n << "\n";
        exit(1);
    }

    if (t == 0) {
        encode_at_most_seq(lits, 0, nextVar, clauses);
        return;
    }
    if (t == n) {
        for (int l : lits) add_clause(clauses, { l });
        return;
    }

    encode_at_most_seq(lits, t, nextVar, clauses);

    vector<int> negLits;
    for (int l : lits) negLits.push_back(-l);
    encode_at_most_seq(negLits, n - t, nextVar, clauses);
}


// ================== 主要：把骨架轉 CNF ==================
bool write_cnf_fopen(int L, int pair_index_one_based, const string& raw_line) {

    vector<int> vals = parse_ints(raw_line);
    int C = vals.size();

    if (C % 2 != 0) {
        cerr << "[ERROR] invalid line: " << C << " ints\n";
        return false;
    }

    int blocks = C / 2;
    if (L % blocks != 0) {
        cerr << "[ERROR] L not divisible by blocks\n";
        return false;
    }

    int blockSize = L / blocks;

    int baseVars = 2 * L;
    int nextVar  = baseVars + 1;

    vector<vector<int>> clauses;

    // ========= A row-sum blocks =========
    for (int b = 0; b < blocks; ++b) {
        int rs = vals[b];
        int t  = (blockSize + rs) / 2;
        if ((blockSize + rs) % 2 != 0) return false;

        vector<int> lits;
        int start = b * blockSize;
        for (int i = 0; i < blockSize; ++i)
            lits.push_back(1 + start + i);

        encode_exactly(lits, t, nextVar, clauses);
    }

    // ========= B row-sum blocks =========
    for (int b = 0; b < blocks; ++b) {
        int rs = vals[blocks + b];
        int t  = (blockSize + rs) / 2;
        if ((blockSize + rs) % 2 != 0) return false;

        vector<int> lits;
        int start = b * blockSize;
        for (int i = 0; i < blockSize; ++i)
            lits.push_back(L + 1 + start + i);

        encode_exactly(lits, t, nextVar, clauses);
    }

    // =====================================================================
    // ★★★★★ 新增：u=1 部分 PCP 條件 ★★★★★
    // =====================================================================
    int umax = L/2;
    for (int u = 1; u <= umax; ++u) {

        vector<int> eqlits_u;
        eqlits_u.reserve(2 * L);

        // ---- A: eqA_u[n] ----
        for (int n = 0; n < L; ++n) {
            int ai = 1 + n;
            int aj = 1 + ((n + u) % L);

            int p = nextVar++;        // 新變數 eqA_u[n]
            eqlits_u.push_back(p);

            // p ↔ (ai == aj)
            add_clause(clauses, { -p,  ai, -aj });
            add_clause(clauses, { -p, -ai,  aj });
            add_clause(clauses, {  p, -ai, -aj });
            add_clause(clauses, {  p,  ai,  aj });
        }

        // ---- B: eqB_u[n] ----
        for (int n = 0; n < L; ++n) {
            int bi = L + 1 + n;
            int bj = L + 1 + ((n + u) % L);

            int p = nextVar++;        // 新變數 eqB_u[n]
            eqlits_u.push_back(p);

            // p ↔ (bi == bj)
            add_clause(clauses, { -p,  bi, -bj });
            add_clause(clauses, { -p, -bi,  bj });
            add_clause(clauses, {  p, -bi, -bj });
            add_clause(clauses, {  p,  bi,  bj });
        }

        // (#eqA_u + #eqB_u) = L
        encode_exactly(eqlits_u, L, nextVar, clauses);

        cout << "[CHK] added u=" << u
             << " PCP partial constraint, eq-vars=" 
             << eqlits_u.size() << "\n";
    }
    // =====================================================================


    // ========== 寫檔 ==========
    string cnf_path = "results/" + to_string(L) + "_SAT/" +
                      to_string(L) + "_CNF.txt";

    cout << "[CHK] CNF path = " << cnf_path << "\n";

    FILE* fp = fopen(cnf_path.c_str(), "w");
    if (!fp) {
        cerr << "[ERROR] fopen failed\n";
        return false;
    }

    fprintf(fp, "c CNF generated for L=%d pair_index=%d\n", L, pair_index_one_based);
    fprintf(fp, "c raw line: %s\n", raw_line.c_str());
    fprintf(fp, "p cnf %d %d\n", nextVar - 1, (int)clauses.size());

    for (auto &c : clauses) {
        for (int lit : c) fprintf(fp, "%d ", lit);
        fprintf(fp, "0\n");
    }

    fclose(fp);
    cout << "[CHK] write_cnf_fopen finished.\n";
    return true;
}


// ================= 主程式 =================
int main(int argc, char** argv) {

    cout << "[INFO] sat_cnf start\n";

    if (argc != 3) {
        cerr << "Usage: ./sat_cnf <L> <pair_index>\n";
        return 1;
    }

    int L        = safe_stoi(argv[1], "L");
    int pair_idx = safe_stoi(argv[2], "pair_index");
    if (pair_idx <= 0) return 1;

    ensure_folder("results");
    string folder_sat = "results/" + to_string(L) + "_SAT";
    ensure_folder(folder_sat);

    string infile = "data/" + to_string(L) + "/" +
                    to_string(L) + "-pairs-found-1";

    ifstream fin(infile);
    if (!fin.is_open()) {
        cerr << "[ERROR] cannot open input: " << infile << "\n";
        return 1;
    }

    vector<string> lines;
    string line;
    while (getline(fin, line))
        if (!line.empty())
            lines.push_back(line);

    int idx0 = pair_idx - 1;
    if (idx0 < 0 || idx0 >= (int)lines.size()) {
        cerr << "[ERROR] pair_index out of range\n";
        return 1;
    }

    string selected = lines[idx0];
    cout << "[SELECTED] " << selected << "\n";

    if (!write_cnf_fopen(L, pair_idx, selected)) return 1;

    cout << "[INFO] sat_cnf finished.\n";
    return 0;
}
