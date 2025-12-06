#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

// ABC Headers
#include "base/abc/abc.h"
#include "base/main/main.h"

// Helper: Convert Binary string to Hex string
std::string BinToHex(const std::string& bin) {
    std::string hex = "";
    int len = bin.length();
    // Pad to multiple of 4
    int pad = (4 - (len % 4)) % 4;
    std::string paddedBin = std::string(pad, '0') + bin;
    
    for (size_t i = 0; i < paddedBin.length(); i += 4) {
        std::string chunk = paddedBin.substr(i, 4);
        int val = std::stoi(chunk, nullptr, 2);
        std::stringstream ss;
        ss << std::hex << val;
        hex += ss.str();
    }
    return hex;
}

int main(int argc, char * argv[]) {
    // 1. Initialize ABC
    Abc_Start();
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();

    std::cout << "ABC is running..." << std::endl;

    // 2. Check arguments (Modified to require 3 arguments)
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <truth_file> <output_base_name>" << std::endl;
        std::cerr << "Example: " << argv[0] << " input.truth my_results/circuit" << std::endl;
        Abc_Stop();
        return 1;
    }

    std::string filename = argv[1];
    std::string outputBase = argv[2]; // Store the output argument
    std::ifstream infile(filename);
    
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        Abc_Stop();
        return 1;
    }

    // (Removed automatic stem extraction logic as output name is now manual)

    std::string line;
    int index = 0;

    // 3. Process each line
    while (std::getline(infile, line)) {
        // Clean line (remove whitespace)
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        
        // Skip empty lines
        if (line.empty()) continue;

        std::cout << "Processing function #" << index << "..." << std::endl;

        std::string hexString = BinToHex(line);

        // --- COMMAND SEQUENCE START ---

        // Command 1: read_truth
        std::string cmdRead = "read_truth " + hexString;
        if (Cmd_CommandExecute(pAbc, cmdRead.c_str())) {
            std::cerr << "Cannot execute command: " << cmdRead << std::endl;
            continue; 
        }

        // Command 2: sop (Use Espresso Internally)
        if (Cmd_CommandExecute(pAbc, "sop")) {
             std::cerr << "Cannot execute command: sop" << std::endl;
             continue;
        }

        // Command 3: strash
        if (Cmd_CommandExecute(pAbc, "strash")) {
             std::cerr << "Cannot execute command: strash" << std::endl;
             continue;
        }
        
        // --- COMMAND SEQUENCE END ---

        // Command 4: write_aiger
        // Construct filename using the User Provided Argument + Index + Extension
        std::string outputFilename = outputBase + "_" + std::to_string(index) + ".aig";
        
        std::string cmdWrite = "write_aiger " + outputFilename;
        if (Cmd_CommandExecute(pAbc, cmdWrite.c_str())) {
            std::cerr << "Cannot execute command: " << cmdWrite << std::endl;
            continue;
        }

        std::cout << "Successfully wrote to " << outputFilename << std::endl;
        index++;
    }
    
    infile.close();

    if (index == 0) {
        std::cerr << "Warning: No valid truth tables found in file." << std::endl;
    }

    // 4. Stop ABC
    Abc_Stop();
    return 0;
}