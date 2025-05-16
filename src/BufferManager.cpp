#include "BufferManager.h"

BufferManager::BufferManager(FileManager& fileMgr) : fm(fileMgr) {}

BufferManager::~BufferManager() {
    // Garante que o frame de índice seja salvo se estiver sujo quando o BufferManager for destruído.
    flushIndexNodeIfDirty(); 
}

Result<WineRecord, BufferError> BufferManager::fetchDataRecord(int recordId) {
    // Verifica se o registro correto já está no data frame
    if (dataFrame_content.has_value() && dataFrame_recordId == recordId) {
        return Result<WineRecord, BufferError>::Ok(*dataFrame_content);
    }

    // Se não, carrega do disco
    auto r = fm.readDataRecord(recordId);
    if (!r.success) {
        return Result<WineRecord, BufferError>::Err({r.error.msg});
    }
    dataFrame_content = r.value;
    dataFrame_recordId = recordId; // Armazena o ID do registro que está no frame
    return Result<WineRecord, BufferError>::Ok(*dataFrame_content);
}

Result<int, BufferError> BufferManager::appendDataRecord(const WineRecord& wine_rec) {
    // Anexa o registro ao arquivo de dados
    auto append_res = fm.appendDataRecord(wine_rec);
    if (!append_res.success) {
        return Result<int, BufferError>::Err({append_res.error.msg});
    }
    
    // Opcional: atualizar o data frame com o registro recém-anexado.
    // Para este trabalho, manter o frame de dados simples e não necessariamente sincronizado com o último append.
    // A alternativa seria: 
    // dataFrame_content = wine_rec;
    // dataFrame_recordId = append_res.value;
    // Ou simplesmente invalidar:
    // dataFrame_content.reset();
    // dataFrame_recordId = -1;
    // Por ora, vamos deixar o data frame como estava ou invalidá-lo para evitar confusão.
    dataFrame_content.reset(); // Invalida o frame de dados após um append.
    dataFrame_recordId = -1;

    return Result<int, BufferError>::Ok(append_res.value);
}

Result<Unit, BufferError> BufferManager::flushIndexNodeIfDirty() {
    if (indexFrame_node.has_value() && indexFrame_isDirty) {
        auto save_res = fm.saveNode(*indexFrame_node);
        if (!save_res.success) {
            return Result<Unit, BufferError>::Err({save_res.error.msg + " (ao salvar nó sujo)"});
        }
        indexFrame_isDirty = false; // Marcado como limpo após salvar
    }
    return Result<Unit, BufferError>::Ok(Unit{});
}

Result<BPTNode, BufferError> BufferManager::fetchNode(int nodeId) {
    // Se o nó solicitado já estiver no frame, retorna uma cópia dele.
    if (indexFrame_node.has_value() && indexFrame_nodeId == nodeId) {
        return Result<BPTNode, BufferError>::Ok(*indexFrame_node);
    }

    // Nó não está no frame (ou é um nó diferente). 
    // Se o frame atual contiver um nó diferente e sujo, salve-o primeiro.
    if (indexFrame_node.has_value() && indexFrame_nodeId != nodeId && indexFrame_isDirty) {
        auto flush_res = flushIndexNodeIfDirty(); // Tenta salvar o nó atual
        if (!flush_res.success) {
            return Result<BPTNode, BufferError>::Err(flush_res.error); // Propaga o erro do flush
        }
    }

    // Agora, carregue o novo nó do disco.
    auto load_res = fm.loadNode(nodeId);
    if (!load_res.success) {
        return Result<BPTNode, BufferError>::Err({load_res.error.msg});
    }

    indexFrame_node = load_res.value;
    indexFrame_nodeId = nodeId;
    indexFrame_isDirty = false; // O nó recém-carregado não está sujo.

    return Result<BPTNode, BufferError>::Ok(*indexFrame_node);
}

Result<Unit, BufferError> BufferManager::stageNode(const BPTNode& node_to_stage) {
    // Se o frame de índice atual contiver um nó diferente do que queremos 'stagear', 
    // e esse nó estiver sujo, ele deve ser salvo primeiro.
    if (indexFrame_node.has_value() && indexFrame_nodeId != node_to_stage.id && indexFrame_isDirty) {
         auto flush_res = flushIndexNodeIfDirty(); // Tenta salvar o nó atual
         if (!flush_res.success) {
            // Não podemos prosseguir se o flush falhar, pois perderíamos dados.
            return Result<Unit, BufferError>::Err(flush_res.error); 
        }
    }

    // Coloca o novo nó no frame e marca como sujo.
    indexFrame_node = node_to_stage;
    indexFrame_nodeId = node_to_stage.id;
    indexFrame_isDirty = true; // O nó no frame agora está modificado (sujo).

    return Result<Unit, BufferError>::Ok(Unit{});
}