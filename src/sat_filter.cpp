#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cerrno>

#ifdef _WIN32
    #include <direct.h>
#else
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

using namespace std;

// =========================================================
// 基本工具
// =========================================================

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


// =========================================================
// 讀取 CNF 的 p cnf 行 → 得到 maxVar
// =========================================================
int read_maxVar_from_cnf(const string& cnfPath) {
    cout << "[CHK] read_maxVar_from_cnf: " << cnfPath << "\n";

    ifstream fin(cnfPath);
    if (!fin.is_open()) {
        cerr << "[ERROR] cannot open CNF: " << cnfPath << "\n";
        exit(1);
    }

    string line;
    while (getline(fin, line)) {
        if (!line.empty() && line[0] == 'p') {
            auto t = split_tokens(line);
            if (t.size() < 4) {
                cerr << "[ERROR] malformed p-line: " << line << "\n";
                exit(1);
            }
            int vars = stoi(t[2]);
            cout << "[CHK] p-line found, maxVar=" << vars << "\n";
            return vars;
        }
    }

    cerr << "[ERROR] p-line not found in CNF\n";
    exit(1);
}


// =========================================================
// periodic ACF
// =========================================================
void periodic_acf(const vector<int>& x, vector<int>& R) {
    int L = (int)x.size();
    R.assign(L, 0);
    for (int u = 0; u < L; ++u) {
        long long sum = 0;
        for (int n = 0; n < L; ++n) {
            int j = n + u;
            if (j >= L) j -= L;
            sum += (long long)x[n] * x[j];
        }
        R[u] = (int)sum;
    }
}

// =========================================================
// PCP check
// =========================================================
bool is_PCP(const vector<int>& A, const vector<int>& B) {
    int L = (int)A.size();
    vector<int> Ra, Rb;
    periodic_acf(A, Ra);
    periodic_acf(B, Rb);

    for (int u = 1; u < L; ++u) {
        if (Ra[u] + Rb[u] != 0) return false;
    }
    return true;
}

// 印出序列
void print_seq(const vector<int>& s, const string& name) {
    cout << name << ":";
    for (int v : s) cout << " " << v;
    cout << "\n";
}


// =========================================================
// main()
// =========================================================
int main(int argc, char** argv) {

    cout << "[CHK] sat_filter main start, argc=" << argc << "\n";

    if (argc != 2) {
        cerr << "Usage: ./sat_filter <L>\n";
        return 1;
    }

    int L = safe_stoi(argv[1], "L");
    cout << "[CHK] L = " << L << "\n";

    ensure_folder("results");
    string dir = "results/" + to_string(L) + "_SAT";
    ensure_folder(dir);

    string cnfPath   = dir + "/" + to_string(L) + "_CNF.txt";
    string solSorted = dir + "/" + to_string(L) + "_sat_solution_sorted.txt";
    string outPCP    = dir + "/" + to_string(L) + "_sat_pcp.txt";

    cout << "[INFO] CNF file   = " << cnfPath   << "\n";
    cout << "[INFO] Sorted sol = " << solSorted << "\n";
    cout << "[INFO] PCP out    = " << outPCP    << "\n";

    int maxVar = read_maxVar_from_cnf(cnfPath);

    ifstream fin(solSorted);
    if (!fin.is_open()) {
        cerr << "[ERROR] cannot open sorted solutions: " << solSorted << "\n";
        return 1;
    }

    ofstream fout(outPCP, ios::app);
    if (!fout.is_open()) {
        cerr << "[ERROR] cannot open PCP output: " << outPCP << "\n";
        return 1;
    }

    string line;
    int model_id = 0;
    int pcp_found = 0;

    while (getline(fin, line)) {
        if (line.empty()) continue;
        if (line[0] != 'v') continue;

        model_id++;
        cout << "\n================ MODEL " << model_id << " ================\n";

        // decode model
        vector<int> assign(maxVar + 1, 0); // 1..maxVar
        string body = line.substr(1);
        auto toks = split_tokens(body);
        for (auto &s : toks) {
            int lit = stoi(s);
            if (lit == 0) break;
            int v = abs(lit);
            if (v >= 1 && v <= maxVar)
                assign[v] = (lit > 0 ? 1 : -1);
        }

        vector<int> A(L), B(L);
        bool ok = true;

        for (int i = 0; i < L; ++i) {
            int vA = assign[i+1];
            int vB = assign[L + 1 + i];
            if (vA == 0 || vB == 0) {
                cout << "[WARN] model " << model_id << " missing A/B var at pos " << i << "\n";
                ok = false;
                break;
            }
            A[i] = (vA > 0 ? 1 : -1);
            B[i] = (vB > 0 ? 1 : -1);
        }

        if (!ok) continue;

        // 顯示還原序列
        print_seq(A, "A");
        print_seq(B, "B");

        // PCP check
        if (is_PCP(A, B)) {
            pcp_found++;
            cout << "[INFO] model " << model_id << " is PCP\n";

            fout << "c PCP model " << model_id << "\n";
            fout << "A:";
            for (int v : A) fout << " " << v;
            fout << "\nB:";
            for (int v : B) fout << " " << v;
            fout << "\n\n";
        } else {
            cout << "[INFO] model " << model_id << " NOT PCP\n";
        }
    }

    cout << "\n====================================\n";
    cout << "[INFO] total models scanned = " << model_id << "\n";
    cout << "[INFO] total PCP found      = " << pcp_found << "\n";
    cout << "[INFO] sat_filter finished.\n";

    return 0;
}
