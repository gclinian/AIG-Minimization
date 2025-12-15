#!/bin/bash

# ==============================================================================
# IWLS 2025 Optimization Pipeline (Fixed Timer Logic)
# Usage: ./optimize.sh <input_file> <output_aig> [total_time_seconds]
# ==============================================================================

# --- 1. Configuration ---

INPUT_FILE="$1"
OUTPUT_FILE="$2"
TOTAL_BUDGET_SEC="${3:-3600}"

# Tool Paths
TOOL_ESLIM="./bin/eslim/main"
TOOL_SIMPLIFIER="./bin/simplifier/main"
TOOL_TEAMMATE_B="./bin/teammate_b/optimizer"
CHECKER_SCRIPT="./scripts/check_aig.sh"

# eSlim Config
ITER_TIME=180

# --- 2. Validation & Setup ---

if [ -z "$INPUT_FILE" ] || [ -z "$OUTPUT_FILE" ]; then
    echo "Usage: $0 <input_file> <output_file> [time_limit_sec]"
    exit 1
fi

if [ ! -x "$CHECKER_SCRIPT" ]; then
    echo "[Error] Checker script not executable: $CHECKER_SCRIPT"
    echo "        Run: chmod +x $CHECKER_SCRIPT"
    exit 1
fi

# Timer Setup
START_TIME=$(date +%s)
END_TIME=$((START_TIME + TOTAL_BUDGET_SEC))

# --- 3. Helper Functions ---

# FIXED: Purely calculates time. Does NOT exit.
function get_remaining_time {
    local now=$(date +%s)
    echo $((END_TIME - now))
}

function finalize_and_exit {
    echo ""
    if [ -f "current_best.aig" ]; then
        cp "current_best.aig" "$OUTPUT_FILE"
        echo "[System] Saved best result to: $OUTPUT_FILE"
    else 
        echo "[System] No result generated."
    fi
    rm -f "current_best.aig" "temp_next.aig" "golden.aig" "cec.log"
    exit 0
}

function run_tool_step {
    local tool_path="$1"
    local step_name="$2"
    local use_timeout_cmd="$3"
    local extra_args="$4"
    
    # 1. Check Time BEFORE running
    local rem_time=$(get_remaining_time)
    
    if [ "$rem_time" -le 0 ]; then
        echo "[Timeout] Time limit reached before $step_name."
        finalize_and_exit
    fi

    if [ -f "$tool_path" ]; then
        echo "[$step_name] Running... (Max: ${rem_time}s)"
        
        # Execute Tool
        if [ "$use_timeout_cmd" == "yes" ]; then
            timeout "$rem_time" $tool_path "current_best.aig" "temp_next.aig" $extra_args
        else
            $tool_path "current_best.aig" "temp_next.aig" "time_limit=$rem_time" $extra_args
        fi

        # Verify Result
        if [ -f "temp_next.aig" ]; then
            $CHECKER_SCRIPT "golden.aig" "temp_next.aig"
            if [ $? -eq 0 ]; then
                echo "   [Pass] Verified."
                mv "temp_next.aig" "current_best.aig"
            else
                echo "   [FAIL] Verification Failed. Discarding result."
                rm "temp_next.aig"
            fi
        fi
    else
        echo "[$step_name] Binary missing (skipping)..."
    fi
}

trap finalize_and_exit SIGINT SIGTERM

# ==============================================================================
# PHASE 1: INITIAL PASS
# ==============================================================================

echo "Phase 1: Initialization"

# 1. Establish Golden Reference
if [[ "$INPUT_FILE" == *.aig ]]; then
    cp "$INPUT_FILE" "golden.aig"
fi

# 2. Check Time
REMAINING=$(get_remaining_time)
if [ "$REMAINING" -le 0 ]; then finalize_and_exit; fi

# 3. Run Initial Synthesis
# Note: If time runs out HERE, eSLIM will handle it internally via time_limit
$TOOL_ESLIM "$INPUT_FILE" "current_best.aig" "time_limit=$REMAINING" "iter_time=$ITER_TIME"

if [ ! -f "current_best.aig" ]; then
    echo "[Error] Initial pass failed."
    exit 1
fi

# 4. Finalize Golden Reference
if [ ! -f "golden.aig" ]; then
    cp "current_best.aig" "golden.aig"
else
    $CHECKER_SCRIPT "golden.aig" "current_best.aig"
    if [ $? -ne 0 ]; then
         echo "[Fatal] Initial pass corrupted the circuit! Reverting."
         cp "golden.aig" "current_best.aig"
    fi
fi

# ==============================================================================
# PHASE 2: OPTIMIZATION LOOP
# ==============================================================================

for i in {1..5}; do
    echo "--- Iteration $i / 5 ---"
    
    # Check global time at start of iteration
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