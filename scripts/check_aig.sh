#!/bin/bash

# --- Configuration ---
# Path to the ABC directory
ABC_DIR="./third_party/abc"
# Path to the ABC binary
ABC_BIN="$ABC_DIR/abc"

# --- 1. Check and Build ABC (Preserved) ---
if [ ! -f "$ABC_BIN" ]; then
    echo "[*] ABC binary not found at $ABC_BIN"
    
    if [ ! -d "$ABC_DIR" ]; then
        echo "[!] Error: Directory $ABC_DIR does not exist."
        exit 1
    fi

    echo "[*] Attempting to build ABC..."
    
    # Run make inside the ABC directory
    # -C changes directory before running make
    # -j$(nproc) uses all available CPU cores for faster compilation
    make -C "$ABC_DIR" -j$(nproc)

    # Check if make succeeded
    if [ $? -ne 0 ]; then
        echo "[!] Build failed. Please check the compilation errors."
        exit 1
    fi
    
    echo "[*] Build successful."
else
    # Commented out to reduce noise in the main optimization loop
    # echo "[*] ABC binary found."
    : 
fi

# --- 2. Verify Arguments ---
if [ "$#" -ne 2 ]; then
    echo ""
    echo "Usage: $0 <file1.aig> <file2.aig>"
    echo "Example: $0 golden.aig revised.aig"
    exit 1
fi

FILE1=$1
FILE2=$2

# Check if input files exist
if [ ! -f "$FILE1" ]; then
    echo "[!] Error: File '$FILE1' not found."
    exit 1
fi

if [ ! -f "$FILE2" ]; then
    echo "[!] Error: File '$FILE2' not found."
    exit 1
fi

# --- 3. Run Equivalence Checking ---
# We capture the output silently so we can parse it.
# 2>&1 merges stderr into stdout so we catch everything.

OUTPUT=$("$ABC_BIN" -c "read $FILE1; cec $FILE2" 2>&1)

# Check for the specific success string that ABC prints
if echo "$OUTPUT" | grep -q "Networks are equivalent"; then
    # Success: Return 0
    echo "[Check] Equivalent."
    exit 0
else
    # Failure: Return 1 and print what went wrong
    echo "[Check] Verification FAILED!"
    echo "--- ABC Output ---"
    echo "$OUTPUT"
    echo "------------------"
    exit 1
fi