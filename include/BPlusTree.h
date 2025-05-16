#pragma once
#include <vector>
#include "Model.h"
#include "BufferManager.h"

class BPlusTree {
    int degree;
    int rootId;
    int nextId;
    int height;
    FileManager& fm;
    BufferManager& bm;

    // Estrutura auxiliar para o resultado do split na inserção recursiva
    struct SplitResult {
        bool split_occurred = false;
        int key_to_promote = -1; 
        int new_sibling_node_id = -1; 
    };

    // Método auxiliar para inserção recursiva
    SplitResult insert_recursive(int current_node_id, int key, const RID& rid, BPTError& err);

public:
    BPlusTree(int deg, FileManager& fileMgr, BufferManager& bufMgr);
    int getHeight() const;
    std::vector<RID> search(int key, BPTError& err);
    void insert(int key, const RID& rid, BPTError& err);
};