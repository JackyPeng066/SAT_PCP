SAT-PCP is a SAT-based tool for reconstructing Periodic Complementary Pairs (PCP) from existing compressed skeletons. It converts skeletons into CNF, runs a SAT solver, and filters valid PCP results.

Setup:(msys2 ucrt64)
./cook.sh

Build:
make

Data:
data/<L>/<L>-pairs-found-1

Run:
./runmulti.sh <L> <start pair> <end pair> <SAT number>