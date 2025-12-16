#!/bin/bash

# ==============================================================================
# IWLS 2025 Optimization Pipeline (Parallel Safe)
# Usage: ./optimize.sh <input_file> <output_aig> [total_time_seconds]
# ==============================================================================

# --- 1. Configuration ---

# Get absolute path of the project root (where this script is launched from)
PROJECT_ROOT=$(pwd)

INPUT_FILE="$1"
OUTPUT_FILE="$2"
TOTAL_BUDGET_SEC="${3:-3600}"

# Tool Paths (Absolute)
TOOL_ESLIM="$PROJECT_ROOT/bin/eslim/main"
TOOL_SIMPLIFIER="$PROJECT_ROOT/bin/simplifier/main"
TOOL_TEAMMATE_B="$PROJECT_ROOT/bin/teammate_b/optimizer"
CHECKER_SCRIPT="$PROJECT_ROOT/scripts/check_aig.sh"

# eSlim Config
ITER_TIME=600

# --- 2. Sandbox Setup ---

if [ -z "$INPUT_FILE" ] || [ -z "$OUTPUT_FILE" ]; then
    echo "Usage: $0 <input_file> <output_file> [time_limit_sec]"
    exit 1
fi

# Create a visible temp directory in the project root
mkdir -p "$PROJECT_ROOT/temp"
# Create a unique subfolder for THIS specific run
WORK_DIR=$(mktemp -d -p "$PROJECT_ROOT/temp" run_XXXXXX)

# Ensure we cleanup on exit
function cleanup {
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT SIGINT SIGTERM

# Timer Setup
START_TIME=$(date +%s)
END_TIME=$((START_TIME + TOTAL_BUDGET_SEC))

# --- 3. Helper Functions ---

function get_remaining_time {
    local now=$(date +%s)
    echo $((END_TIME - now))
}

function finalize_and_exit {
    echo ""
    # If successful, copy result back to the user's requested location
    if [ -f "$WORK_DIR/current_best.aig" ]; then
        cp "$WORK_DIR/current_best.aig" "$OUTPUT_FILE"
        echo "[System] Saved best result to: $OUTPUT_FILE"
    else 
        echo "[System] No result generated."
    fi
    exit 0 # trap will handle cleanup
}

function run_tool_step {
    local tool_path="$1"
    local step_name="$2"
    local use_timeout_cmd="$3"
    local extra_args="$4"
    
    local rem_time=$(get_remaining_time)
    
    if [ "$rem_time" -le 0 ]; then
        echo "[Timeout] Time limit reached before $step_name."
        finalize_and_exit
    fi

    if [ -f "$tool_path" ]; then
        echo "[$step_name] Running... (Max: ${rem_time}s)"
        
        # All tools now operate strictly inside WORK_DIR
        # Because we fixed C++ main.cpp, it will create its temp files inside WORK_DIR too
        if [ "$use_timeout_cmd" == "yes" ]; then
            timeout "$rem_time" "$tool_path" "$WORK_DIR/current_best.aig" "$WORK_DIR/temp_next.aig" $extra_args
        else
            "$tool_path" "$WORK_DIR/current_best.aig" "$WORK_DIR/temp_next.aig" "time_limit=$rem_time" $extra_args
        fi

        # Verify Result
        if [ -f "$WORK_DIR/temp_next.aig" ]; then
            "$CHECKER_SCRIPT" "$WORK_DIR/golden.aig" "$WORK_DIR/temp_next.aig"
            if [ $? -eq 0 ]; then
                echo "   [Pass] Verified."
                mv "$WORK_DIR/temp_next.aig" "$WORK_DIR/current_best.aig"
            else
                echo "   [FAIL] Verification Failed. Discarding result."
                rm "$WORK_DIR/temp_next.aig"
            fi
        fi
    else
        echo "[$step_name] Binary missing (skipping)..."
    fi
}

# ==============================================================================
# PHASE 1: INITIAL PASS
# ==============================================================================

echo "Phase 1: Initialization (Sandbox: $WORK_DIR)"

# 1. Establish Golden Reference inside Sandbox
# We use absolute paths for 'cp' to be safe
if [[ "$INPUT_FILE" == *.aig ]]; then
    # If input path is relative, make it relative to PROJECT_ROOT
    if [[ "$INPUT_FILE" != /* ]]; then
        cp "$PROJECT_ROOT/$INPUT_FILE" "$WORK_DIR/golden.aig"
    else
        cp "$INPUT_FILE" "$WORK_DIR/golden.aig"
    fi
fi

# 2. Check Time
REMAINING=$(get_remaining_time)
if [ "$REMAINING" -le 0 ]; then finalize_and_exit; fi

# 3. Run Initial Synthesis
# Handle input path resolution again
REAL_INPUT="$INPUT_FILE"
if [[ "$INPUT_FILE" != /* ]]; then REAL_INPUT="$PROJECT_ROOT/$INPUT_FILE"; fi

"$TOOL_ESLIM" "$REAL_INPUT" "$WORK_DIR/current_best.aig" "time_limit=$REMAINING" "iter_time=$ITER_TIME"

if [ ! -f "$WORK_DIR/current_best.aig" ]; then
    echo "[Error] Initial pass failed."
    exit 1
fi

# 4. Finalize Golden Reference
if [ ! -f "$WORK_DIR/golden.aig" ]; then
    cp "$WORK_DIR/current_best.aig" "$WORK_DIR/golden.aig"
else
    "$CHECKER_SCRIPT" "$WORK_DIR/golden.aig" "$WORK_DIR/current_best.aig"
    if [ $? -ne 0 ]; then
         echo "[Fatal] Initial pass corrupted the circuit! Reverting."
         cp "$WORK_DIR/golden.aig" "$WORK_DIR/current_best.aig"
    fi
fi

# ==============================================================================
# PHASE 2: OPTIMIZATION LOOP
# ==============================================================================

for i in {1..5}; do
    echo "--- Iteration $i / 5 ---"
    
    REMAINING=$(get_remaining_time)
    if [ "$REMAINING" -le 0 ]; then 
        echo "[Timeout] Global time limit reached."
        finalize_and_exit
    fi

    run_tool_step "$TOOL_SIMPLIFIER" "Step 1 (Simplifier)" "yes" ""
    run_tool_step "$TOOL_ESLIM"      "Step 2 (eSLIM)"      "no"  "iter_time=$ITER_TIME"
    run_tool_step "$TOOL_TEAMMATE_B" "Step 3 (Teammate B)" "yes" ""
done

echo "[Success] Optimization loop completed."
finalize_and_exit
