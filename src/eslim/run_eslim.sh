#!/bin/bash

# =========================================================
# CONFIGURATION (Relative to Project Root)
# =========================================================
VENV_DIR=".venv"
ESLIM_DIR="third_party/eslim"
ESLIM_SRC_DIR="$ESLIM_DIR/src"
ESLIM_SCRIPT="$ESLIM_SRC_DIR/reduce.py"
BINDINGS_DIR="$ESLIM_SRC_DIR/bindings"
BINDINGS_BUILD_DIR="$BINDINGS_DIR/build"

# =========================================================
# 1. SETUP VIRTUAL ENVIRONMENT
# =========================================================
if [ ! -d "$VENV_DIR" ]; then
    echo "[Bash] Creating Python virtual environment in $VENV_DIR..."
    python3 -m venv $VENV_DIR
fi

# Activate venv
source $VENV_DIR/bin/activate

# Install Dependencies
if ! python3 -c "import bitarray" &> /dev/null; then
    echo "[Bash] Installing required modules (bitarray, pybind11, cmake)..."
    pip install bitarray pybind11 cmake
fi

# Double check CMake install
if ! command -v cmake &> /dev/null; then
    echo "[Bash] CMake not found. Installing via pip..."
    pip install cmake
fi

# =========================================================
# 2. COMPILE C++ BINDINGS (With Fixes)
# =========================================================
# Check if the .so file exists (compilation success marker)
# We look for any .so file in the build directory
if [ -z "$(find "$BINDINGS_BUILD_DIR" -name "*.so" 2>/dev/null)" ]; then
    echo "[Bash] eSLIM bindings not found. Compiling now..."
    
    mkdir -p "$BINDINGS_BUILD_DIR"
    pushd "$BINDINGS_BUILD_DIR" > /dev/null
    
    # --- CRITICAL FIX ---
    # 1. Get the path to pybind11Config.cmake from the pip installation
    PYBIND11_CMAKE_DIR=$(python3 -m pybind11 --cmakedir)
    # 2. Get the specific python executable (in the venv)
    PYTHON_EXE=$(which python3)
    
    echo "[Bash] Configured pybind11 at: $PYBIND11_CMAKE_DIR"
    
    # Run CMake with explicit paths to ensure it finds the venv libraries
    if cmake .. \
        -Dpybind11_DIR="$PYBIND11_CMAKE_DIR" \
        -DPython3_EXECUTABLE="$PYTHON_EXE" \
        && make; then
        
        echo "[Bash] Compilation successful."
    else
        echo "[Bash] Error: Compilation failed."
        popd > /dev/null
        deactivate
        exit 1
    fi
    popd > /dev/null
    
    # Ensure Python treats these folders as packages
    touch "$BINDINGS_DIR/__init__.py"
    touch "$BINDINGS_BUILD_DIR/__init__.py"
fi

# =========================================================
# 3. CONFIGURE PATHS
# =========================================================
# Add bindings to PYTHONPATH so eSLIM can import them
export PYTHONPATH=$PYTHONPATH:$(pwd)/$ESLIM_DIR/src

if [ ! -f "$ESLIM_SCRIPT" ]; then
    echo "[Bash] Error: Could not find eSLIM script at $ESLIM_SCRIPT"
    echo "[Bash] Ensure you are running this from the project root."
    deactivate
    exit 1
fi

# =========================================================
# 4. EXECUTE SCRIPT
# =========================================================
echo "[Bash] Executing eSLIM optimization..."
python3 "$ESLIM_SCRIPT" "$@"
EXIT_CODE=$?

deactivate
exit $EXIT_CODE