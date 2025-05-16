#pragma once
#include <vector>
#include <string>
#include "Result.h"
#include "Model.h"

struct Command { bool isInsert; int key; };

Result<std::vector<Command>, ParseError> parseCommands(const std::string& path, int& degree);