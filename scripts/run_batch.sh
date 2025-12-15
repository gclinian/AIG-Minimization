#!/bin/bash

# ==============================================================================
# Batch Runner for IWLS 2025 Benchmarks
# Usage: ./run_batch.sh [time_limit_per_case]
# ==============================================================================

# 1. Configuration
BENCH_DIR="./benchmarks/2022"
RESULT_DIR="./results/2022"
SCRIPT="./scripts/optimize.sh"
TIME_LIMIT="${1:-600}"  # Default 600s (10 mins) per case if not specified

# Define the specific test case numbers you want to run
# Syntax: Numbers separated by spaces. Ranges like {1..10} expand automatically.
CASES=( {1..10} 20 30 40 50 60 70 80 90 99 )

# 2. Setup
# Ensure result directory exists
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
    INPUT_FILE="${BENCH_DIR}/ex${ID}.truth"
    OUTPUT_FILE="${RESULT_DIR}/ex${ID}.aig"

    # Check if input exists
    if [ ! -f "$INPUT_FILE" ]; then
        echo "[Skip] Input file not found: $INPUT_FILE"
        continue
    fi

    # Check if output already exists (Skip if done)
    if [ -f "$OUTPUT_FILE" ]; then
        echo "[Skip] Result already exists for ex${ID}"
        continue
    fi

    echo ""
    echo ">>> Processing ex${ID} ..."
    
    # Run the optimization pipeline
    # We redirect stdout to a log file to keep the terminal clean, 
    # but print errors to stderr.
    LOG_FILE="${RESULT_DIR}/ex${ID}.log"
    
    $SCRIPT "$INPUT_FILE" "$OUTPUT_FILE" "$TIME_LIMIT" > "$LOG_FILE" 2>&1
    
    # Check exit code of the script
    if [ $? -eq 0 ]; then
        echo "[Done] ex${ID} finished successfully."
    else
        echo "[Fail] ex${ID} encountered an error. Check log: $LOG_FILE"
    fi
done

echo ""
echo "=========================================================="
echo "Batch Run Complete."