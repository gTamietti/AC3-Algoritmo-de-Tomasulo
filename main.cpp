#include "simulator.h"
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Uso: " << argv[0] << " <arquivo_de_instrucoes>" << std::endl;
        std::cerr << "Ex: " << argv[0] << " instructions.txt" << std::endl;
        return EXIT_FAILURE;
    }

    Simulator sim;
    
    // Carrega instrucoes
    if (!sim.load_instructions(argv[1])) {
        return EXIT_FAILURE;
    }

    // Executa a simulacao
    sim.run();

    return EXIT_SUCCESS;
}