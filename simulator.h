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
#include <cstdlib>

// --- Constantes de Configuracao ---
#define ADD_RS_COUNT 3 
#define MUL_RS_COUNT 2 
#define LS_COUNT 2     

// Latencias das Unidades Funcionais (em ciclos)
#define ADD_LATENCY 2
#define MUL_LATENCY 10
#define DIV_LATENCY 40
#define MEM_ACCESS_LATENCY 3 

// --- NOVOS ENUMS E ESTRUTURAS DE DADOS ---

// Tipo de Operacao (usamos string para flexibilidade)
using OpType = std::string; 

// Estado da instrucao (Rob Entry State)
enum InstrState {
    NOT_ISSUED,
    ISSUED, 
    EXECUTING, 
    WRITE_RESULT, // Instrucao completou a execucao, pronta para o Commit (Writeback ocorreu)
    COMMITTED
};

// Representa uma instrucao COMPLETA (incluindo estados e tempos)
struct Instruction {
    int id; // ID sequencial (indice na fila)
    OpType op;
    std::string dest; 
    std::string src1; 
    std::string src2; 
    long address; // Para LOAD/STORE
    
    InstrState state; // Estado atual
    int issue_cycle;
    int exec_start_cycle;
    int exec_end_cycle;
    int write_cycle;
    int commit_cycle;
    
    Instruction() : id(-1), op(""), dest(""), src1(""), src2(""), address(-1), 
                    state(NOT_ISSUED), issue_cycle(0), exec_start_cycle(0), 
                    exec_end_cycle(0), write_cycle(0), commit_cycle(0) {}
};

// Representa uma entrada na Estacao de Reserva (RS)
struct RS_Entry {
    std::string name; 
    bool busy = false; 
    std::string op = ""; 
    double vj = 0.0; 
    double vk = 0.0; 
    std::string qj = ""; 
    std::string qk = ""; 
    int instruction_id = -1; // ID da instrucao sendo executada
    int cycles_remaining = -1; 
    double result = 0.0; 
    bool ready_to_writeback = false; // Sinaliza que o resultado esta no CDB
};

// Estrutura para o Buffer de Load/Store (LS)
struct LS_Entry {
    std::string name;
    bool busy = false;
    std::string op = ""; 
    std::string dest_reg = ""; 
    std::string base_reg = ""; 

    double base_value = 0.0; 
    std::string base_producer = ""; 
    int offset = 0; 
    long calculated_address = -1; 
    bool address_ready = false; 

    double store_value = 0.0; 
    std::string store_producer = ""; 

    int instruction_id = -1; // ID da instrucao sendo executada

    int cycles_remaining = -1; 
    double result = 0.0; 
    bool ready_to_writeback = false;
};


class Simulator {
private:
    int cycle; 
    int pc; // Ponteiro para a proxima instrucao a ser emitida
    bool simulation_complete;
    int committed_inst_count; // Proxima instrucao a ser Commited (in-order)

    // Fila de instrucoes (Funciona como o ROB/Tabela de Status)
    std::vector<Instruction> instruction_queue; 
    std::map<std::string, RS_Entry> add_rs; 
    std::map<std::string, RS_Entry> mul_rs; 
    std::map<std::string, LS_Entry> ls_rs; 
    
    std::map<std::string, double> reg_file; 
    std::map<std::string, std::string> reg_status; 
    
    std::map<long, double> memory; 

    // Estagios do Pipeline (Ordem sugerida: COMMIT -> WB -> EXEC -> ISSUE)
    void commit();
    void issue();
    void execute();
    void writeback();
    bool check_completion();

    // Funcoes de Impressao
    void print_state();
    void print_final_registers();
    std::string get_register_name(int index); 
    void print_memory_state(); 
    void print_instruction_status(); 
    
    // Funcoes de ajuda para Load/Store
    LS_Entry* find_address_hazard(long address, const std::string& current_name); 

public:
    Simulator();
    bool load_instructions(const std::string& filename);
    void run();
};

#endif // SIMULATOR_H