#include "FileManager.h"
#include <fstream>
#include <sstream>

FileManager::FileManager(const std::string& dp, const std::string& ip)
  : dataPath(dp), indexPath(ip) {}

Result<WineRecord, StoreError> FileManager::readDataRecord(int recordId) {
    std::ifstream f(dataPath);
    if (!f) return Result<WineRecord, StoreError>::Err({"CSV não encontrado"});
    std::string line;
    std::getline(f, line); // cabeçalho
    for (int i = 0; i < recordId; ++i) {
        if (!std::getline(f, line))
            return Result<WineRecord, StoreError>::Err({"Registro não existe"});
    }
    std::stringstream ss(line);
    WineRecord w;
    std::string tmp;
    std::getline(ss, tmp, ','); w.wineId = std::stoi(tmp);
    std::getline(ss, w.label, ',');
    std::getline(ss, tmp, ','); w.harvestYear = std::stoi(tmp);
    std::getline(ss, w.type);
    return Result<WineRecord, StoreError>::Ok(w);
}

Result<int, StoreError> FileManager::appendDataRecord(const WineRecord& r) {
    std::ifstream fin(dataPath);
    if (!fin) return Result<int, StoreError>::Err({"CSV não encontrado"});
    int count = 0;
    std::string line;
    std::getline(fin, line); // cabeçalho
    while (std::getline(fin, line)) ++count;
    std::ofstream fout(dataPath, std::ios::app);
    if (!fout) return Result<int, StoreError>::Err({"Falha ao abrir CSV"});
    fout << r.wineId << ',' << r.label << ',' << r.harvestYear << ',' << r.type << '\n';
    return Result<int, StoreError>::Ok(count);
}

Result<std::tuple<int,int,int>, StoreError> FileManager::loadMetadata() {
    std::ifstream f(indexPath);
    if (!f) return Result<std::tuple<int,int,int>, StoreError>::Err({"Índice não existe (loadMetadata)"});
    int root=0, next=1, height=0;
    std::string line;
    try {
        while (std::getline(f, line)) {
            if (line.rfind("ROOT=",0)==0) root = std::stoi(line.substr(5));
            else if (line.rfind("NEXT=",0)==0) next = std::stoi(line.substr(5));
            else if (line.rfind("HEIGHT=",0)==0) height = std::stoi(line.substr(7));
            if (line.empty() || line.find("NODE ") == 0) break; // Parar se linha vazia ou se chegou nos nós
        }
    } catch (const std::invalid_argument& ia) {
        return Result<std::tuple<int,int,int>, StoreError>::Err({"Formato de metadados inválido no índice (stoi falhou): " + std::string(ia.what())});
    } catch (const std::out_of_range& oor) {
        return Result<std::tuple<int,int,int>, StoreError>::Err({"Valor de metadados fora do range no índice (stoi falhou): " + std::string(oor.what())});
    }
    return Result<std::tuple<int,int,int>, StoreError>::Ok({root,next,height});
}

Result<Unit, StoreError> FileManager::saveMetadata(int root, int next, int height) {
    std::ifstream fin(indexPath);
    std::vector<std::string> lines;
    if (fin) {
        std::string l;
        while (std::getline(fin,l)) lines.push_back(l);
    }
    std::ofstream fout(indexPath);
    fout << "ROOT=" << root << '\n';
    fout << "NEXT=" << next << '\n';
    fout << "HEIGHT="<< height << "\n\n";
    for (auto& l: lines)
        if (l.rfind("NODE ",0)==0)
            fout << l << '\n';
    return Result<Unit, StoreError>::Ok(Unit{});
}

Result<BPTNode, StoreError> FileManager::loadNode(int nodeId) {
    std::ifstream f(indexPath);
    if (!f) return Result<BPTNode, StoreError>::Err({"Índice não existe (loadNode)"});
    std::string line;
    try {
        while (std::getline(f,line)) {
            if (line.rfind("NODE "+std::to_string(nodeId)+" ",0)==0) {
                BPTNode n; n.id=nodeId;
                std::vector<std::string> parts;
                std::stringstream ss(line);
                std::string p;
                while (std::getline(ss,p,'|')) parts.push_back(p);
                
                if (parts.size() < 3) return Result<BPTNode, StoreError>::Err({"Formato de nó inválido (pocas partes) para nó " + std::to_string(nodeId)});

                n.isLeaf = (parts[1].find("true")!=std::string::npos);
                auto pos_keys = parts[2].find('=');
                if (pos_keys == std::string::npos) return Result<BPTNode, StoreError>::Err({"Formato de nó inválido (sem '=' para KEYS) para nó " + std::to_string(nodeId)});
                std::string ks = parts[2].substr(pos_keys+1);
                if (!ks.empty()) {
                    std::stringstream sk(ks);
                    std::string ki;
                    while(std::getline(sk,ki,',')) n.keys.push_back(std::stoi(ki));
                }

                if (n.isLeaf) {
                    if (parts.size() < 5) return Result<BPTNode, StoreError>::Err({"Formato de nó folha inválido (pocas partes) para nó " + std::to_string(nodeId)});
                    auto pos_values = parts[3].find('=');
                    if (pos_values == std::string::npos) return Result<BPTNode, StoreError>::Err({"Formato de nó inválido (sem '=' para VALUES) para nó " + std::to_string(nodeId)});
                    std::string vs = parts[3].substr(pos_values+1);
                    if (!vs.empty()) {
                        std::stringstream sv(vs);
                        std::string ri;
                        while(std::getline(sv,ri,',')) n.rids.push_back({std::stoi(ri),0});
                    }
                    auto pos_next = parts[4].find('=');
                    if (pos_next == std::string::npos) return Result<BPTNode, StoreError>::Err({"Formato de nó inválido (sem '=' para NEXT) para nó " + std::to_string(nodeId)});
                    std::string ns = parts[4].substr(pos_next+1);
                    n.next = (ns=="null"?-1:std::stoi(ns));
                } else { // Nó interno
                    if (parts.size() < 4) return Result<BPTNode, StoreError>::Err({"Formato de nó interno inválido (pocas partes) para nó " + std::to_string(nodeId)});
                    auto pos_children = parts[3].find('=');
                    if (pos_children == std::string::npos) return Result<BPTNode, StoreError>::Err({"Formato de nó inválido (sem '=' para CHILDREN) para nó " + std::to_string(nodeId)});
                    std::string cs = parts[3].substr(pos_children+1);
                    if (!cs.empty()) {
                        std::stringstream sc(cs);
                        std::string ci;
                        while(std::getline(sc,ci,',')) n.children.push_back(std::stoi(ci));
                    }
                }
                return Result<BPTNode, StoreError>::Ok(n);
            }
        }
    } catch (const std::invalid_argument& ia) {
        return Result<BPTNode, StoreError>::Err({"Dado de nó inválido no índice (stoi falhou): " + std::string(ia.what()) + " na linha: " + line});
    } catch (const std::out_of_range& oor) {
        return Result<BPTNode, StoreError>::Err({"Valor de nó fora do range no índice (stoi falhou): " + std::string(oor.what()) + " na linha: " + line});
    }
    return Result<BPTNode, StoreError>::Err({"Nó " + std::to_string(nodeId) + " não encontrado"});
}

Result<Unit, StoreError> FileManager::saveNode(const BPTNode& n) {
    std::ifstream fin(indexPath);
    std::vector<std::string> lines;
    if (fin) {
        std::string l;
        while(std::getline(fin,l)) lines.push_back(l);
    }
    std::ostringstream sb;
    sb << "NODE " << n.id << " | LEAF="<<(n.isLeaf?"true":"false")
       <<" | KEYS=";
    for (size_t i=0; i<n.keys.size(); ++i) {
        if (i) sb<<",";
        sb<<n.keys[i];
    }
    if (n.isLeaf) {
        sb<<" | VALUES=";
        for (size_t i=0; i<n.rids.size(); ++i) {
            if (i) sb<<",";
            sb<<n.rids[i].pageId;
        }
        sb<<" | NEXT="<<(n.next==-1?"null":std::to_string(n.next));
    } else {
        sb<<" | CHILDREN=";
        for (size_t i=0; i<n.children.size(); ++i) {
            if (i) sb<<",";
            sb<<n.children[i];
        }
    }
    std::string nodeLine = sb.str();
    bool found=false;
    for (auto &l: lines) {
        if (l.rfind("NODE "+std::to_string(n.id)+" ",0)==0) {
            l = nodeLine; found=true; break;
        }
    }
    if (!found) lines.push_back(nodeLine);
    std::ofstream fout(indexPath);
    for (auto &l: lines) fout<<l<<"\n";
    return Result<Unit, StoreError>::Ok(Unit{});
}