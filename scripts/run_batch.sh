#!/bin/bash

# ==============================================================================
# Parallel Batch Runner for IWLS 2025 Benchmarks
# Usage: ./run_batch.sh [num_threads] [time_limit_per_case]
# ==============================================================================

# 1. Configuration
export BENCH_DIR="./benchmarks/2022"
export RESULT_DIR="./results/2022"
export SCRIPT="./scripts/optimize.sh"

# Default Arguments
NUM_THREADS="${1:-4}"     # Default to 4 parallel jobs
export TIME_LIMIT="${2:-1800}" # Default to 600s per case

# Define cases to run (Space separated, ranges {N..M} allowed)
CASES=( {1..99} )

# 2. Setup
mkdir -p "$RESULT_DIR"

if [ ! -x "$SCRIPT" ]; then
    echo "[Error] Script not executable: $SCRIPT"
    exit 1
fi

echo "=========================================================="
echo "Starting Parallel Run"
echo "Threads:    $NUM_THREADS"
echo "Time Limit: ${TIME_LIMIT}s per case"
echo "Cases:      ${CASES[*]}"
echo "=========================================================="

# 3. Worker Function
# This function handles a single test case. We export it so xargs can see it.
function process_case() {
    local ID="$1"
    
    # Zero-pad the ID (e.g., 1 -> 01)
    local CASE_ID=$(printf "%02d" "$ID")
    
    local INPUT_FILE="${BENCH_DIR}/ex${CASE_ID}.truth"
    local OUTPUT_FILE="${RESULT_DIR}/ex${CASE_ID}.aig"
    local LOG_FILE="${RESULT_DIR}/ex${CASE_ID}.log"

    # Checks
    if [ ! -f "$INPUT_FILE" ]; then
        echo "[Skip] ex${CASE_ID}: Input not found."
        return
    fi

    if [ -f "$OUTPUT_FILE" ]; then
        echo "[Skip] ex${CASE_ID}: Result exists."
        return
    fi

    echo ">>> [Start] ex${CASE_ID} (Log: $LOG_FILE)"
    
    # Run the pipeline
    # Redirect BOTH stdout and stderr to the log file to prevent terminal clutter
    "$SCRIPT" "$INPUT_FILE" "$OUTPUT_FILE" "$TIME_LIMIT" > "$LOG_FILE" 2>&1
    
    local EXIT_CODE=$?
    
    if [ $EXIT_CODE -eq 0 ]; then
        echo ">>> [Done]  ex${CASE_ID} finished."
    else
        echo ">>> [Fail]  ex${CASE_ID} failed (Code: $EXIT_CODE). See $LOG_FILE"
    fi
}

# Export the function and variables so subshells can use them
export -f process_case

# 4. Parallel Execution
# We pipe the CASES array into xargs.
# -P: Number of parallel processes
# -I {}: Placeholder for the argument
# -n 1: Use 1 argument per command
printf "%s\n" "${CASES[@]}" | xargs -P "$NUM_THREADS" -I {} -n 1 bash -c 'process_case "{}"'

echo ""
echo "=========================================================="
echo "Parallel Batch Run Complete."