#pragma once
#include <tuple>
#include <string>
#include "Result.h"
#include "Model.h"

class FileManager {
    std::string dataPath;
    std::string indexPath;
public:
    FileManager(const std::string& dp, const std::string& ip);
    // Leitura de dados CSV
    Result<WineRecord, StoreError> readDataRecord(int recordId);
    // Append de registro e retorna novo ID
    Result<int, StoreError> appendDataRecord(const WineRecord& r);

    // Metadados de índice
    Result<std::tuple<int,int,int>, StoreError> loadMetadata();
    Result<Unit, StoreError> saveMetadata(int root, int next, int height);
    // Nós de índice
    Result<BPTNode, StoreError> loadNode(int nodeId);
    Result<Unit, StoreError> saveNode(const BPTNode& node);
};