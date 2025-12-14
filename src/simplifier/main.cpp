#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <map>
#include <algorithm>

namespace fs = std::filesystem;

// ================= 路徑設定 =================
const std::string ABC_PATH = "./third_party/abc/abc"; 
const std::string SIMPLIFIER_EXEC = "./third_party/simplifier/build/simplifier"; 
const std::string SIMPLIFIER_DB = "./third_party/simplifier/databases";

// ================= 輔助工具 =================

void run_command(const std::string& cmd) {
    // std::cout << "[CMD] " << cmd << std::endl;
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Error: Command failed!" << std::endl;
        exit(1);
    }
}

// ================= Binary AIGER 解析器 =================

unsigned decode_vli(std::ifstream& infile) {
    unsigned x = 0, i = 0;
    unsigned char b;
    while (infile.read(reinterpret_cast<char*>(&b), 1)) {
        x |= (b & 0x7f) << (7 * i);
        i++;
        if (!(b & 0x80)) break;
    }
    return x;
}

// AIG 轉 BENCH 轉換器
void aig_binary_to_bench(const std::string& aig_file, const std::string& bench_file) {
    std::ifstream infile(aig_file, std::ios::binary);
    if (!infile.is_open()) {
        std::cerr << "Error: Cannot open AIG file: " << aig_file << std::endl;
        exit(1);
    }

    std::ofstream outfile(bench_file);
    std::string line, type;
    int M, I, L, O, A;

    // 1. Header
    while (std::getline(infile, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::stringstream ss(line);
        ss >> type;
        if (type == "aig") {
            ss >> M >> I >> L >> O >> A;
            break; 
        } else {
            std::cerr << "Error: Expected 'aig' header, found: " << line << std::endl;
            exit(1);
        }
    }

    std::map<int, std::string> node_names;

    auto get_signal_name = [&](int literal) -> std::string {
        if (literal == 0) return "GND";
        if (literal == 1) return "VCC";
        
        int var_idx = literal >> 1;
        bool is_inv = (literal & 1);
        std::string base_name = "n" + std::to_string(var_idx);
        
        if (!is_inv) {
            return base_name;
        } else {
            std::string inv_name = "inv_n" + std::to_string(var_idx);
            if (node_names.find(literal) == node_names.end()) {
                outfile << inv_name << " = NOT(" << base_name << ")" << std::endl;
                node_names[literal] = inv_name;
            }
            return inv_name;
        }
    };

    outfile << "# Converted from Binary AIGER" << std::endl;
    outfile << "INPUT(GND)\nINPUT(VCC)" << std::endl;

    // 2. Inputs
    for (int i = 0; i < I; ++i) {
        int lit = 2 * (i + 1);
        std::string name = "n" + std::to_string(i + 1);
        outfile << "INPUT(" << name << ")" << std::endl;
        node_names[lit] = name;
    }

    // 3. Latches
    int latches_read = 0;
    while (latches_read < L) {
        if (!std::getline(infile, line)) break;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        latches_read++;
    }

    // 4. Outputs
    std::vector<int> output_literals;
    int outputs_read = 0;
    while (outputs_read < O) {
        if (!std::getline(infile, line)) break;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue; 
        try {
            output_literals.push_back(std::stoi(line));
            outputs_read++;
        } catch (...) {}
    }

    // 5. Gates
    int current_idx = I + L + 1;
    for (int i = 0; i < A; ++i) {
        int lhs = 2 * current_idx;
        unsigned delta0 = decode_vli(infile);
        unsigned delta1 = decode_vli(infile);
        int rhs0 = lhs - delta0;
        int rhs1 = rhs0 - delta1;

        std::string name_lhs = "n" + std::to_string(current_idx);
        std::string name_rhs0 = get_signal_name(rhs0);
        std::string name_rhs1 = get_signal_name(rhs1);

        outfile << name_lhs << " = AND(" << name_rhs0 << ", " << name_rhs1 << ")" << std::endl;
        node_names[lhs] = name_lhs;
        current_idx++;
    }

    // 6. Output Pins (修改重點：拆成兩個 NOT)
    for (size_t i = 0; i < output_literals.size(); ++i) {
        int lit = output_literals[i];
        std::string out_port_name = "po" + std::to_string(i);
        outfile << "OUTPUT(" << out_port_name << ")" << std::endl;
        
        std::string internal_sig = get_signal_name(lit);
        
        // 為了符合 BENCH 語法 (Gate 只能有一層)，我們把 Buffer 拆成兩個 NOT
        std::string temp_inv_sig = "tmp_inv_" + out_port_name;
        
        // 第一層：tmp = NOT(internal)
        outfile << temp_inv_sig << " = NOT(" << internal_sig << ")" << std::endl;
        
        // 第二層：output = NOT(tmp)
        outfile << out_port_name << " = NOT(" << temp_inv_sig << ")" << std::endl; 
    }
    
    infile.close();
    outfile.close();
}


// ================= 主程式 =================

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <input.aig> <output.aig>" << std::endl;
        return 1;
    }

    std::string input_aig = argv[1];
    std::string output_aig = argv[2]; 
    
    // Check 1: 輸入檔案是否存在？
    if (!fs::exists(input_aig)) {
        std::cerr << "Error: Input file does not exist: " << input_aig << std::endl;
        return 1;
    }

    // Check 2: 自動建立輸出路徑的父目錄
    fs::path out_path(output_aig);
    if (out_path.has_parent_path()) {
        fs::path parent = out_path.parent_path();
        if (!fs::exists(parent)) {
            try {
                fs::create_directories(parent);
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Error: Could not create output directory " << parent << ": " << e.what() << std::endl;
                return 1;
            }
        }
    }

    std::string temp_id = "tmp_sim_" + std::to_string(std::rand() % 10000);
    std::string temp_aig_raw = temp_id + "_raw.aig"; 
    std::string temp_bench_clean = temp_id + ".bench"; 
    std::string dir_in = temp_id + "_in";
    std::string dir_out = temp_id + "_out";

    if (!fs::exists(ABC_PATH)) { std::cerr << "Missing ABC at " << ABC_PATH << std::endl; return 1; }
    if (!fs::exists(SIMPLIFIER_EXEC)) { std::cerr << "Missing Simplifier at " << SIMPLIFIER_EXEC << std::endl; return 1; }

    // 1. Normalize
    std::string cmd_norm = ABC_PATH + " -c \"read_aiger " + input_aig + "; strash; write_aiger " + temp_aig_raw + "\" > /dev/null 2>&1";
    run_command(cmd_norm);

    // 2. To Bench
    aig_binary_to_bench(temp_aig_raw, temp_bench_clean);

    // 3. Run Simplifier
    if (fs::exists(dir_in)) fs::remove_all(dir_in);
    if (fs::exists(dir_out)) fs::remove_all(dir_out);
    fs::create_directory(dir_in);
    fs::create_directory(dir_out);

    fs::rename(temp_bench_clean, dir_in + "/" + temp_bench_clean);

    // 拿掉 /dev/null 以便除錯
    std::string sim_cmd = SIMPLIFIER_EXEC + " -i " + dir_in + " -o " + dir_out + " --basis BENCH --databases " + SIMPLIFIER_DB;
    
    int ret = std::system(sim_cmd.c_str());
    
    std::string sim_result_bench = dir_out + "/" + temp_bench_clean;
    bool optimization_success = (ret == 0) && fs::exists(sim_result_bench);

    if (!optimization_success) {
        std::cerr << "[Warning] Simplifier failed. Copying input to output." << std::endl;
        fs::copy(input_aig, output_aig, fs::copy_options::overwrite_existing);
    } else {
        std::string aig_cmd = ABC_PATH + " -c \"read_bench " + sim_result_bench + "; strash; write_aiger " + output_aig + "\" > /dev/null 2>&1";
        run_command(aig_cmd);
    }

    // Cleanup
    if (fs::exists(temp_aig_raw)) fs::remove(temp_aig_raw);
    if (fs::exists(dir_in)) fs::remove_all(dir_in);
    if (fs::exists(dir_out)) fs::remove_all(dir_out);

    return 0;
}
