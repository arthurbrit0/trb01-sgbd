#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include "tree_index.h"
#include "log.h"
#include <fstream>
#include <sstream>
#include <vector>

class InputManager {
    std::string inputPath, outPath, indexPath, csvPath;
    IndexTree*  idx = nullptr;

    static std::string trim(const std::string& s) {
        auto a = s.find_first_not_of(" \t\r\n");
        auto b = s.find_last_not_of(" \t\r\n");
        return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
    }
public:
    InputManager(const std::string& in, const std::string& out,
                 const std::string& idxP, const std::string& csv)
        : inputPath(in), outPath(out), indexPath(idxP), csvPath(csv) {}

    ~InputManager() { if (idx) { idx->Flush(); delete idx; } }

    void ProcessInput() {
        std::ifstream in(inputPath);
        std::ofstream out(outPath);
        std::string line; bool first = true;

        while (std::getline(in, line)) {
            line = trim(line);
            if (line.empty()) continue;

            if (first) {                                   // primeira linha
                out << line << '\n';
                int n = std::stoi(line.substr(line.find('/') + 1));
                idx   = new IndexTree(n - 1, indexPath);
                first = false;
                continue;
            }

            auto pos = line.find(':');
            std::string op = line.substr(0, pos + 1);
            int v = std::stoi(line.substr(pos + 1));

            if (op == "INC:") {
                int cnt = 0;
                std::ifstream csv(csvPath);
                std::string row; std::getline(csv, row);   // header
                while (std::getline(csv, row)) {
                    std::istringstream ss(row);
                    std::vector<std::string> col;
                    std::string t;
                    while (std::getline(ss, t, ',')) col.push_back(t);
                    if (std::stoi(col[2]) == v) {
                        idx->Insert(v, std::stoi(col[0]) + 1);
                        ++cnt;
                    }
                }
                out << "INC:" << v << "/" << cnt << '\n';
            } else if (op == "BUS=:") {
                std::vector<int> res;
                idx->Search(v, res);
                out << "BUS=:" << v << "/" << res.size() << '\n';
            }
        }
        out << "H/" << idx->GetHeight() << '\n';
        idx->Flush();                                      // força saída
    }
};

#endif // INPUTMANAGER_H
