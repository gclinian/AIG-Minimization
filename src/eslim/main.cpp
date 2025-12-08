#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cstdlib> // Required for std::system

// ABC Headers (Ensure your Makefile includes -Ithird_party/abc/src)
#include "base/abc/abc.h"
#include "base/main/main.h"

// =========================================================
// FUNCTION DECLARATIONS
// =========================================================

// Optimizes using ABC (Internal Library)
int run_abc_optimization(std::string inputTruthFile, std::string outputAigFile);

// Optimizes using eSLIM (Calls Bash Wrapper)
int run_eslim_optimization(std::string inputAigFile, std::string outputAigFile);

// Fallback utility
void copy_file(std::string srcFilename, std::string dstFilename);

// =========================================================
// MAIN ORCHESTRATOR
// =========================================================

int main(int argc, char * argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <truth_file> <output_aig>" << std::endl;
        return 1;
    }
    std::string truthFile = argv[1];
    std::string finalOutputFile = argv[2];
    
    // Intermediate file
    std::string tempAbcOutput = "temp_abc_result.aig";

    // STEP 1: ABC Optimization
    if (run_abc_optimization(truthFile, tempAbcOutput) != 0) {
        std::cerr << "[Error] ABC Optimization step failed." << std::endl;
        return 1;
    }

    // STEP 2: eSLIM Optimization (via Bash Script)
    int eslimResult = run_eslim_optimization(tempAbcOutput, finalOutputFile);

    // STEP 3: Fallback Strategy
    if (eslimResult != 0) {
        std::cout << "[Main] eSLIM failed. Falling back to ABC result." << std::endl;
        copy_file(tempAbcOutput, finalOutputFile);
    }

    // Cleanup
    std::remove(tempAbcOutput.c_str());

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
        
        char outName[20];
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

int run_eslim_optimization(std::string inputFile, std::string outputFile) {
    // 1. Paths relative to project root
    // We use the Python interpreter inside the .venv created by 'make eslim'
    std::string pythonExe = ".venv/bin/python3";
    std::string scriptPath = "third_party/eslim/src/reduce.py";
    // We need to add the source dir to PYTHONPATH so it finds the 'bindings' module
    std::string bindingsPath = "third_party/eslim/src"; 
    int timeout = 300; 
    
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
       << inputFile << " " << outputFile << " " << timeout 
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