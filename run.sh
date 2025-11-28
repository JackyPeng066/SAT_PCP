#!/usr/bin/bash

# ============================
# 讀取輸入參數
# ============================
L="$1"
R="$2"

if [ -z "$L" ] || [ -z "$R" ]; then
    echo "[ERROR] usage: ./run.sh <L> <run_times>"
    exit 1
fi

# ============================
# 準備路徑
# ============================
SATDIR="results/${L}_SAT"
CNF="${SATDIR}/${L}_CNF.txt"
SATTXT="${SATDIR}/${L}_SAT.txt"
SOL="${SATDIR}/${L}_sat_solution.txt"
SOL_UNIQ="${SATDIR}/${L}_sat_solution_sorted.txt"

# ============================
# 計算總時間
# ============================
START_TOTAL=$(date +%s)

# ============================
# 主 loop
# ============================
for ((i=1; i<=R; i++))
do
    echo "===== round $i / $R ====="
    
    START_ROUND=$(date +%s)

    ./bin/glucose.exe -model "$CNF" > "$SATTXT"
    ./bin/sat_parse "$L"

    END_ROUND=$(date +%s)
    RUNTIME=$((END_ROUND - START_ROUND))
    echo "[TIME] round $i spent ${RUNTIME} sec"
done

# ============================
# sort + uniq
# ============================
sort "$SOL" | uniq > "$SOL_UNIQ"

# ============================
# 總耗時
# ============================
END_TOTAL=$(date +%s)
TOTAL=$((END_TOTAL - START_TOTAL))
echo "[TIME] total runtime = ${TOTAL} sec"

echo "[INFO] All done."
echo "[INFO] Unique solutions saved to: $SOL_UNIQ"
