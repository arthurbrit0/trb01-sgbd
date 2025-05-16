#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include "FileManager.h"
#include "BufferManager.h"
#include "BPlusTree.h"
#include "CommandParser.h"
#include "Model.h"

int main() {
    const std::string dataFile = "vinos.csv", idxFile = "index.txt";
    const std::string inputFile = "in.txt";
    const std::string outputFile = "out.txt";

    int degree;
    auto pc = parseCommands(inputFile, degree);
    if (!pc.success) {
        std::cerr << "Erro ao parsear comandos: " << pc.error.msg << " na linha " << pc.error.line << std::endl;
        return 1;
    }
    auto cmds = pc.value;

    FileManager fm(dataFile, idxFile);
    BufferManager bm(fm);
    BPlusTree tree(degree, fm, bm);

    BPTError bpt_error;

    std::ifstream csv_file_stream(dataFile);
    std::string line;
    
    if (!csv_file_stream.is_open()) {
        std::cerr << "Erro ao abrir o arquivo de dados: " << dataFile << std::endl;
        return 1;
    }

    std::getline(csv_file_stream, line);
    int current_record_id = 0;

    while (std::getline(csv_file_stream, line)) {
        std::stringstream ss(line);
        WineRecord w;
        std::string temp_field;

        if (!std::getline(ss, temp_field, ',')) continue;
        try {
            w.wineId = std::stoi(temp_field);
        } catch (const std::invalid_argument& ia) {
            std::cerr << "Aviso: Falha ao converter wineId '" << temp_field << "' na linha de dados " << current_record_id << ". Pulando registro." << std::endl;
            current_record_id++;
            continue;
        }

        if (!std::getline(ss, w.label, ',')) continue;
        
        if (!std::getline(ss, temp_field, ',')) continue;
        try {
            w.harvestYear = std::stoi(temp_field);
        } catch (const std::invalid_argument& ia) {
            std::cerr << "Aviso: Falha ao converter harvestYear '" << temp_field << "' na linha de dados " << current_record_id << ". Pulando registro." << std::endl;
            current_record_id++;
            continue;
        }
        
        if (!std::getline(ss, w.type)) continue;

        RID rid{current_record_id, 0};
        tree.insert(w.harvestYear, rid, bpt_error);
        if (!bpt_error.msg.empty()) {
            std::cerr << "Erro ao inserir na B+Tree durante população inicial (chave " << w.harvestYear << "): " << bpt_error.msg << std::endl;
            bpt_error.msg.clear();
        }
        current_record_id++;
    }
    csv_file_stream.close();

    std::ofstream out_stream(outputFile);
    if (!out_stream.is_open()) {
        std::cerr << "Erro ao abrir o arquivo de saída: " << outputFile << std::endl;
        return 1;
    }

    out_stream << "FLH/" << degree << "\n";

    for (const auto& cmd : cmds) {
        if (cmd.isInsert) {
            WineRecord new_wine;
            new_wine.harvestYear = cmd.key;
            new_wine.wineId = -1;
            new_wine.label = "Vinho Adicionado";
            new_wine.type = "N/A";

            auto append_result = fm.appendDataRecord(new_wine);
            int tuples_inserted_count = 0;

            if (append_result.success) {
                RID rid{append_result.value, 0};
                tree.insert(new_wine.harvestYear, rid, bpt_error);
                if (bpt_error.msg.empty()) {
                    tuples_inserted_count = 1;
                } else {
                    std::cerr << "Erro ao inserir na B+Tree (INC:" << cmd.key << "): " << bpt_error.msg << std::endl;
                    bpt_error.msg.clear();
                }
            } else {
                std::cerr << "Erro ao anexar registro de dados (INC:" << cmd.key << "): " << append_result.error.msg << std::endl;
            }
            out_stream << "INC:" << cmd.key << "/" << tuples_inserted_count << "\n";
        } else {
            auto search_results = tree.search(cmd.key, bpt_error);
            if (!bpt_error.msg.empty()) {
                 std::cerr << "Erro ao buscar na B+Tree (BUS=:" << cmd.key << "): " << bpt_error.msg << std::endl;
                 bpt_error.msg.clear();
            }
            out_stream << "BUS=:" << cmd.key << "/" << search_results.size() << "\n";
        }
    }

    out_stream << "H/" << tree.getHeight() << "\n";
    out_stream.close();

    return 0;
}