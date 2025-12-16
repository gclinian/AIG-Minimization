# AIG Minimization Project

This project uses [ABC](https://github.com/berkeley-abc/abc) for AIG manipulation and minimization.

## Setup & Installation

This repository contains `abc` as a submodule. You need to clone it recursively or initialize it after cloning.

### 1. Clone the Repository

**Option A: Clone with recursion (Recommended)**
```bash
git clone --recursive <REPO_URL>
```

**Option B: If you already cloned without recursion**
```bash
git submodule update --init --recursive
```

### 2. Build the Project

The project includes a root `Makefile` that manages the build process for both `abc` and your custom code.

Simply run:
```bash
make
```
This command will:
1.  Automatically build the `abc` library (`libabc.a`) if it's missing.
2.  Compile all `.cpp` files found in any subdirectory (excluding `abc/`).

## Project Structure

-   **`third_party/`**: Third-party dependencies.
    -   **`abc/`**: The ABC tool source code (submodule).
    -   **`eSlim/`**: The eSlim tool source code (submodule).
    -   **`sinplifier/`**: The simplifier source code (submodule).
-   **`bin/`**: All compiled executables will be placed here, mirroring the source directory structure.
-   **`benchmarks/`**: Truth table files and other benchmarks.
-   **`src/`**: Implemented AIG-Minimization by different method.
-   **`scripts/`**: Shell scripts for automated execution and equivalent checking.

## How to Add New Code

To add a new experiment or solver:

1.  **Create a new directory** (optional, or use an existing one):
    ```bash
    mkdir my_experiment
    ```

2.  **Create your C++ file**:
    e.g., `my_experiment/main.cpp`.
    
    > **Note**: You usually need both headers:
    > ```cpp
    > #include "base/abc/abc.h"
    > #include "base/main/main.h"
    > ```

3.  **Compile**:
    Run `make` in the root directory:
    ```bash
    make
    ```
    
    The `Makefile` automatically detects new `.cpp` files.
    
    Your executable will be generated at:
    `bin/my_experiment/main`

## Running the Code

Run the compiled executable from the `bin` directory:

```bash
./bin/example/main benchmarks/2025/ex00.truth
```

## Cleaning Up

To remove all compiled binaries and temporary files:

```bash
make clean
```
