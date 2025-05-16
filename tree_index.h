#ifndef INDEXTREE_H
#define INDEXTREE_H

#include "node_index.h"
#include <string>
#include <stack>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <sstream>

class IndexTree {
private:
    int order;
    int pageSize;
    std::string filePath;
    IndexNode nodeBuffer;

    int setPageSize(int ord) const {
        return 37 + 12 * ord;
    }

    void writeFixedLine(int line, const std::string &content) {
        std::fstream fs(filePath, std::ios::in | std::ios::out | std::ios::binary);
        if (!fs) throw std::runtime_error("Cannot open index file");
        std::string buf = content;
        if ((int)buf.size() > pageSize - 1) throw std::invalid_argument("Line too long");
        buf.resize(pageSize - 1, ' ');
        buf.push_back('\n');
        fs.seekp((std::streamoff)line * pageSize);
        fs.write(buf.data(), pageSize);
    }

    void loadNode(int id) {
        std::ifstream fs(filePath, std::ios::binary);
        if (!fs) throw std::runtime_error("Cannot open index file");
        fs.seekg((std::streamoff)id * pageSize);
        std::string buf(pageSize, '\0');
        fs.read(&buf[0], pageSize);
        size_t pos = buf.find('\n');
        if (pos != std::string::npos) buf.erase(pos);
        nodeBuffer = IndexNode::Deserialize(buf);
        nodeBuffer.id = id;
    }

    int getNextId() const {
        std::ifstream fs(filePath, std::ios::binary);
        fs.seekg(0);
        std::string buf(pageSize, '\0');
        fs.read(&buf[0], pageSize);
        auto p = buf.find("nextId:");
        if (p == std::string::npos) return 0;
        return std::stoi(buf.substr(p + 7));
    }

    int getRootId() const {
        std::ifstream fs(filePath, std::ios::binary);
        fs.seekg(0);
        std::string buf(pageSize, '\0');
        fs.read(&buf[0], pageSize);
        auto p = buf.find("rootId:");
        if (p == std::string::npos) return 0;
        return std::stoi(buf.substr(p + 7));
    }

    void updateMetadata(int nextId, int rootId) {
        writeFixedLine(0, "nextId:" + std::to_string(nextId) + ";rootId:" + std::to_string(rootId));
    }

    void writeNode(const IndexNode &node, bool rewrite) {
        int id = rewrite ? node.id : getNextId();
        writeFixedLine(id, node.Serialize());
        if (!rewrite) {
            int newNext = id + 1;
            int root = getRootId();
            updateMetadata(newNext, root);
        }
    }

    void splitLeaf(std::stack<int> &path) {
        // split current leaf in nodeBuffer
        int m = (nodeBuffer.keys.size() + 1) / 2;
        std::vector<int> rightKeys(nodeBuffer.keys.begin() + m, nodeBuffer.keys.end());
        std::vector<int> rightRefs(nodeBuffer.refs.begin() + m, nodeBuffer.refs.end());
        // shrink left
        nodeBuffer.keys.resize(m);
        nodeBuffer.refs.resize(m);
        int leftId = nodeBuffer.id;
        int rightId = getNextId();
        int oldNext = nodeBuffer.next;

        // write left
        writeNode(nodeBuffer, true);

        // prepare right node
        IndexNode right;
        right.id = rightId;
        right.isLeaf = true;
        right.keys = rightKeys;
        right.refs = rightRefs;
        right.parent = nodeBuffer.parent;
        right.prev = leftId;
        right.next = oldNext;
        writeNode(right, false);

        // link siblings
        loadNode(leftId);
        nodeBuffer.next = rightId;
        writeNode(nodeBuffer, true);
        if (oldNext >= 0) {
            loadNode(oldNext);
            nodeBuffer.prev = rightId;
            writeNode(nodeBuffer, true);
        }

        // promote
        int promoteKey = rightKeys.front();
        if (path.empty()) {
            // new root
            IndexNode root;
            int rootId = getNextId();
            root.id = rootId;
            root.isLeaf = false;
            root.keys = {promoteKey};
            root.children = {leftId, rightId};
            root.parent = -1;
            writeNode(root, false);
            updateMetadata(getNextId(), rootId);
            // set children parent
            loadNode(leftId); nodeBuffer.parent = rootId; writeNode(nodeBuffer, true);
            loadNode(rightId); nodeBuffer.parent = rootId; writeNode(nodeBuffer, true);
        } else {
            int pid = path.top(); path.pop();
            loadNode(pid);
            int idx = 0;
            while (idx < (int)nodeBuffer.keys.size() && promoteKey > nodeBuffer.keys[idx]) ++idx;
            nodeBuffer.keys.insert(nodeBuffer.keys.begin() + idx, promoteKey);
            nodeBuffer.children.insert(nodeBuffer.children.begin() + idx + 1, rightId);
            writeNode(nodeBuffer, true);
            if (nodeBuffer.keys.size() > (size_t)order) splitInternal(path);
        }
    }

    void splitInternal(std::stack<int> &path) {
        // similar logic for internal nodes
        int m = nodeBuffer.keys.size() / 2;
        int promoteKey = nodeBuffer.keys[m];
        int leftId = nodeBuffer.id;
        std::vector<int> rightKeys(nodeBuffer.keys.begin() + m + 1, nodeBuffer.keys.end());
        std::vector<int> rightChildren(nodeBuffer.children.begin() + m + 1, nodeBuffer.children.end());
        nodeBuffer.keys.resize(m);
        nodeBuffer.children.resize(m + 1);

        writeNode(nodeBuffer, true);

        IndexNode right;
        int rightId = getNextId();
        right.id = rightId;
        right.isLeaf = false;
        right.keys = rightKeys;
        right.children = rightChildren;
        right.parent = nodeBuffer.parent;
        writeNode(right, false);

        for (int c : rightChildren) {
            loadNode(c);
            nodeBuffer.parent = rightId;
            writeNode(nodeBuffer, true);
        }

        if (path.empty()) {
            IndexNode root;
            int rootId = getNextId();
            root.id = rootId;
            root.isLeaf = false;
            root.keys = {promoteKey};
            root.children = {leftId, rightId};
            root.parent = -1;
            writeNode(root, false);
            updateMetadata(getNextId(), rootId);
            loadNode(leftId); nodeBuffer.parent = rootId; writeNode(nodeBuffer, true);
            loadNode(rightId); nodeBuffer.parent = rootId; writeNode(nodeBuffer, true);
        } else {
            int pid = path.top(); path.pop();
            loadNode(pid);
            int idx = 0;
            while (idx < (int)nodeBuffer.keys.size() && promoteKey > nodeBuffer.keys[idx]) ++idx;
            nodeBuffer.keys.insert(nodeBuffer.keys.begin() + idx, promoteKey);
            nodeBuffer.children.insert(nodeBuffer.children.begin() + idx + 1, rightId);
            writeNode(nodeBuffer, true);
            if (nodeBuffer.keys.size() > (size_t)order) splitInternal(path);
        }
    }

public:
    IndexTree(int ord, const std::string &fp)
        : order(ord), pageSize(setPageSize(ord)), filePath(fp) {
        std::ifstream f(fp, std::ios::binary);
        if (!f.good() || f.peek() == EOF) {
            std::ofstream o(fp, std::ios::binary | std::ios::trunc);
            o.close();
            nodeBuffer = IndexNode(); nodeBuffer.id = 2; nodeBuffer.isLeaf = true;
            updateMetadata(3, 2);
            writeFixedLine(1, "id;leaf;keys;parent;children;prev;next;refs");
            writeFixedLine(2, nodeBuffer.Serialize());
        }
    }

    void Insert(int key, int refId) {
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

    int Search(int key, std::vector<int> &outRefs) {
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
        return (int)outRefs.size();
    }

    int GetHeight() {
        loadNode(getRootId());
        int height = 0;
        while (!nodeBuffer.isLeaf) {
            loadNode(nodeBuffer.children[0]);
            ++height;
        }
        return height;
    }
};

#endif // INDEXTREE_H
