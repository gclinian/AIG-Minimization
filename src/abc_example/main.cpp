#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "base/abc/abc.h"
#include "base/main/main.h"

// 輔助函式：將二進位字串轉換為十六進位字串
std::string BinToHex(const std::string& bin) {
    std::string hex = "";
    int len = bin.length();
    // 補齊長度為 4 的倍數
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
    // 1. 初始化 ABC 框架
    Abc_Start();
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();

    std::cout << "ABC is running..." << std::endl;

    // 2. 讀取檔案
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <truth_file>" << std::endl;
        Abc_Stop();
        return 1;
    }
    std::string filename = argv[1];
    std::ifstream infile(filename);
    
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        Abc_Stop();
        return 1;
    }

    // 取得檔名主體 (去除路徑和副檔名) 用於輸出
    std::string stem = filename;
    size_t lastSlash = stem.find_last_of("/\\");
    if (lastSlash != std::string::npos) stem = stem.substr(lastSlash + 1);
    size_t lastDot = stem.find_last_of(".");
    if (lastDot != std::string::npos) stem = stem.substr(0, lastDot);

    std::string line;
    int index = 0;

    while (std::getline(infile, line)) {
        // 移除空白字元
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        
        // 跳過空行
        if (line.empty()) continue;

        std::cout << "Processing function #" << index << " (Length: " << line.length() << ")..." << std::endl;

        // 3. 轉換為 Hex 字串
        std::string hexString = BinToHex(line);

        // 4. 執行 ABC 指令
        // 指令 1: read_truth
        std::string cmdRead = "read_truth " + hexString;
        if (Cmd_CommandExecute(pAbc, cmdRead.c_str())) {
            std::cerr << "Cannot execute command: " << cmdRead << std::endl;
            continue; // 失敗則跳過此行
        }

        // 指令 2: strash
        if (Cmd_CommandExecute(pAbc, "strash")) {
            std::cerr << "Cannot execute command: strash" << std::endl;
            continue;
        }

        // 指令 3: write_aig (為每個函數產生獨立的檔案)
        std::string outputFilename = "example/output/" + stem + "_" + std::to_string(index) + ".aig";
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

    // 5. 停止 ABC 框架
    Abc_Stop();
    return 0;
}