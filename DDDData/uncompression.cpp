#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <map>
#include <vector>
#include <set>
#include <array>
#include <time.h>
#include "fftw3.h"
#include "../lib/array.h"
#include "../lib/decomps.h"
#include "../lib/fourier.h"
#include "../lib/equivalence.h"
#include <tgmath.h>
#include <algorithm>
#include <fstream>

using namespace std;

double norm(fftw_complex dft) {
    return dft[0] * dft[0] + dft[1] * dft[1];
}

int main(int argc, char ** argv) {

    // ======================
    //  UNCOMPRESS 輸出上限
    // ======================
    const unsigned long long MAX_A_OUTPUT = 10000000ULL;  // A 最多輸出 1000 萬筆
    const unsigned long long MAX_B_OUTPUT = 10000000ULL;  // B 最多輸出 1000 萬筆

    unsigned long long printedA = 0;
    unsigned long long printedB = 0;

    int ORDER       = stoi(argv[3]);
    int COMPRESS    = stoi(argv[4]);
    int NEWCOMPRESS = stoi(argv[5]);

    int LEN = ORDER / COMPRESS;

    printf("Uncompressing sequence of length %d\n", LEN);

    fftw_complex *in, *out;
    fftw_plan p;

    in  = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (ORDER / NEWCOMPRESS));
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (ORDER / NEWCOMPRESS));
    p   = fftw_plan_dft_1d((ORDER) / NEWCOMPRESS, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    int linenumber = stoi(argv[1]);
    int procnum    = stoi(argv[2]);

    char fname[100];

    if (procnum == 0) {
        sprintf(fname, "results/%d-pairs-found", ORDER);
    } else {
        sprintf(fname, "results/%d-pairs-found-%d", ORDER, procnum);
    }

    ifstream file(fname);
    string line;
    string letter;

    if (!file) {
        printf("Bad file\n");
        return -1;
    }

    vector<int> origa(LEN);
    vector<int> origb(LEN);

    int i = 1;
    while (i < linenumber && getline(file, line)) {
        i++;
    }

    i = 0;
    while (file.good() && i < LEN) {
        file >> letter;
        origa[i] = stoi(letter);
        i++;
    }

    i = 0;
    while (file.good() && i < LEN) {
        file >> letter;
        origb[i] = stoi(letter);
        i++;
    }

    vector<int> seq(ORDER / NEWCOMPRESS);

    set<int> alphabet;

    if (COMPRESS % 2 == 0) {
        for (int i = 0; i <= COMPRESS; i += 2) {
            alphabet.insert(i);
            alphabet.insert(-i);
        }
    } else {
        for (int i = 1; i <= COMPRESS; i += 2) {
            alphabet.insert(i);
            alphabet.insert(-i);
        }
    }

    set<int> newalphabet;

    if (NEWCOMPRESS % 2 == 0) {
        for (int i = 0; i <= NEWCOMPRESS; i += 2) {
            newalphabet.insert(i);
            newalphabet.insert(-i);
        }
    } else {
        for (int i = 1; i <= NEWCOMPRESS; i += 2) {
            newalphabet.insert(i);
            newalphabet.insert(-i);
        }
    }

    // 所有 NEWCOMPRESS-level 的分割
    vector<vector<int>> parts = getCombinations(COMPRESS / NEWCOMPRESS, newalphabet);

    vector<vector<int>> partition;
    map<int, vector<vector<int>>> partitions;

    // generate all permutations of possible decompositions for each letter in the alphabet
    for (int letter_val : alphabet) {
        partition.clear();
        for (vector<int> part : parts) {
            int sum = 0;
            for (size_t j = 0; j < part.size(); j++) {
                sum += part[j];
            }
            if (sum == letter_val) {
                do {
                    partition.push_back(part);
                } while (next_permutation(part.begin(), part.end()));
            }
        }
        partitions.insert(make_pair(letter_val, partition));
    }

    sprintf(fname, "results/%d/%d-unique-filtered-a_%d", ORDER, ORDER, procnum);
    FILE * outa = fopen(fname, "w");

    sprintf(fname, "results/%d/%d-unique-filtered-b_%d", ORDER, ORDER, procnum);
    FILE * outb = fopen(fname, "w");

    // shift original sequence such that the element with the largest number of permutations is in the front
    set<int> seta;
    for (int element : origa) {
        seta.insert(element);
    }

    set<int> setb;
    for (int element : origb) {
        setb.insert(element);
    }

    int max = 0;
    int best = 0;

    for (int element : seta) {
        if (partitions.at(element).size() > (size_t)max) {
            max  = (int)partitions.at(element).size();
            best = element;
        }
    }
    for (int idx = 0; idx < (int)origa.size(); idx++) {
        if (origa[idx] == best) {
            rotate(origa.begin(), origa.begin() + idx, origa.end());
            break;
        }
    }

    max = 0;
    best = 0;
    for (int element : setb) {
        if (partitions.at(element).size() > (size_t)max) {
            max  = (int)partitions.at(element).size();
            best = element;
        }
    }
    for (int idx = 0; idx < (int)origb.size(); idx++) {
        if (origb[idx] == best) {
            rotate(origb.begin(), origb.begin() + idx, origb.end());
            break;
        }
    }

    set<vector<int>> perma;
    for (vector<int> perm : partitions.at(origa[0])) {
        set<vector<int>> equiv = generateUncompress(perm);
        perma.insert(*equiv.begin());
    }

    vector<vector<int>> newfirsta;
    for (vector<int> perm : perma) {
        newfirsta.push_back(perm);
    }

    set<vector<int>> permb;
    for (vector<int> perm : partitions.at(origb[0])) {
        set<vector<int>> equiv = generateUncompress(perm);
        permb.insert(*equiv.begin());
    }

    vector<vector<int>> newfirstb;
    for (vector<int> perm : permb) {
        newfirstb.push_back(perm);
    }

    unsigned long long int count = 0;
    int curr = 0;
    vector<int> stack(LEN, 0);

    // =======================
    // Uncompress A (with cap)
    // =======================

    printf("Uncompressing A\n");

    bool stopA = false;

    while (curr != -1 && !stopA) {

        while (curr != LEN - 1 && !stopA) {

            vector<int> permutation = partitions.at(origa[curr])[stack[curr]];

            if (curr == 0) {
                permutation = newfirsta[stack[curr]];
            }

            for (int k = 0; k < COMPRESS / NEWCOMPRESS; k++) {
                seq[curr + (LEN * k)] = permutation[k];
            }
            stack[curr]++;
            curr++;
        }

        // base case: curr == LEN - 1
        if (curr == LEN - 1 && !stopA) {

            for (vector<int> permutation : partitions.at(origa[curr])) {

                count++;

                for (int k = 0; k < COMPRESS / NEWCOMPRESS; k++) {
                    seq[curr + (LEN * k)] = permutation[k];
                }

                for (size_t t = 0; t < seq.size(); t++) {
                    in[t][0] = (double)seq[t];
                    in[t][1] = 0;
                }

                fftw_execute(p);

                if (dftfilter(out, (int)seq.size(), ORDER)) {
                    // 只對通過 dftfilter 的做輸出與統計
                    for (size_t t = 0; t < seq.size() / 2; t++) {
                        fprintf(outa, "%d", (int)rint(norm(out[t])));
                    }
                    fprintf(outa, " ");
                    for (int num : seq) {
                        fprintf(outa, "%d ", num);
                    }
                    fprintf(outa, "\n");

                    printedA++;
                    if (printedA >= MAX_A_OUTPUT) {
                        printf("A reached output limit (%llu), stop early.\n", MAX_A_OUTPUT);
                        stopA = true;
                        break;  // break for(permutation)
                    }
                }
            }

            if (stopA) break;   // break while(curr != -1 && !stopA)

            // backtrack
            curr--;

            while (curr >= 0 &&
                   ( ( (size_t)stack[curr] == partitions.at(origa[curr]).size() )
                     || (curr == 0 && (size_t)stack[curr] == newfirsta.size()) )) {

                stack[curr] = 0;
                curr--;
            }
        }
    }

    printf("%llu A sequences checked\n", count);
    count = 0;

    // =======================
    // Uncompress B (with cap)
    // =======================

    curr = 0;
    vector<int> stackb(LEN, 0);
    stack = stackb;

    bool stopB = false;

    while (curr != -1 && !stopB) {

        while (curr != LEN - 1 && !stopB) {
            vector<int> permutation = partitions.at(origb[curr])[stack[curr]];

            if (curr == 0) {
                permutation = newfirstb[stack[curr]];
            }

            for (int k = 0; k < COMPRESS / NEWCOMPRESS; k++) {
                seq[curr + (LEN * k)] = permutation[k];
            }
            stack[curr]++;
            curr++;
        }

        if (curr == LEN - 1 && !stopB) {

            for (vector<int> permutation : partitions.at(origb[curr])) {

                count++;

                for (int k = 0; k < COMPRESS / NEWCOMPRESS; k++) {
                    seq[curr + (LEN * k)] = permutation[k];
                }

                for (size_t t = 0; t < seq.size(); t++) {
                    in[t][0] = (double)seq[t];
                    in[t][1] = 0;
                }

                fftw_execute(p);

                if (dftfilter(out, (int)seq.size(), ORDER)) {
                    for (size_t t = 0; t < seq.size() / 2; t++) {
                        fprintf(outb, "%d", ORDER * 2 - (int)rint(norm(out[t])));
                    }
                    fprintf(outb, " ");
                    for (int num : seq) {
                        fprintf(outb, "%d ", num);
                    }
                    fprintf(outb, "\n");

                    printedB++;
                    if (printedB >= MAX_B_OUTPUT) {
                        printf("B reached output limit (%llu), stop early.\n", MAX_B_OUTPUT);
                        stopB = true;
                        break;  // break for(permutation)
                    }
                }
            }

            if (stopB) break;

            curr--;

            while (curr >= 0 &&
                   ( ( (size_t)stack[curr] == partitions.at(origb[curr]).size() )
                     || (curr == 0 && (size_t)stack[curr] == newfirstb.size()) )) {

                stack[curr] = 0;
                curr--;
            }
        }
    }

    printf("%llu B sequences checked\n", count);
    count = 0;

    fftw_free(in);
    fftw_free(out);
    fftw_destroy_plan(p);

    fclose(outa);
    fclose(outb);

    return 0;
}
