#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cstdint>
#include <set>
#include <map>

#include "base/abc/abc.h"
#include "base/main/main.h"

namespace fs = std::filesystem;

/*** ================== Implicant 結構 ================== ***/
// mask: 1 = don't care
// bits: 在 mask=0 的位置上，實際的 0/1
struct Implicant {
    uint32_t bits;
    uint32_t mask;

    bool operator<(const Implicant& other) const {
        if (mask != other.mask) return mask < other.mask;
        return bits < other.bits;
    }
    bool operator==(const Implicant& other) const {
        return mask == other.mask && bits == other.bits;
    }
};

/*** ================== 小工具函式 ================== ***/

// 展開一個 implicant 所有覆蓋的 minterms（nVars 通常 <= 20）
void ExpandImplicant(const Implicant& imp, int nVars, std::vector<int>& outMints) {
    outMints.clear();
    std::vector<int> dcPos;
    for (int i = 0; i < nVars; ++i) {
        if ((imp.mask >> i) & 1u)
            dcPos.push_back(i);
    }
    int numDc = (int)dcPos.size();
    int combos = 1 << numDc;

    for (int c = 0; c < combos; ++c) {
        uint32_t val = imp.bits;
        for (int j = 0; j < numDc; ++j) {
            int pos = dcPos[j];
            if ((c >> j) & 1) {
                val |= (1u << pos);
            } else {
                val &= ~(1u << pos);
            }
        }
        outMints.push_back((int)val);
    }
}

// 嘗試 combine 兩個 implicant
bool CombineImplicants(const Implicant& a, const Implicant& b, Implicant& out) {
    if (a.mask != b.mask) return false;
    uint32_t diff = a.bits ^ b.bits;
    if (diff == 0) return false;
    // diff 必須只有一個 bit
    if (diff & (diff - 1)) return false;
    // 這個 bit 不能已經是 don't care
    if (a.mask & diff) return false;
    out.mask = a.mask | diff;
    out.bits = a.bits & ~diff;
    return true;
}

/*** ================== QM 最小化 ================== ***/

// onset: 包含所有 f(x) = 1 的 minterm index
// 回傳：一組 implicant（用 greedy cover）
std::vector<Implicant> QM_Minimize(const std::vector<int>& onset, int nVars) {
    std::vector<Implicant> result;
    if (onset.empty()) return result; // constant 0

    // 初始集合：每個 minterm 一個 implicant
    std::set<Implicant> current;
    for (int m : onset) {
        Implicant imp;
        imp.bits = (uint32_t)m;
        imp.mask = 0u;
        current.insert(imp);
    }

    std::set<Implicant> primeImps;
    std::vector<Implicant> curList;

    while (!current.empty()) {
        std::set<Implicant> next;
        std::set<Implicant> used;
        curList.assign(current.begin(), current.end());

        for (size_t i = 0; i < curList.size(); ++i) {
            for (size_t j = i + 1; j < curList.size(); ++j) {
                Implicant comb;
                if (CombineImplicants(curList[i], curList[j], comb)) {
                    next.insert(comb);
                    used.insert(curList[i]);
                    used.insert(curList[j]);
                }
            }
        }

        for (const auto& imp : current) {
            if (used.find(imp) == used.end()) {
                primeImps.insert(imp);
            }
        }

        if (next.empty()) break;
        current.swap(next);
    }

    // 建立每個 prime implicant 覆蓋的 onset minterms
    std::map<Implicant, std::vector<int>> impCover;
    std::vector<int> expanded;
    for (const auto& imp : primeImps) {
        ExpandImplicant(imp, nVars, expanded);
        std::vector<int> cover;
        for (int m : expanded) {
            if (std::find(onset.begin(), onset.end(), m) != onset.end()) {
                cover.push_back(m);
            }
        }
        if (!cover.empty()) {
            impCover[imp] = cover;
        }
    }

    // greedy cover
    std::set<int> uncovered(onset.begin(), onset.end());
    while (!uncovered.empty()) {
        Implicant bestImp{};
        int bestGain = 0;
        bool found = false;

        for (const auto& kv : impCover) {
            const Implicant& imp = kv.first;
            const std::vector<int>& cv = kv.second;
            int gain = 0;
            for (int m : cv) {
                if (uncovered.count(m)) gain++;
            }
            if (gain > bestGain) {
                bestGain = gain;
                bestImp = imp;
                found = true;
            }
        }
        if (!found) {
            std::cerr << "  [WARN] Greedy cover stalled, uncovered size = " << uncovered.size() << "\n";
            break;
        }
        result.push_back(bestImp);
        for (int m : impCover[bestImp]) {
            uncovered.erase(m);
        }
    }

    return result;
}

/*** ================== Implicant → Verilog Expr ================== ***/

// 將 implicant 轉成 Verilog 表達式，例如：x0 & ~x1 & x3
std::string ImpToExpr(const Implicant& imp, int nVars) {
    std::vector<std::string> terms;
    for (int i = 0; i < nVars; ++i) {
        uint32_t bitMask = 1u << i;
        if (imp.mask & bitMask) continue; // don't care
        bool isOne = (imp.bits & bitMask) != 0;
        std::stringstream ss;
        if (!isOne) ss << "~";
        ss << "x" << i;
        terms.push_back(ss.str());
    }
    if (terms.empty()) {
        return "1'b1"; // 全 don't care → constant 1
    }
    std::stringstream out;
    for (size_t i = 0; i < terms.size(); ++i) {
        if (i > 0) out << " & ";
        out << terms[i];
    }
    return out.str();
}

/*** ================== ABC command helper ================== ***/

bool ExecAbcCmd(Abc_Frame_t* pAbc, const std::string& cmd) {
    if (Cmd_CommandExecute(pAbc, cmd.c_str())) {
        std::cerr << "  [ABC ERROR] " << cmd << "\n";
        return false;
    }
    return true;
}

/*** ================== Main ================== ***/

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: QM <truth_file>" << std::endl;
        return 1;
    }

    std::string filename = argv[1];
    std::ifstream fin(filename);
    if (!fin.good()) {
        std::cerr << "Cannot open truth file: " << filename << std::endl;
        return 1;
    }

    fs::create_directories("QM/output");

    // 取得檔名前綴（不含路徑與副檔名）
    std::string stem = filename;
    {
        auto pos = stem.find_last_of("/\\");
        if (pos != std::string::npos) stem = stem.substr(pos + 1);
        pos = stem.find_last_of('.');
        if (pos != std::string::npos) stem = stem.substr(0, pos);
    }

    // ------- 讀所有行：每行 = 一個 function -------
    std::vector<std::string> funcs;
    std::string line;
    while (std::getline(fin, line)) {
        // 移除空白
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        if (line.empty()) continue;
        funcs.push_back(line);
    }
    fin.close();

    if (funcs.empty()) {
        std::cerr << "No truth table lines found in " << filename << std::endl;
        return 1;
    }

    // 檢查每行長度一致
    size_t L = funcs[0].size();
    for (size_t i = 1; i < funcs.size(); ++i) {
        if (funcs[i].size() != L) {
            std::cerr << "Line " << i << " length mismatch: " << funcs[i].size() << " vs " << L << std::endl;
            return 1;
        }
    }

    // L 應該是 2^n
    if (L == 0 || (L & (L - 1)) != 0) {
        std::cerr << "Truth length " << L << " is not power-of-2!\n";
        return 1;
    }

    int nVars = 0;
    while ((1u << nVars) < L) nVars++;
    int nOuts = (int)funcs.size();

    std::cout << "nVars = " << nVars << ", nOuts = " << nOuts << ", length = " << L << std::endl;

    // ------- fallback 條件：nVars 太大不跑 QM -------
    if (nVars > 20) {
        std::cout << "[WARN] nVars = " << nVars << " > 20, QM disabled (no output generated)." << std::endl;
        return 1;
    }

    // ------- 建每個 output 的 onset -------
    std::vector<std::vector<int>> onset(nOuts);
    for (int j = 0; j < nOuts; ++j) {
        const std::string& f = funcs[j];
        for (int m = 0; m < (int)L; ++m) {
            if (f[m] == '1')
                onset[j].push_back(m);
        }
    }

    // ------- 對每個 output 跑 QM -------
    std::vector<std::vector<Implicant>> allImps(nOuts);
    for (int j = 0; j < nOuts; ++j) {
        std::cout << "  [QM] Output y" << j << ": onset size = " << onset[j].size() << std::endl;
        allImps[j] = QM_Minimize(onset[j], nVars);
        std::cout << "      implicants = " << allImps[j].size() << std::endl;
    }

    // ------- 寫 multi-output Verilog -------
    std::string verilogFile = "QM/output/" + stem + "_qm.v";
    std::ofstream fout(verilogFile);
    if (!fout.good()) {
        std::cerr << "Cannot open Verilog output: " << verilogFile << std::endl;
        return 1;
    }

    // module 宣告
    fout << "module " << stem << " (";
    for (int i = 0; i < nVars; ++i) {
        fout << "x" << i << ", ";
    }
    for (int j = 0; j < nOuts; ++j) {
        fout << "y" << j;
        if (j + 1 < nOuts) fout << ", ";
    }
    fout << ");\n";

    // inputs
    for (int i = 0; i < nVars; ++i) {
        fout << "  input x" << i << ";\n";
    }
    // outputs
    for (int j = 0; j < nOuts; ++j) {
        fout << "  output y" << j << ";\n";
    }

    // assign yj = ...
    for (int j = 0; j < nOuts; ++j) {
        fout << "  assign y" << j << " = ";
        const auto& imps = allImps[j];
        if (imps.empty()) {
            fout << "1'b0;\n";
            continue;
        }
        for (size_t k = 0; k < imps.size(); ++k) {
            if (k > 0) fout << " | ";
            fout << "(" << ImpToExpr(imps[k], nVars) << ")";
        }
        fout << ";\n";
    }

    fout << "endmodule\n";
    fout.close();

    std::cout << "[INFO] SOP Verilog written to " << verilogFile << std::endl;

    // ------- ABC：read_verilog → AIG 最佳化 → write_aiger -------
    std::string aigFile = "QM/output/" + stem + "_qm.aig";

    Abc_Start();
    Abc_Frame_t* pAbc = Abc_FrameGetGlobalFrame();

    std::stringstream cmd;
    cmd << "read_verilog " << verilogFile << "; "
        << "strash; " 
        // << "balance; rewrite; rewrite -z; refactor; refactor -z; resub -K 8; dc2; "
        << "print_stats; "
        << "write_aiger " << aigFile;

    ExecAbcCmd(pAbc, cmd.str());
    Abc_Stop();

    std::cout << "[DONE] QM+ABC AIG written to " << aigFile << std::endl;

    return 0;
}
