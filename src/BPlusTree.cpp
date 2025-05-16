#include "BPlusTree.h"
#include <algorithm>
#include <sstream>
#include <iostream>

BPlusTree::BPlusTree(int deg, FileManager& fileMgr, BufferManager& bufMgr)
  : degree(deg), fm(fileMgr), bm(bufMgr) {
    auto m = fm.loadMetadata();
    if (m.success) {
        std::tie(rootId, nextId, height) = m.value;
        auto rootNodeCheck = bm.fetchNode(rootId);
        if (!rootNodeCheck.success && rootId == 0 && nextId == 1) {
            // Condição de inicialização padrão, pode ser um arquivo de índice vazio mas válido.
        } else if (!rootNodeCheck.success) {
            // err.msg = "Metadados do índice corruptos ou nó raiz inicial não encontrado. Tentando recriar.";
            std::cerr << "Aviso: Metadados do índice apontam para um nó raiz inválido (id: " << rootId << ") ou nó não pôde ser carregado. Tentando recriar a árvore." << std::endl;
        }
    }
    if (!m.success || (m.success && !bm.fetchNode(std::get<0>(m.value)).success) ){
        BPTNode root_node; 
        root_node.id = 0;
        rootId = root_node.id;
        nextId = 1;
        height = 1;
        root_node.isLeaf = true; 
        root_node.next = -1;
        auto stage_res = bm.stageNode(root_node);
        if (!stage_res.success) {
        }
        auto flush_res = bm.flushIndexNodeIfDirty();
        if (!flush_res.success) {
        }
        fm.saveMetadata(rootId, nextId, height);
    }
}

int BPlusTree::getHeight() const { return height; }

std::vector<RID> BPlusTree::search(int key, BPTError& err) {
    err.msg.clear();
    if (rootId < 0) {
        err.msg = "Árvore está vazia ou não inicializada.";
        return {};
    }
    auto curRes = bm.fetchNode(rootId);
    if (!curRes.success) { err.msg = "Falha ao carregar nó raiz: " + curRes.error.msg; return {}; }
    BPTNode cur = curRes.value;

    while (!cur.isLeaf) {
        int i = 0;
        auto it = std::lower_bound(cur.keys.begin(), cur.keys.end(), key);
        i = std::distance(cur.keys.begin(), it);
        int child_idx = 0;
        while(child_idx < cur.keys.size() && key > cur.keys[child_idx]) {
            child_idx++;
        }
        if (child_idx >= cur.children.size()) {
             err.msg = "Índice de filho inválido durante a busca."; return {}; 
        }

        auto childRes = bm.fetchNode(cur.children[child_idx]);
        if (!childRes.success) { err.msg = "Falha ao carregar nó filho: " + childRes.error.msg; return {}; }
        cur = childRes.value;
    }

    std::vector<RID> ans;
    bool continue_search = true;
    while (continue_search && cur.isLeaf) {
        for (size_t i = 0; i < cur.keys.size(); ++i) {
            if (cur.keys[i] == key) {
                if (i < cur.rids.size()) {
                    ans.push_back(cur.rids[i]);
                } else {
                    err.msg = "Inconsistência de dados: número de chaves e RIDs não corresponde na folha.";
                }
            } else if (cur.keys[i] > key && ans.empty()) {
            }
        }

        bool found_key_in_current_node_gte_search_key = false;
        for(int k_val : cur.keys) {
            if (k_val >= key) {
                found_key_in_current_node_gte_search_key = true;
                break;
            }
        }

        if (!found_key_in_current_node_gte_search_key && cur.next != -1) {
            auto nextNodeRes = bm.fetchNode(cur.next);
            if (!nextNodeRes.success) { err.msg = "Falha ao carregar próximo nó folha: " + nextNodeRes.error.msg; return ans; }
            cur = nextNodeRes.value;
        } else {
            continue_search = false;
        }
    }
    return ans;
}

BPlusTree::SplitResult BPlusTree::insert_recursive(int current_node_id, int key, const RID& rid, BPTError& err) {
    auto current_node_res = bm.fetchNode(current_node_id);
    if (!current_node_res.success) {
        err.msg = "Falha ao carregar nó durante inserção: " + current_node_res.error.msg;
        return {true, -1, -1};
    }
    BPTNode node = current_node_res.value;

    if (node.isLeaf) {
        auto it = std::lower_bound(node.keys.begin(), node.keys.end(), key);
        int idx = std::distance(node.keys.begin(), it);
        node.keys.insert(it, key);
        if (idx < node.rids.size()) {
             node.rids.insert(node.rids.begin() + idx, rid);
        } else {
             node.rids.push_back(rid);
        }

        if (node.keys.size() < degree) {
            bm.stageNode(node);
            return {false, -1, -1};
        } else {
            BPTNode sibling_node;
            sibling_node.id = nextId++;
            sibling_node.isLeaf = true;

            int mid_idx = node.keys.size() / 2;
            int key_to_promote = node.keys[mid_idx];
            
            sibling_node.keys.assign(node.keys.begin() + mid_idx, node.keys.end());
            sibling_node.rids.assign(node.rids.begin() + mid_idx, node.rids.end());
            
            node.keys.resize(mid_idx);
            node.rids.resize(mid_idx);
            
            sibling_node.next = node.next;
            node.next = sibling_node.id;

            bm.stageNode(node);
            bm.stageNode(sibling_node);
            
            return {true, key_to_promote, sibling_node.id};
        }
    } else {
        int child_idx = 0;
        while(child_idx < node.keys.size() && key >= node.keys[child_idx]) {
             if (key == node.keys[child_idx] && child_idx + 1 < node.children.size()) child_idx++;
             else if (key > node.keys[child_idx]) child_idx++;
             else break;
        }
         if (child_idx >= node.children.size()) {
            err.msg = "Índice de filho inválido em nó interno.";
            return {true, -1, -1};
        }

        SplitResult child_split_result = insert_recursive(node.children[child_idx], key, rid, err);

        if (!child_split_result.split_occurred) {
            if (!err.msg.empty() && child_split_result.key_to_promote == -1 && child_split_result.new_sibling_node_id == -1) {
                 return {true, -1, -1};
            }
            return {false, -1, -1};
        }
        if (err.msg.empty() && child_split_result.key_to_promote == -1 && child_split_result.new_sibling_node_id == -1 && child_split_result.split_occurred){
            err.msg = "Split recursivo retornou estado inconsistente.";
            return {true, -1, -1};
        }

        auto it_keys = std::lower_bound(node.keys.begin(), node.keys.end(), child_split_result.key_to_promote);
        int insert_pos_keys = std::distance(node.keys.begin(), it_keys);
        node.keys.insert(it_keys, child_split_result.key_to_promote);
        
        if (insert_pos_keys + 1 <= node.children.size()) {
            node.children.insert(node.children.begin() + insert_pos_keys + 1, child_split_result.new_sibling_node_id);
        } else {
            node.children.push_back(child_split_result.new_sibling_node_id);
        }
        

        if (node.keys.size() < degree) {
            bm.stageNode(node);
            return {false, -1, -1};
        } else {
            BPTNode new_internal_sibling;
            new_internal_sibling.id = nextId++;
            new_internal_sibling.isLeaf = false;

            int mid_idx = node.keys.size() / 2;
            int key_to_promote_upwards = node.keys[mid_idx];

            new_internal_sibling.keys.assign(node.keys.begin() + mid_idx + 1, node.keys.end());
            new_internal_sibling.children.assign(node.children.begin() + mid_idx + 1, node.children.end());

            node.keys.resize(mid_idx);
            node.children.resize(mid_idx + 1);

            bm.stageNode(node);
            bm.stageNode(new_internal_sibling);

            return {true, key_to_promote_upwards, new_internal_sibling.id};
        }
    }
}

void BPlusTree::insert(int key, const RID& rid, BPTError& err) {
    err.msg.clear();
    if (rootId < 0) {
        BPTNode root_node; 
        root_node.id = 0; rootId = root_node.id;
        nextId = 1; height = 1;
        root_node.isLeaf = true; root_node.next = -1;
        bm.stageNode(root_node);
    }

    SplitResult split_res = insert_recursive(rootId, key, rid, err);

    if (!err.msg.empty() && split_res.key_to_promote == -1 && split_res.new_sibling_node_id == -1 && split_res.split_occurred) {
        fm.saveMetadata(rootId, nextId, height);
        return;
    }

    if (split_res.split_occurred && split_res.new_sibling_node_id != -1) {
        BPTNode new_root;
        new_root.id = nextId++;
        new_root.isLeaf = false;
        new_root.keys.push_back(split_res.key_to_promote);
        new_root.children.push_back(rootId);
        new_root.children.push_back(split_res.new_sibling_node_id);
        
        rootId = new_root.id;
        height++;
        bm.stageNode(new_root);
    }
    fm.saveMetadata(rootId, nextId, height);
}