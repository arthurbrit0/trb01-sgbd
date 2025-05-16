#ifndef INDEXTREE_H
#define INDEXTREE_H

#include "node_index.h"
#include "buffer_manager.h"
#include "log.h"
#include <stack>
#include <fstream>
#include <sstream>

/* --------------- classe --------------- */
class IndexTree {
    int            order;
    int            pageSize;
    std::string    filePath;
    BufferManager  buffer;
    IndexNode      nodeBuffer;

    /* -------- utilidades internas -------- */
    static int pageSizeForOrder(int ord) { return 37 + 12 * ord; }

    void writeFixedLine(int line, const std::string& content) {
        if ((int)content.size() > pageSize - 1) throw std::invalid_argument("line too long");
        std::string& pg = buffer.getPage(line);
        pg.assign(content);
        pg.resize(pageSize - 1, ' ');
        pg.push_back('\n');
        buffer.markDirty(line);
        LOG("WRITE p" << line << " '" << content << '\'');
    }

    void loadNode(int id) {
        std::string data = buffer.getPage(id);
        data             = data.substr(0, data.find('\n'));
        nodeBuffer       = IndexNode::Deserialize(data);
        nodeBuffer.id    = id;
        LOG("loadNode p" << id << " keys=" << nodeBuffer.keys.size());
    }

    /* --- metadados --- */
    int metaGet(const std::string& tag) {
        std::string meta = buffer.getPage(0);
        meta             = meta.substr(0, meta.find('\n'));
        auto p           = meta.find(tag);
        if (p == std::string::npos) return 0;
        auto end = meta.find(';', p);
        if (end == std::string::npos) end = meta.size();
        return std::stoi(meta.substr(p + tag.size(), end - (p + tag.size())));
    }
    int getNextId() { return metaGet("nextId:"); }
    int getRootId() { return metaGet("rootId:"); }

    void setMeta(int nextId, int rootId) {
        writeFixedLine(0, "nextId:" + std::to_string(nextId) +
                           ";rootId:" + std::to_string(rootId));
    }

    /* ajuste para manter compatibilidade com código antigo */
    inline void updateMetadata(int nextId, int rootId) { setMeta(nextId, rootId); }

    void writeNode(const IndexNode& n, bool rewrite) {
        int id = rewrite ? n.id : getNextId();
        writeFixedLine(id, n.Serialize());
        if (!rewrite) setMeta(id + 1, getRootId());
    }

    /* ---------- splits ---------- */
    void splitLeaf(std::stack<int>& path);
    void splitInternal(std::stack<int>& path);

public:
    IndexTree(int ord, const std::string& fp, size_t bufCap = 64)
        : order(ord),
          pageSize(pageSizeForOrder(ord)),
          filePath(fp),
          buffer(fp, pageSizeForOrder(ord), bufCap) {

        std::ifstream f(fp, std::ios::binary);
        bool empty = (!f.good()) || (f.peek() == EOF);
        if (empty) {
            nodeBuffer       = IndexNode();
            nodeBuffer.id    = 2;
            nodeBuffer.isLeaf= true;
            setMeta(3, 2);
            writeFixedLine(1, "id;leaf;keys;parent;children;prev;next;refs");
            writeFixedLine(2, nodeBuffer.Serialize());
        }
        LOG("IndexTree ctor ordem=" << order << " pageSize=" << pageSize);
    }

    void Flush() { LOG("Flush() chamado"); buffer.flushAll(); }

    /* ---------- API ---------- */
    void Insert(int key, int refId);
    int  Search(int key, std::vector<int>& outRefs);
    int  GetHeight();
};

/* ------------------------------------------------------------------ */
/* -------------- implementações (sem alterações lógicas) ------------ */

void IndexTree::splitLeaf(std::stack<int>& path) {
    int m = (nodeBuffer.keys.size() + 1) / 2;

    std::vector<int> rightKeys(nodeBuffer.keys.begin() + m, nodeBuffer.keys.end());
    std::vector<int> rightRefs(nodeBuffer.refs.begin() + m, nodeBuffer.refs.end());

    nodeBuffer.keys.resize(m);
    nodeBuffer.refs.resize(m);

    int leftId  = nodeBuffer.id;
    int rightId = getNextId();
    int oldNext = nodeBuffer.next;

    writeNode(nodeBuffer, true);

    IndexNode right;
    right.id     = rightId;
    right.isLeaf = true;
    right.keys   = rightKeys;
    right.refs   = rightRefs;
    right.parent = nodeBuffer.parent;
    right.prev   = leftId;
    right.next   = oldNext;
    writeNode(right, false);

    /* religa irmãos */
    loadNode(leftId);  nodeBuffer.next = rightId; writeNode(nodeBuffer, true);
    if (oldNext >= 0) { loadNode(oldNext); nodeBuffer.prev = rightId; writeNode(nodeBuffer, true); }

    int promoteKey = rightKeys.front();
    if (path.empty()) {                                  // novo root
        IndexNode root;
        int rootId   = getNextId();
        root.id      = rootId;
        root.isLeaf  = false;
        root.keys    = { promoteKey };
        root.children= { leftId, rightId };
        writeNode(root, false);
        updateMetadata(getNextId(), rootId);

        loadNode(leftId);  nodeBuffer.parent = rootId; writeNode(nodeBuffer, true);
        loadNode(rightId); nodeBuffer.parent = rootId; writeNode(nodeBuffer, true);
    } else {                                             // insere no pai
        int pid = path.top(); path.pop();
        loadNode(pid);
        int idx = 0;
        while (idx < (int)nodeBuffer.keys.size() && promoteKey > nodeBuffer.keys[idx]) ++idx;
        nodeBuffer.keys.insert    (nodeBuffer.keys.begin() + idx, promoteKey);
        nodeBuffer.children.insert(nodeBuffer.children.begin() + idx + 1, rightId);
        writeNode(nodeBuffer, true);
        if (nodeBuffer.keys.size() > (size_t)order) splitInternal(path);
    }
}

void IndexTree::splitInternal(std::stack<int>& path) {
    int m          = nodeBuffer.keys.size() / 2;
    int promoteKey = nodeBuffer.keys[m];
    int leftId     = nodeBuffer.id;

    std::vector<int> rightKeys(nodeBuffer.keys.begin() + m + 1, nodeBuffer.keys.end());
    std::vector<int> rightChildren(nodeBuffer.children.begin() + m + 1,
                                   nodeBuffer.children.end());

    nodeBuffer.keys.resize(m);
    nodeBuffer.children.resize(m + 1);
    writeNode(nodeBuffer, true);

    IndexNode right;
    int rightId   = getNextId();
    right.id      = rightId;
    right.isLeaf  = false;
    right.keys    = rightKeys;
    right.children= rightChildren;
    right.parent  = nodeBuffer.parent;
    writeNode(right, false);

    for (int c : rightChildren) { loadNode(c); nodeBuffer.parent = rightId; writeNode(nodeBuffer, true); }

    if (path.empty()) {
        IndexNode root;
        int rootId   = getNextId();
        root.id      = rootId;
        root.isLeaf  = false;
        root.keys    = { promoteKey };
        root.children= { leftId, rightId };
        writeNode(root, false);
        updateMetadata(getNextId(), rootId);

        loadNode(leftId);  nodeBuffer.parent = rootId; writeNode(nodeBuffer, true);
        loadNode(rightId); nodeBuffer.parent = rootId; writeNode(nodeBuffer, true);
    } else {
        int pid = path.top(); path.pop();
        loadNode(pid);
        int idx = 0;
        while (idx < (int)nodeBuffer.keys.size() && promoteKey > nodeBuffer.keys[idx]) ++idx;
        nodeBuffer.keys.insert    (nodeBuffer.keys.begin() + idx, promoteKey);
        nodeBuffer.children.insert(nodeBuffer.children.begin() + idx + 1, rightId);
        writeNode(nodeBuffer, true);
        if (nodeBuffer.keys.size() > (size_t)order) splitInternal(path);
    }
}

void IndexTree::Insert(int key, int refId) {
    std::stack<int> path;
    loadNode(getRootId());
    while (!nodeBuffer.isLeaf) {
        path.push(nodeBuffer.id);
        int i = 0;
        while (i < (int)nodeBuffer.keys.size() && key >= nodeBuffer.keys[i]) ++i;
        loadNode(nodeBuffer.children[i]);
    }
    int i = 0;
    while (i < (int)nodeBuffer.keys.size() && key > nodeBuffer.keys[i]) ++i;
    nodeBuffer.keys.insert(nodeBuffer.keys.begin() + i, key);
    nodeBuffer.refs.insert(nodeBuffer.refs.begin() + i, refId);

    if (nodeBuffer.keys.size() > (size_t)order) splitLeaf(path);
    else writeNode(nodeBuffer, true);
}

int IndexTree::Search(int key, std::vector<int>& outRefs) {
    loadNode(getRootId());
    bool done = false;
    while (!nodeBuffer.isLeaf) {
        int i = 0;
        while (i < (int)nodeBuffer.keys.size() && key > nodeBuffer.keys[i]) ++i;
        loadNode(nodeBuffer.children[i]);
    }
    while (!done) {
        for (size_t i = 0; i < nodeBuffer.keys.size(); ++i) {
            if (nodeBuffer.keys[i] == key) outRefs.push_back(nodeBuffer.refs[i]);
            else if (nodeBuffer.keys[i] > key) done = true;
        }
        if (nodeBuffer.next < 0) break;
        loadNode(nodeBuffer.next);
    }
    return static_cast<int>(outRefs.size());
}

int IndexTree::GetHeight() {
    loadNode(getRootId());
    int h = 0;
    while (!nodeBuffer.isLeaf) { loadNode(nodeBuffer.children[0]); ++h; }
    return h;
}

#endif // INDEXTREE_H
