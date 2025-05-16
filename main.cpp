#include "input_manager.h"

int main() {
    InputManager manager("in.txt", "out.txt", "index.txt", "vinhos.csv");
    manager.ProcessInput();
    return 0;
}
