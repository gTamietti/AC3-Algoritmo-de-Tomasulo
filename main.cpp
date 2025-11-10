#include "simulator.h"

int main(int argc, char* argv[]) {
    // Verifica se o nome do arquivo de instrucoes foi fornecido
    if (argc != 2) {
        std::cerr << "Uso: " << argv[0] << " <arquivo_de_instrucoes.txt>" << std::endl;
        return 1;
    }

    // Cria e inicializa o simulador
    Simulator sim;

    // Carrega as instrucoes do arquivo fornecido
    if (!sim.load_instructions(argv[1])) {
        return 1;
    }

    // Inicia a simulacao
    sim.run();

    return 0;
}