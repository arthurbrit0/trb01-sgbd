#pragma once
#include <string>
#include <vector>

// Unidade para métodos sem valor de retorno
struct Unit {};

// Registro de vinho
struct WineRecord {
    int wineId;
    std::string label;
    int harvestYear;
    std::string type;
};

// RID para B+ Tree: página + offset
struct RID {
    int pageId;
    int offset;
};

// Nó de B+ Tree
struct BPTNode {
    int id;
    bool isLeaf;
    std::vector<int> keys;
    std::vector<int> children; // IDs de filhos
    std::vector<RID> rids;     // só em folha
    int next;                  // próximo leaf
};

// Erros
struct BufferError { std::string msg; };
struct StoreError  { std::string msg; };
struct BPTError    { std::string msg; };
struct ParseError  { std::string msg; int line; };