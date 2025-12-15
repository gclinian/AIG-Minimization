#!/bin/bash

# ==============================================================================
# Batch Runner for IWLS 2025 Benchmarks (Zero-Padded)
# Usage: ./run_batch.sh [time_limit_per_case]
# ==============================================================================

# 1. Configuration
BENCH_DIR="./benchmarks/2022"
RESULT_DIR="./results/2022"
SCRIPT="./scripts/optimize.sh"
TIME_LIMIT="${1:-1800}"  # Default 1800s (30 mins) per case

# Define the specific test case numbers
CASES=( {20..25} {40..44} {90..99} )

# 2. Setup
mkdir -p "$RESULT_DIR"

if [ ! -x "$SCRIPT" ]; then
    echo "[Error] Optimization script not found or not executable: $SCRIPT"
    echo "       Please run: chmod +x $SCRIPT"
    exit 1
fi

echo "=========================================================="
echo "Starting Batch Run"
echo "Benchmarks: $BENCH_DIR"
echo "Results:    $RESULT_DIR"
echo "Time Limit: ${TIME_LIMIT}s per case"
echo "Cases:      ${CASES[*]}"
echo "=========================================================="

# 3. Main Loop
for ID in "${CASES[@]}"; do
    # Format ID to 2 digits with leading zero (e.g., 1 -> 01)
    CASE_ID=$(printf "%02d" "$ID")
    
    INPUT_FILE="${BENCH_DIR}/ex${CASE_ID}.truth"
    OUTPUT_FILE="${RESULT_DIR}/ex${CASE_ID}.aig"
    LOG_FILE="${RESULT_DIR}/ex${CASE_ID}.log"

    # Check if input exists
    if [ ! -f "$INPUT_FILE" ]; then
        echo "[Skip] Input file not found: $INPUT_FILE"
        continue
    fi

    # Check if output already exists (Skip if done)
    if [ -f "$OUTPUT_FILE" ]; then
        echo "[Skip] Result already exists for ex${CASE_ID}"
        continue
    fi

    echo ""
    echo ">>> Processing ex${CASE_ID} ..."
    
    # Run the pipeline and capture log
    # We use '2>&1' to capture both standard output and errors in the log
    $SCRIPT "$INPUT_FILE" "$OUTPUT_FILE" "$TIME_LIMIT" > "$LOG_FILE" 2>&1
    
    if [ $? -eq 0 ]; then
        echo "[Done] ex${CASE_ID} finished successfully."
    else
        echo "[Fail] ex${CASE_ID} encountered an error. Check log: $LOG_FILE"
    fi
done

echo ""
echo "=========================================================="
echo "Batch Run Complete."