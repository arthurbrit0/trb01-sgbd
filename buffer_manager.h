#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include "log.h"
#include <string>
#include <unordered_map>
#include <list>
#include <fstream>
#include <stdexcept>

class BufferManager {
public:
    struct Frame {
        int         pageId;
        bool        dirty;
        std::string data;
    };

    BufferManager(const std::string& path,
                  size_t pageSize,
                  size_t capacity)
        : filePath(path),
          pageSize(pageSize),
          capacity(capacity) {

        fileStream.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
        if (!fileStream) {
            LOG("Arquivo inexistente, criando: " << filePath);
            std::ofstream newFile(filePath, std::ios::binary);
            newFile.close();
            fileStream.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
        }
        if (!fileStream)
            throw std::runtime_error("Nao foi possivel abrir: " + filePath);

        LOG("BufferManager OK (pageSize=" << pageSize
            << ", cap=" << capacity << ')');
    }

    ~BufferManager() { flushAll(); }

    /* devolve referência (std::string&) à página pedida */
    std::string& getPage(int pageId) {
        auto it = pageTable.find(pageId);
        if (it != pageTable.end()) {                   // HIT
            buffer.splice(buffer.begin(), buffer, it->second);
            LOG("HIT p" << pageId);
            return it->second->data;
        }

        evictIfNeeded();                              // MISS
        Frame f;
        f.pageId = pageId;
        f.dirty  = false;
        f.data.assign(pageSize, ' ');

        fileStream.clear();                           // <-- limpa bits de erro
        fileStream.seekg(static_cast<std::streamoff>(pageId) * pageSize);
        fileStream.read(&f.data[0], pageSize);

        buffer.push_front(std::move(f));
        pageTable[pageId] = buffer.begin();
        LOG("MISS p" << pageId << " carregada");
        return buffer.begin()->data;
    }

    void markDirty(int pageId) {
        auto it = pageTable.find(pageId);
        if (it != pageTable.end()) {
            it->second->dirty = true;
            buffer.splice(buffer.begin(), buffer, it->second);
            LOG("markDirty p" << pageId);
        }
    }

    void flushAll() {
        LOG("flushAll()");
        for (auto& f : buffer) {
            if (f.dirty) {
                fileStream.clear();                   // <-- limpa flags
                fileStream.seekp(static_cast<std::streamoff>(f.pageId) * pageSize);
                fileStream.write(f.data.data(), pageSize);
                LOG("flush p" << f.pageId);
                f.dirty = false;
            }
        }
        fileStream.flush();
    }

private:
    void evictIfNeeded() {
        if (buffer.size() < capacity) return;

        Frame& v = buffer.back();
        LOG("Evict p" << v.pageId << (v.dirty ? " (dirty)" : " (clean)"));

        if (v.dirty) {
            fileStream.clear();                       // <-- limpa flags
            fileStream.seekp(static_cast<std::streamoff>(v.pageId) * pageSize);
            fileStream.write(v.data.data(), pageSize);
        }
        pageTable.erase(v.pageId);
        buffer.pop_back();
    }

    std::string filePath;
    size_t      pageSize;
    size_t      capacity;
    std::fstream fileStream;

    std::list<Frame> buffer;   // lista LRU
    std::unordered_map<int,
        std::list<Frame>::iterator> pageTable;   // id -> iterador
};

#endif // BUFFER_MANAGER_H
