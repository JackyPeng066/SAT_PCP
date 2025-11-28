#include <iostream>
#include <vector>
#include <string>
#include <sstream>
using namespace std;

/* ===========================================================
   =============== PARAMETERS — 你只需要改這裡 ===============
   =========================================================== */

// ★★★ 在這裡填入你的序列（±1），中間用空白隔開就好 ★★★
// 不需要管長度，下面會自動解析成 vector<int>。

// 範例：先把你那串貼在 A_STR，B 先複製一份一樣的做測試
const string A_STR =

"-1 1 1 -1 -1 -1 -1 1 1 -1 1 -1 1 -1 1 1 1 1 1 -1";

const string B_STR =

"-1 -1 1 1 -1 1 1 -1 1 -1 1 -1 -1 1 1 1 1 1 1 1";

// 之後你要測別的，就改上面兩條字串就好


/* ===========================================================
   =============== 字串 → vector<int> 解析 ====================
   =========================================================== */

vector<int> parse_seq(const string& s) {
    vector<int> v;
    stringstream ss(s);
    int x;
    while (ss >> x) {
        v.push_back(x);
    }
    return v;
}

/* ===========================================================
   =============== periodic ACF ===============================
   =========================================================== */

int R(const vector<int>& x, int u) {
    int L = x.size();
    long long s = 0;
    for (int n = 0; n < L; n++)
        s += 1LL * x[n] * x[(n + u) % L];
    return (int)s;
}


/* ===========================================================
   ========================= MAIN ============================
   =========================================================== */

int main() {
    ios::sync_with_stdio(false);

    // 解析 A, B
    vector<int> A = parse_seq(A_STR);
    vector<int> B = parse_seq(B_STR);

    int LA = (int)A.size();
    int LB = (int)B.size();

    cout << "Parsed A length = " << LA << "\n";
    cout << "Parsed B length = " << LB << "\n\n";

    // 長度檢查
    if (LA == 0 || LB == 0) {
        cout << "[ERROR] A 或 B 為空序列！\n";
        return 0;
    }
    if (LA != LB) {
        cout << "[ERROR] A 與 B 長度不同，無法檢查 PCP。\n";
        return 0;
    }
    int L = LA;

    // 值合法性檢查（是否都是 ±1）
    for (int x : A) {
        if (x != 1 && x != -1) {
            cout << "[ERROR] A 中出現非 ±1 的值：" << x << "\n";
            return 0;
        }
    }
    for (int x : B) {
        if (x != 1 && x != -1) {
            cout << "[ERROR] B 中出現非 ±1 的值：" << x << "\n";
            return 0;
        }
    }

    cout << "Checking PCP for L = " << L << "...\n";

    bool ok = true;
    for (int u = 1; u < L; u++) {
        int Ra = R(A, u);
        int Rb = R(B, u);
        if (Ra + Rb != 0) {
            cout << "\nNOT PCP!\n";
            cout << "Failed shift u = " << u
                 << ", RA = " << Ra
                 << ", RB = " << Rb
                 << ", RA+RB = " << (Ra + Rb) << "\n";
            ok = false;
            break;
        }
    }

    if (ok) {
        cout << "\nYES, PCP!\n";
        cout << "(All RA(u)+RB(u) = 0 for u = 1..L-1)\n";
    }

    return 0;
}
