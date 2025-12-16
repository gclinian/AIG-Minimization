#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <chrono>

// ABC Headers
#include "base/abc/abc.h"
#include "base/main/main.h"

// =========================================================
// FUNCTION DECLARATIONS (Updated Signatures)
// =========================================================

int run_abc_optimization(std::string inputTruthFile, std::string outputAigFile);
int run_eslim_optimization(std::string inputAigFile, std::string outputAigFile, int timeLimit);
void copy_file(std::string srcFilename, std::string dstFilename);
int run_iterative_eslim(std::string inputFile, std::string outputFile, int totalTimeLimit, int iterTimeLimit);
int get_gate_count(std::string filename);

// =========================================================
// MAIN ORCHESTRATOR
// =========================================================

int main(int argc, char * argv[]) {
    // 1. Basic Usage Check
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_aig> [options]" << std::endl;
        std::cerr << "Options (key=value):" << std::endl;
        std::cerr << "  time_limit=<int>   Total runtime budget in seconds (Default: 300)" << std::endl;
        std::cerr << "  iter_time=<int>    Max runtime per optimization step (Default: 60)" << std::endl;
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    
    // 2. Default Configuration
    int totalTimeLimit = 300; 
    int iterTimeLimit = 60;   

    // 3. Flexible Argument Parsing
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("time_limit=") == 0) {
            try {
                totalTimeLimit = std::stoi(arg.substr(11));
            } catch (...) { std::cerr << "[Warn] Invalid time_limit ignored.\n"; }
        }
        else if (arg.find("iter_time=") == 0) { // Renamed from chunk_size
            try {
                iterTimeLimit = std::stoi(arg.substr(10));
            } catch (...) { std::cerr << "[Warn] Invalid iter_time ignored.\n"; }
        }
        else {
            std::cerr << "[Warn] Unknown argument: " << arg << std::endl;
        }
    }

    std::cout << "[Config] Total Limit: " << totalTimeLimit << "s | Iteration Limit: " << iterTimeLimit << "s" << std::endl;

    // 4. Detect File Extension & Execute
    std::string ext = "";
    size_t dot = inputFile.find_last_of(".");
    if (dot != std::string::npos) ext = inputFile.substr(dot);

    if (ext == ".truth") {
        std::cout << "[Main] Detected .truth file. Starting ABC Synthesis..." << std::endl;
        std::string tempAbcOutput = outputFile + ".abc_tmp.aig";

        if (run_abc_optimization(inputFile, tempAbcOutput) != 0) {
            std::cerr << "[Error] ABC Synthesis failed." << std::endl;
            return 1;
        }

        std::cout << "[Main] Starting eSLIM Iterative Minimization..." << std::endl;
        int res = run_iterative_eslim(tempAbcOutput, outputFile, totalTimeLimit, iterTimeLimit);
        
        if (res != 0) copy_file(tempAbcOutput, outputFile); // Fallback
        std::remove(tempAbcOutput.c_str());
    } 
    else if (ext == ".aig") {
        std::cout << "[Main] Detected .aig file. Starting eSLIM Iterative Minimization..." << std::endl;
        int res = run_iterative_eslim(inputFile, outputFile, totalTimeLimit, iterTimeLimit);
        
        if (res != 0) copy_file(inputFile, outputFile); // Fallback
    } 
    else {
        std::cerr << "[Error] Unknown file extension: " << ext << std::endl;
        return 1;
    }

    return 0;
}

// =========================================================
// IMPLEMENTATIONS
// =========================================================

int run_abc_optimization(std::string inputTruthFile, std::string outputAigFile) {
    std::cout << "[ABC] Starting Optimization..." << std::endl;

    Abc_Start();
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();

    std::ifstream infile(inputTruthFile);
    if (!infile.is_open()) {
        std::cerr << "[ABC] Error: Could not open file " << inputTruthFile << std::endl;
        Abc_Stop(); return 1;
    }

    std::vector<std::string> functions;
    std::string line;
    while (std::getline(infile, line)) {
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        if (!line.empty()) functions.push_back(line);
    }
    infile.close();

    if (functions.empty()) {
        std::cerr << "[ABC] Warning: No valid truth tables found." << std::endl;
        Abc_Stop(); return 1;
    }

    // Construct Network
    Abc_Ntk_t * pNtk = Abc_NtkAlloc( ABC_NTK_STRASH, ABC_FUNC_AIG, 1 );
    pNtk->pName = Extra_UtilStrsav( "multi_output_solution" );

    int len = functions[0].length();
    int numInputs = static_cast<int>(std::log2(len));
    std::cout << "[ABC] Constructing network: " << numInputs << " inputs, " << functions.size() << " outputs." << std::endl;

    for (int i = 0; i < numInputs; i++) {
        char name[10];
        sprintf(name, "%c", 'a' + i);
        Abc_NtkCreatePi( pNtk );
        Abc_ObjAssignName( Abc_NtkPi(pNtk, i), name, NULL );
    }

    for (size_t fIdx = 0; fIdx < functions.size(); fIdx++) {
        std::string& truthBin = functions[fIdx];
        std::reverse(truthBin.begin(), truthBin.end()); 

        Abc_Obj_t * pTotalNand = Abc_AigConst1(pNtk);
        bool hasMinterms = false;

        for (int i = 0; i < len; i++) {
            if (truthBin[i] == '1') {
                hasMinterms = true;
                Abc_Obj_t * pTermAnd = Abc_AigConst1(pNtk);
                for (int v = 0; v < numInputs; v++) {
                    int bitVal = (i >> v) & 1;
                    Abc_Obj_t * pPi = Abc_NtkPi(pNtk, v);
                    Abc_Obj_t * pSignal = (bitVal == 0) ? Abc_ObjNot(pPi) : pPi;
                    pTermAnd = Abc_AigAnd( (Abc_Aig_t*)pNtk->pManFunc, pTermAnd, pSignal );
                }
                pTotalNand = Abc_AigAnd( (Abc_Aig_t*)pNtk->pManFunc, pTotalNand, Abc_ObjNot(pTermAnd) );
            }
        }

        Abc_Obj_t * pFinalNode;
        if (hasMinterms) pFinalNode = Abc_ObjNot(pTotalNand);
        else             pFinalNode = Abc_ObjNot(Abc_AigConst1(pNtk));

        Abc_Obj_t * pPo = Abc_NtkCreatePo( pNtk );
        Abc_ObjAddFanin( pPo, pFinalNode );
        
        char outName[30];
        if (functions.size() == 1) sprintf(outName, "F0");
        else sprintf(outName, "f%lu", fIdx);
        Abc_ObjAssignName( pPo, outName, NULL );
    }

    Abc_FrameReplaceCurrentNetwork(pAbc, pNtk);

    // Standard high-effort optimization script (resyn2)
    Cmd_CommandExecute(pAbc, "strash");
    Cmd_CommandExecute(pAbc, "balance");
    Cmd_CommandExecute(pAbc, "rewrite -l");
    Cmd_CommandExecute(pAbc, "balance");
    Cmd_CommandExecute(pAbc, "rewrite -lz");
    Cmd_CommandExecute(pAbc, "balance");
    Cmd_CommandExecute(pAbc, "strash");
    
    std::string cmdWrite = "write_aiger " + outputAigFile;
    int res = Cmd_CommandExecute(pAbc, cmdWrite.c_str());

    if (res == 0) std::cout << "[ABC] Optimization successful." << std::endl;
    else          std::cerr << "[ABC] Error writing AIGER file." << std::endl;

    Abc_Stop();
    return res;
}

int run_eslim_optimization(std::string inputFile, std::string outputFile, int timeLimit) {
    // 1. Paths relative to project root
    // We use the Python interpreter inside the .venv created by 'make eslim'
    std::string pythonExe = ".venv/bin/python3";
    std::string scriptPath = "third_party/eslim/src/reduce.py";
    // We need to add the source dir to PYTHONPATH so it finds the 'bindings' module
    std::string bindingsPath = "third_party/eslim/src"; 
    
    // 2. Sanity Check: Does venv exist?
    std::ifstream f(pythonExe.c_str());
    if (!f.good()) {
        std::cerr << "[C++] Error: Python venv not found at " << pythonExe << std::endl;
        std::cerr << "[C++] Please run 'make eslim' to setup the environment." << std::endl;
        return 1;
    }

    // 3. Construct the Command
    // Syntax: PYTHONPATH=... .venv/bin/python3 script.py [args]
    // This sets the environment variable ONLY for this command execution.
    std::stringstream ss;
    ss << "PYTHONPATH=$PYTHONPATH:" << bindingsPath << " "
       << pythonExe << " " << scriptPath << " "
       << inputFile << " " << outputFile << " " << timeLimit 
       << " --aig --aig-out " << outputFile << " --gs 2 --syn-mode sat";

    std::string command = ss.str();
    std::cout << "[C++] Executing eSLIM: " << command << std::endl;

    // 4. Execute
    int result = std::system(command.c_str());

    if (result != 0) {
        std::cerr << "[C++] eSLIM optimization failed (return code " << result << ")." << std::endl;
        return 1;
    }
    
    std::cout << "[C++] eSLIM optimization complete. Saved to " << outputFile << std::endl;
    return 0;
}

void copy_file(std::string srcFilename, std::string dstFilename) {
    std::ifstream src(srcFilename, std::ios::binary);
    std::ofstream dst(dstFilename, std::ios::binary);
    if (src && dst) {
        dst << src.rdbuf();
        std::cout << "[Fallback] Copied " << srcFilename << " to " << dstFilename << std::endl;
    } else {
        std::cerr << "[Fallback] Error: Could not copy file." << std::endl;
    }
}

int get_gate_count(std::string filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return -1;
    
    std::string word;
    // Header format: aig M I L O A (A is the 5th number)
    if (file >> word && word == "aig") {
        int m, i, l, o, a;
        if (file >> m >> i >> l >> o >> a) {
            return a;
        }
    }
    return -1; // Parse error
}

int run_iterative_eslim(std::string inputFile, std::string outputFile, int totalTimeLimit, int iterTimeLimit) {
    std::cout << "[Iterative] Starting loop. Total Budget: " << totalTimeLimit 
              << "s, Step Budget: " << iterTimeLimit << "s" << std::endl;
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Initialize: Copy input to output as the "Best So Far"
    copy_file(inputFile, outputFile);
    
    int bestCost = get_gate_count(outputFile);
    if (bestCost == -1) {
        std::cerr << "[Iterative] Error: Could not read input AIG size." << std::endl;
        return 1;
    }
    std::cout << "[Iterative] Initial Size: " << bestCost << " AND gates." << std::endl;

    std::string tempIterOutput = outputFile + ".iter_tmp.aig";
    int iteration = 1;

    while (true) {
        // Check remaining time
        auto now = std::chrono::steady_clock::now();
        int elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        int remaining = totalTimeLimit - elapsed;

        if (remaining <= 5) { // 5s buffer for safety
            std::cout << "[Iterative] Total time limit reached." << std::endl;
            break;
        }

        // Determine budget for this specific run
        // If remaining time is less than iterTimeLimit, use whatever is left
        int currentLimit = (remaining < iterTimeLimit) ? remaining : iterTimeLimit;

        std::cout << "[Iterative] Iteration " << iteration << " (Limit: " << currentLimit << "s)..." << std::endl;

        int res = run_eslim_optimization(outputFile, tempIterOutput, currentLimit);
        
        if (res != 0) {
            std::cerr << "[Iterative] eSLIM run failed or timed out hard. Stopping." << std::endl;
            break;
        }

        int newCost = get_gate_count(tempIterOutput);
        
        if (newCost != -1) {
            std::cout << "[Iterative] Size change: " << bestCost << " -> " << newCost << std::endl;

            if (newCost < bestCost) {
                std::cout << "[Iterative] Improvement found! Updating best result." << std::endl;
                bestCost = newCost;
                copy_file(tempIterOutput, outputFile);
                iteration++;
            } else {
                std::cout << "[Iterative] No improvement (Converged). Stopping." << std::endl;
                break;
            }
        } else {
            std::cerr << "[Iterative] Error reading result size." << std::endl;
            break;
        }
    }

    std::remove(tempIterOutput.c_str());
    std::cout << "[Iterative] Final Result: " << bestCost << " AND gates." << std::endl;
    return 0;
}