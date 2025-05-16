#pragma once
#include <optional>
#include "Result.h"
#include "Model.h"
#include "FileManager.h"

class BufferManager {
    FileManager& fm;

    // Data Frame
    std::optional<WineRecord> dataFrame_content;
    int dataFrame_recordId = -1; 
    // Não há estado 'dirty' para dataFrame neste trabalho, pois os dados são lidos ou anexados.

    // Index Frame
    std::optional<BPTNode> indexFrame_node; 
    int indexFrame_nodeId = -1;    
    bool indexFrame_isDirty = false; 

public:
    explicit BufferManager(FileManager& fileMgr);

    // Operações de Dados
    // Busca um registro de dados. Se não estiver no frame, carrega do disco.
    Result<WineRecord, BufferError> fetchDataRecord(int recordId); 
    // Anexa um registro de dados ao arquivo e opcionalmente atualiza o frame de dados.
    Result<int, BufferError> appendDataRecord(const WineRecord& r); // Renomeado de putData para clareza

    // Operações de Nó de Índice
    // Busca um nó. Se não estiver no frame, o frame atual (se sujo e diferente) é salvo primeiro.
    // Retorna uma CÓPIA do nó do buffer.
    Result<BPTNode, BufferError> fetchNode(int nodeId); 

    // Coloca o nó fornecido no frame de índice e o marca como sujo.
    // Se o frame continha um nó diferente e sujo, esse nó é salvo primeiro.
    Result<Unit, BufferError> stageNode(const BPTNode& node); 

    // Salva o conteúdo do frame de índice no disco SE estiver sujo.
    Result<Unit, BufferError> flushIndexNodeIfDirty();
    
    // Destrutor para garantir que o frame de índice sujo seja salvo ao sair.
    ~BufferManager(); 
};