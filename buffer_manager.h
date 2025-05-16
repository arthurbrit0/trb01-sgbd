#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <string>
#include <unordered_map>
#include <list>
#include <fstream>
#include <stdexcept>

class BufferManager {
public:
    struct Frame {
        int pageId;         // ID da página (número da linha no arquivo binário)
        bool dirty;         // se conteúdo foi modificado e precisa ser escrito
        std::string data;   // buffer com o conteúdo da página
    };

    // path: arquivo de índice ou dados
    // pageSize: tamanho fixo da página em bytes (p.ex. calculado em tree_index)
    // capacity: número máximo de frames em buffer
    BufferManager(const std::string& path, size_t pageSize, size_t capacity)
        : filePath(path), pageSize(pageSize), capacity(capacity) {
        fileStream.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
        if (!fileStream)
            throw std::runtime_error("Não foi possível abrir arquivo: " + filePath);
    }

    ~BufferManager() {
        flushAll();
        if (fileStream.is_open()) fileStream.close();
    }

    // Retorna referência ao conteúdo da página (traz para buffer se ausente)
    std::string& getPage(int pageId) {
        auto it = pageTable.find(pageId);
        if (it != pageTable.end()) {
            // Move frame para a frente (MRU)
            buffer.splice(buffer.begin(), buffer, it->second);
            return it->second->data;
        }
        // novo frame: evict se necessário
        evictIfNeeded();
        Frame frame;
        frame.pageId = pageId;
        frame.dirty = false;
        frame.data.resize(pageSize);
        fileStream.seekg((std::streamoff)pageId * pageSize);
        fileStream.read(&frame.data[0], pageSize);
        buffer.push_front(frame);
        pageTable[pageId] = buffer.begin();
        return buffer.begin()->data;
    }

    // Marca página como suja (modificada)
    void markDirty(int pageId) {
        auto it = pageTable.find(pageId);
        if (it != pageTable.end()) {
            it->second->dirty = true;
            buffer.splice(buffer.begin(), buffer, it->second);
        }
    }

    // Grava uma página suja de volta no disco
    void flushPage(int pageId) {
        auto it = pageTable.find(pageId);
        if (it != pageTable.end() && it->second->dirty) {
            fileStream.seekp((std::streamoff)pageId * pageSize);
            fileStream.write(it->second->data.data(), pageSize);
            it->second->dirty = false;
        }
    }

    // Grava todas as páginas sujas no disco
    void flushAll() {
        for (auto& frame : buffer) {
            if (frame.dirty) {
                fileStream.seekp((std::streamoff)frame.pageId * pageSize);
                fileStream.write(frame.data.data(), pageSize);
                frame.dirty = false;
            }
        }
    }

private:
    std::string filePath;
    size_t pageSize;
    size_t capacity;
    std::fstream fileStream;

    // Lista de frames: front = mais recentemente usado (MRU), back = a ser evictado (LRU)
    std::list<Frame> buffer;
    std::unordered_map<int, std::list<Frame>::iterator> pageTable;

    void evictIfNeeded() {
        if (buffer.size() >= capacity) {
            auto& victim = buffer.back();
            if (victim.dirty) {
                fileStream.seekp((std::streamoff)victim.pageId * pageSize);
                fileStream.write(victim.data.data(), pageSize);
            }
            pageTable.erase(victim.pageId);
            buffer.pop_back();
        }
    }
};

#endif // BUFFER_MANAGER_H
