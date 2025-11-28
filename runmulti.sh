#!/usr/bin/bash

# ===============================
# Usage: ./run_multi.sh <L> <start> <end> <R> 
# ===============================

L="$1"
START="$2"
END="$3"
R="$4"

if [ -z "$L" ] || [ -z "$START" ] || [ -z "$END" ] || [ -z "$R" ]; then
    echo "[ERROR] Usage: ./run_multi.sh <L> <start> <end> <R>"
    exit 1
fi

SATDIR="results/${L}_SAT"
mkdir -p "$SATDIR"

echo "[INFO] -------- RUN MULTI START --------"
echo "[INFO] L=$L   start=$START   end=$END   R=$R"
echo

total_start=$(date +%s)

for ((p=$START; p<=$END; p++)); do
    echo "==============================="
    echo "[INFO] Start pair index $p"
    echo "==============================="

    round_start=$(date +%s)

    # Step 1: 產生 CNF
    echo "[INFO] sat_cnf generating CNF for pair $p ..."
    ./bin/sat_cnf "$L" "$p"
    if [ $? -ne 0 ]; then
        echo "[ERROR] sat_cnf failed at pair $p"
        exit 1
    fi

    # Step 2: 執行 R 次 SAT solve
    echo "[INFO] running SAT loop R=$R ..."
    ./run.sh "$L" "$R"
    if [ $? -ne 0 ]; then
        echo "[ERROR] run.sh failed at pair $p"
        exit 1
    fi

    # Step 3: 執行 filter 還原序列 + 檢查 PCP
    echo "[INFO] filtering (check PCP) ..."
    ./bin/sat_filter "$L"
    if [ $? -ne 0 ]; then
        echo "[ERROR] sat_filter failed at pair $p"
        exit 1
    fi

    round_end=$(date +%s)
    diff=$((round_end - round_start))
    echo "[INFO] Finished pair $p in $diff seconds"
    echo
done

total_end=$(date +%s)
total_diff=$((total_end - total_start))

echo "----------------------------------------"
echo "[INFO] run_multi DONE. Total time: $total_diff seconds"
echo "----------------------------------------"
