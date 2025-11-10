#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>

// --- Constantes de Configuracao ---
#define ADD_RS_COUNT 3 // Numero de Estacoes de Reserva para ADD/SUB
#define MUL_RS_COUNT 2 // Numero de Estacoes de Reserva para MUL/DIV

// Latencias das Unidades Funcionais (em ciclos)
#define ADD_LATENCY 2
#define MUL_LATENCY 10
#define DIV_LATENCY 40

// --- Estruturas de Dados ---

// Representa uma instrucao (OP DEST SRC1 SRC2)
struct Instruction {
    std::string op;
    std::string dest;
    std::string src1;
    std::string src2;
};

// Representa uma entrada na Estacao de Reserva (RS)
struct RS_Entry {
    std::string name; // Nome da RS (ex: "Add1", "Mult2")
    bool busy = false; // Estado: ocupada ou livre
    std::string op = ""; // Operacao (ex: "ADD", "MUL")

    // Campos dos Operandos (Vj, Vk ou Qj, Qk)
    double vj = 0.0; // Valor do operando J (se estiver pronto)
    double vk = 0.0; // Valor do operando K (se estiver pronto)
    std::string qj = ""; // RS que produzira J (se nao estiver pronto)
    std::string qk = ""; // RS que produzira K (se nao estiver pronto)

    // Controle de Execucao
    int cycles_remaining = -1; // Ciclos restantes para execucao. -1 = nao iniciada
    double result = 0.0; // Resultado da operacao (apos a execucao)
    bool ready_to_writeback = false; // Sinaliza que a execucao terminou e esta pronta para o CDB
};

class Simulator {
private:
    int cycle; // Contador de ciclos
    int pc; // Program Counter (indice da proxima instrucao a emitir)
    bool simulation_complete;

    // Estruturas de Tomasulo
    std::vector<Instruction> instruction_queue; // Fila de instrucoes (programa)
    std::map<std::string, RS_Entry> add_rs; // Estacoes de Reserva de Adicao/Subtracao
    std::map<std::string, RS_Entry> mul_rs; // Estacoes de Reserva de Multiplicacao/Divisao
    std::map<std::string, double> reg_file; // Banco de Registradores (valores F0, F1, ...)
    std::map<std::string, std::string> reg_status; // Status dos Registradores (Qi: quem produz o valor)

    // Estagios do Pipeline
    void issue();
    void execute();
    void writeback();
    bool check_completion();

    // Funcoes de Impressao
    void print_state();
    void print_final_registers();

public:
    Simulator();
    bool load_instructions(const std::string& filename);
    void run();
};

#endif // SIMULATOR_H