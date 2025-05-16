#include "CommandParser.h"
#include <fstream>
#include <string>

// Função trim: remove espaços, tabs, CR e LF de ambos os lados
static void trim(std::string &s) {
    const char* whitespace = " \t\r\n";

    size_t start = s.find_first_not_of(whitespace);
    if (start == std::string::npos) { s.clear(); return; }
    size_t end = s.find_last_not_of(whitespace);
    s = s.substr(start, end - start + 1);
}

Result<std::vector<Command>, ParseError> parseCommands(const std::string& path, int& degree) {
    std::ifstream f(path);
    if (!f) return Result<std::vector<Command>,ParseError>::Err({"in.txt não encontrado", 0});
    std::string line;
    if (!std::getline(f, line))
        return Result<std::vector<Command>,ParseError>::Err({"Arquivo vazio", 0});

    trim(line);
    if (line.rfind("FLH/", 0) != 0)
        return Result<std::vector<Command>,ParseError>::Err({"Header inválido", 0});

    std::string dstr = line.substr(4);
    trim(dstr);
    try {
        degree = std::stoi(dstr);
    } catch (...) {
        return Result<std::vector<Command>,ParseError>::Err({"Valor de grau inválido", 0});
    }

    std::vector<Command> cmds;
    while (std::getline(f, line)) {
        trim(line);
        if (line.empty()) continue;

        if (line.rfind("INC:", 0) == 0) {
            std::string num = line.substr(4);
            trim(num);
            try {
                cmds.push_back({true, std::stoi(num)});
            } catch (...) {
                return Result<std::vector<Command>,ParseError>::Err({"Número inválido em INC", 0});
            }
        } else if (line.rfind("BUS=:", 0) == 0) {
            std::string num = line.substr(5);
            trim(num);
            try {
                cmds.push_back({false, std::stoi(num)});
            } catch (...) {
                return Result<std::vector<Command>,ParseError>::Err({"Número inválido em BUS", 0});
            }
        } else {
            return Result<std::vector<Command>,ParseError>::Err({"Comando desconhecido: " + line, 0});
        }
    }

    return Result<std::vector<Command>,ParseError>::Ok(cmds);
}