#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include "tree_index.h"
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

class InputManager {
private:
    std::string inputPath, outPath, indexPath, csvPath;
    IndexTree *idx;
public:
    InputManager(const std::string &in, const std::string &out,
                 const std::string &idxP, const std::string &csv)
        : inputPath(in), outPath(out), indexPath(idxP), csvPath(csv), idx(nullptr) {}

    void ProcessInput() {
        std::ifstream in(inputPath);
        std::ofstream out(outPath);
        std::string line;
        bool first = true;
        while (std::getline(in, line)) {
            line = trim(line);
            if (line.empty()) continue;
            if (first) {
                out << line << '\n';
                int n = std::stoi(line.substr(line.find('/') + 1));
                idx = new IndexTree(n - 1, indexPath);
                first = false;
                continue;
            }
            auto pos = line.find(':');
            std::string op = line.substr(0, pos + 1);
            int v = std::stoi(line.substr(pos + 1));
            if (op == "INC:") {
                int cnt = 0;
                std::ifstream csv(csvPath);
                std::string row;
                std::getline(csv, row); // header
                while (std::getline(csv, row)) {
                    std::istringstream ss(row);
                    std::vector<std::string> tok;
                    std::string t;
                    while (std::getline(ss, t, ',')) tok.push_back(t);
                    int year = std::stoi(tok[2]);
                    if (year == v) {
                        idx->Insert(year, std::stoi(tok[0]) + 1);
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
    }
};

#endif // INPUTMANAGER_H