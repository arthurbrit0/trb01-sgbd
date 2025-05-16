#ifndef INDEXNODE_H
#define INDEXNODE_H

#include <string>
#include <vector>
#include <sstream>

inline std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? std::string() : s.substr(start, end - start + 1);
}

class IndexNode {
public:
    int id;
    bool isLeaf;
    std::vector<int> keys;
    int parent;
    std::vector<int> children;
    int prev;
    int next;
    std::vector<int> refs;

    IndexNode()
        : id(-1), isLeaf(false), parent(-1), prev(-1), next(-1) {}

    std::string Serialize() const {
        std::ostringstream oss;
        oss << id << ";" << (isLeaf ? 1 : 0) << ";";
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i) oss << ',';
            oss << keys[i];
        }
        oss << ";" << parent << ";";
        if (!children.empty()) {
            for (size_t i = 0; i < children.size(); ++i) {
                if (i) oss << ',';
                oss << children[i];
            }
        } else {
            oss << "null";
        }
        oss << ";" << (prev == -1 ? "null" : std::to_string(prev))
            << ";" << (next == -1 ? "null" : std::to_string(next))
            << ";";
        for (size_t i = 0; i < refs.size(); ++i) {
            if (i) oss << ',';
            oss << refs[i];
        }
        return oss.str();
    }

    static IndexNode Deserialize(const std::string& line) {
        IndexNode node;
        std::vector<std::string> parts;
        std::istringstream iss(line);
        std::string part;
        while (std::getline(iss, part, ';')) parts.push_back(part);
        // id
        node.id = std::stoi(trim(parts[0]));
        // isLeaf
        node.isLeaf = (trim(parts[1]) == "1");
        // keys
        if (!parts[2].empty()) {
            std::istringstream ks(parts[2]);
            std::string k;
            while (std::getline(ks, k, ','))
                node.keys.push_back(std::stoi(trim(k)));
        }
        // parent
        node.parent = std::stoi(trim(parts[3]));
        // children
        if (parts[4] != "null") {
            std::istringstream cs(parts[4]);
            std::string c;
            while (std::getline(cs, c, ','))
                node.children.push_back(std::stoi(trim(c)));
        }
        // prev
        node.prev = (trim(parts[5]) == "null") ? -1 : std::stoi(trim(parts[5]));
        // next
        node.next = (trim(parts[6]) == "null") ? -1 : std::stoi(trim(parts[6]));
        // refs
        if (!parts[7].empty()) {
            std::istringstream rs(parts[7]);
            std::string r;
            while (std::getline(rs, r, ',')) {
                std::string t = trim(r);
                if (!t.empty()) node.refs.push_back(std::stoi(t));
            }
        }
        return node;
    }
};

#endif // INDEXNODE_H