#include "simulator.h"
#include <iomanip> // Para std::setw
#include <algorithm> // Para std::max

// --- Construtor ---
Simulator::Simulator() : cycle(0), pc(0), simulation_complete(false) {
    // Inicializa as estacoes de reserva de Adicao/Subtracao
    for (int i = 1; i <= ADD_RS_COUNT; ++i) {
        std::string name = "Add" + std::to_string(i);
        add_rs[name] = RS_Entry();
        add_rs[name].name = name;
    }

    // Inicializa as estacoes de reserva de Multiplicacao/Divisao
    for (int i = 1; i <= MUL_RS_COUNT; ++i) {
        std::string name = "Mult" + std::to_string(i);
        mul_rs[name] = RS_Entry();
        mul_rs[name].name = name;
    }

    // Inicializa registradores com valores iniciais para testes
    // Usando valores mais simples e notáveis
    reg_file["F0"] = 10.0;
    reg_file["F1"] = 1.0;
    reg_file["F2"] = 2.0;
    reg_file["F3"] = 3.0;
    reg_file["F4"] = 4.0;
    reg_file["F5"] = 5.0;
    reg_file["F6"] = 6.0;
    reg_file["F7"] = 7.0;
    reg_file["F8"] = 8.0;

    // RegStatus comeca limpo (todos os valores estao no reg_file)
    for (auto const& pair : reg_file) {
        reg_status[pair.first] = "";
    }
}

// --- Carregamento de Instrucoes ---
bool Simulator::load_instructions(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Erro: Nao foi possivel abrir o arquivo: " << filename << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; // Ignora linhas vazias ou comentarios

        std::stringstream ss(line);
        Instruction inst;

        // Formato esperado: OP DEST SRC1 SRC2
        ss >> inst.op;
        ss >> inst.dest;
        ss >> inst.src1;
        ss >> inst.src2;

        instruction_queue.push_back(inst);
    }
    file.close();
    return true;
}

// --- Loop Principal da Simulacao ---
void Simulator::run() {
    std::cout << "Iniciando Simulacao de Tomasulo..." << std::endl;
    std::cout << "Configuracao: Add/Sub Latencia=" << ADD_LATENCY << " | Mul/Div Latencia=" << MUL_LATENCY << "/" << DIV_LATENCY << std::endl;
    std::cout << "Valores Iniciais dos Registradores:" << std::endl;
    print_final_registers();
    std::cout << "Estado Inicial (Ciclo 0):" << std::endl;
    print_state();

    while (!simulation_complete) {
        cycle++;
        std::cout << "\n--- CICLO " << cycle << " ---" << std::endl;

        // Ordem dos estagios: WB -> EXEC -> ISSUE
        writeback();
        execute();
        issue();

        print_state();

        // Verifica condicao de termino
        if (check_completion()) {
            simulation_complete = true;
        }

        // Medida de seguranca para evitar loop infinito
        if (cycle > 200) {
            std::cout << "Simulacao interrompida (limite de ciclos atingido)." << std::endl;
            break;
        }
    }

    std::cout << "\n--- Simulacao Concluida em " << cycle << " Ciclos ---" << std::endl;
    print_final_registers();
}

// --- Estagio de Emissao (Issue) ---
void Simulator::issue() {
    if (pc >= instruction_queue.size()) {
        return;
    }

    Instruction inst = instruction_queue[pc];
    std::map<std::string, RS_Entry>* target_rs_map = nullptr;

    // 1. Encontrar o tipo de RS
    if (inst.op == "ADD" || inst.op == "SUB") {
        target_rs_map = &add_rs;
    } else if (inst.op == "MUL" || inst.op == "DIV") {
        target_rs_map = &mul_rs;
    } else {
        std::cout << "  [ISSUE] Ignorando OP desconhecido: " << inst.op << std::endl;
        pc++; 
        return;
    }

    // 2. Encontrar uma RS livre
    std::string free_rs_name = "";
    for (auto& pair : *target_rs_map) {
        if (!pair.second.busy) {
            free_rs_name = pair.first;
            break;
        }
    }

    // 3. Se nao houver RS livre (perigo estrutural), parar.
    if (free_rs_name == "") {
        std::cout << "  [ISSUE] Parado: Perigo Estrutural em " << inst.op << " (Sem RS Livre)" << std::endl;
        return; 
    }

    std::cout << "  [ISSUE] Emitindo (" << pc+1 << "/" << instruction_queue.size() << ") " << inst.op << " " << inst.dest << "," << inst.src1 << "," << inst.src2 << " para " << free_rs_name << std::endl;

    // 4. Preencher a RS
    RS_Entry& rs = (*target_rs_map)[free_rs_name];
    rs.busy = true;
    rs.op = inst.op;
    rs.cycles_remaining = -1; // Sinaliza que a execucao nao comecou
    rs.ready_to_writeback = false;
    rs.result = 0.0;

    // 5. Verificar e preencher Vj/Qj (para src1)
    if (reg_status[inst.src1] == "") {
        rs.vj = reg_file[inst.src1];
        rs.qj = "";
    } else {
        rs.vj = 0.0; 
        rs.qj = reg_status[inst.src1];
    }

    // 6. Verificar e preencher Vk/Qk (para src2)
    if (reg_status[inst.src2] == "") {
        rs.vk = reg_file[inst.src2];
        rs.qk = "";
    } else {
        rs.vj = 0.0; 
        rs.qk = reg_status[inst.src2];
    }

    // 7. Renomear o registrador de destino (Atualiza Qi)
    reg_status[inst.dest] = free_rs_name;

    // 8. Avancar o PC
    pc++;
}

// --- Estagio de Execucao (Execute) ---
void Simulator::execute() {
    // Funcao Lambda para processar um mapa de RS (evita duplicacao de codigo)
    auto process_rs_map = [&](std::map<std::string, RS_Entry>& rs_map) {
        for (auto& pair : rs_map) {
            RS_Entry& rs = pair.second;
            
            // So executa se: 1. Ocupada. 2. Operandos prontos. 3. Nao terminou a execucao.
            if (rs.busy && rs.qj == "" && rs.qk == "" && !rs.ready_to_writeback) {
                
                // 1. Iniciar execucao (se ainda nao comecou)
                if (rs.cycles_remaining == -1) {
                    int latency = 0;
                    if (rs.op == "ADD" || rs.op == "SUB") {
                        latency = ADD_LATENCY;
                    } else if (rs.op == "MUL") {
                        latency = MUL_LATENCY;
                    } else if (rs.op == "DIV") {
                        latency = DIV_LATENCY;
                    }
                    rs.cycles_remaining = latency;
                    std::cout << "  [EXEC] Iniciando " << rs.name << " (" << rs.op << ") | Latencia: " << latency << std::endl;
                }
                
                // 2. Decrementar contador
                if (rs.cycles_remaining > 0) {
                    rs.cycles_remaining--;
                }

                // 3. Execucao concluida
                if (rs.cycles_remaining == 0) {
                    // Calcular o resultado
                    if (rs.op == "ADD") rs.result = rs.vj + rs.vk;
                    if (rs.op == "SUB") rs.result = rs.vj - rs.vk;
                    if (rs.op == "MUL") rs.result = rs.vj * rs.vk;
                    if (rs.op == "DIV") {
                        if (rs.vk == 0) {
                            std::cerr << "  [EXEC] ERRO: Divisao por zero em " << rs.name << "! Resultado 0.0" << std::endl;
                            rs.result = 0.0; 
                        }
                        else rs.result = rs.vj / rs.vk;
                    }
                    
                    std::cout << "  [EXEC] Concluindo " << rs.name << " | Resultado: " << rs.result << std::endl;
                    rs.ready_to_writeback = true;
                    // Nao setar cycles_remaining = -1 aqui, pois o proximo ciclo de WB vai limpar a RS.
                    // Isso evita que a execucao seja iniciada novamente antes do WB.
                }
            }
        }
    };

    process_rs_map(add_rs);
    process_rs_map(mul_rs);
}

// --- Estagio de Escrita (Writeback) ---
void Simulator::writeback() {
    std::string broadcasting_rs_name = "";
    double broadcasting_result = 0.0;
    RS_Entry* rs_to_clear = nullptr; // Ponteiro para a RS que vai escrever

    // 1. Encontrar UMA RS pronta para escrever (arbitragem: a primeira encontrada)

    // Busca nas RS de Adicao
    for (auto& pair : add_rs) {
        if (pair.second.ready_to_writeback) {
            broadcasting_rs_name = pair.first;
            broadcasting_result = pair.second.result;
            rs_to_clear = &pair.second;
            break;
        }
    }

    // Se nao achou nas de Adicao, busca nas de Multiplicacao
    if (broadcasting_rs_name.empty()) {
        for (auto& pair : mul_rs) {
            if (pair.second.ready_to_writeback) {
                broadcasting_rs_name = pair.first;
                broadcasting_result = pair.second.result;
                rs_to_clear = &pair.second;
                break;
            }
        }
    }

    // 2. Se ninguem esta transmitindo, nao faz nada
    if (broadcasting_rs_name.empty()) {
        return;
    }

    std::cout << "  [WB] " << broadcasting_rs_name << " transmitindo resultado " << broadcasting_result << std::endl;

    // 3. Atualizar o banco de registradores e o status (Qi)
    for (auto& pair : reg_status) {
        std::string reg = pair.first;
        std::string& status = pair.second;
        // CRITICO: Apenas escreve no registrador se o produtor ainda for esta RS
        if (status == broadcasting_rs_name) {
            reg_file[reg] = broadcasting_result;
            status = ""; // Registrador esta pronto
        }
    }

    // 4. Transmitir para todas as outras RSs em espera (CDB)
    auto update_waiting_rs = [&](std::map<std::string, RS_Entry>& rs_map) {
        for (auto& pair : rs_map) {
            RS_Entry& rs = pair.second;
            // Nao atualiza RS que terminou, mas pode atualizar RS que esta em execucao mas espera um operando
            if (rs.busy && !rs.ready_to_writeback) { 
                if (rs.qj == broadcasting_rs_name) {
                    rs.vj = broadcasting_result;
                    rs.qj = ""; // Sinaliza que o operando esta pronto
                    std::cout << "  [WB] CDB: " << rs.name << " (Vj) atualizada por " << broadcasting_rs_name << std::endl;
                }
                if (rs.qk == broadcasting_rs_name) {
                    rs.vk = broadcasting_result;
                    rs.qk = ""; // Sinaliza que o operando esta pronto
                    std::cout << "  [WB] CDB: " << rs.name << " (Vk) atualizada por " << broadcasting_rs_name << std::endl;
                }
            }
        }
    };
    update_waiting_rs(add_rs);
    update_waiting_rs(mul_rs);

    // 5. Limpar a RS que transmitiu
    if (rs_to_clear) {
        rs_to_clear->busy = false;
        rs_to_clear->op = "";
        rs_to_clear->vj = 0.0;
        rs_to_clear->vk = 0.0;
        rs_to_clear->qj = "";
        rs_to_clear->qk = "";
        rs_to_clear->cycles_remaining = -1;
        rs_to_clear->ready_to_writeback = false;
        rs_to_clear->result = 0.0; 
    }
}


// --- Funcoes Auxiliares ---

bool Simulator::check_completion() {
    // Se o PC nao chegou ao fim da fila
    if (pc < instruction_queue.size()) {
        return false;
    }

    // Se alguma RS ainda estiver ocupada
    for (auto const& pair : add_rs) {
        if (pair.second.busy) return false;
    }
    for (auto const& pair : mul_rs) {
        if (pair.second.busy) return false;
    }

    return true;
}

void Simulator::print_state() {
    // Configura o formato de impressao para float
    std::cout << std::fixed << std::setprecision(2); 

    // --- Tabela de Estacoes de Reserva ---
    std::cout << "\n  --- Estacoes de Reserva (Janela de Instrucoes) ---" << std::endl;
    std::cout << "    " << std::setw(6) << "Nome" << " | " << std::setw(4) << "Busy" << " | " << std::setw(4) << "Op" << " | " << std::setw(8) << "Vj" << " | " << std::setw(8) << "Vk" << " | " << std::setw(6) << "Qj" << " | " << std::setw(6) << "Qk" << " | " << "Ciclos" << std::endl;
    std::cout << "    " << std::string(67, '-') << std::endl;
    
    // Processa RS de Adicao
    for (auto const& pair : add_rs) {
        const std::string& name = pair.first;
        const RS_Entry& rs = pair.second;
        
        // Imprime valores de Vj/Vk apenas se Qj/Qk estiverem vazios
        std::string vj_str = rs.qj.empty() && rs.busy ? std::to_string(rs.vj) : "";
        std::string vk_str = rs.qk.empty() && rs.busy ? std::to_string(rs.vk) : "";
        
        std::string cycles_str = "-";
        if (rs.ready_to_writeback) cycles_str = "WB";
        else if (rs.cycles_remaining >= 0) cycles_str = std::to_string(rs.cycles_remaining);
        
        std::string busy_str = rs.busy ? "Sim" : "Nao";

        std::cout << "    " << std::setw(6) << name << " | " << std::setw(4) << busy_str << " | " << std::setw(4) << rs.op << " | "
                  << std::setw(8) << vj_str << " | "
                  << std::setw(8) << vk_str << " | "
                  << std::setw(6) << rs.qj << " | " << std::setw(6) << rs.qk << " | "
                  << std::setw(6) << cycles_str << std::endl;
    }

    // Processa RS de Multiplicacao
    for (auto const& pair : mul_rs) {
        const std::string& name = pair.first;
        const RS_Entry& rs = pair.second;
        
        std::string vj_str = rs.qj.empty() && rs.busy ? std::to_string(rs.vj) : "";
        std::string vk_str = rs.qk.empty() && rs.busy ? std::to_string(rs.vk) : "";

        std::string cycles_str = "-";
        if (rs.ready_to_writeback) cycles_str = "WB";
        else if (rs.cycles_remaining >= 0) cycles_str = std::to_string(rs.cycles_remaining);
        
        std::string busy_str = rs.busy ? "Sim" : "Nao";

        std::cout << "    " << std::setw(6) << name << " | " << std::setw(4) << busy_str << " | " << std::setw(4) << rs.op << " | "
                  << std::setw(8) << vj_str << " | "
                  << std::setw(8) << vk_str << " | "
                  << std::setw(6) << rs.qj << " | " << std::setw(6) << rs.qk << " | "
                  << std::setw(6) << cycles_str << std::endl;
    }

    // --- Tabela de Status dos Registradores (Qi) ---
    std::cout << "\n  --- Status dos Registradores (Qi) ---" << std::endl;
    std::cout << "   ";
    // Determina a largura da coluna baseada no maior nome de RS (ex: Mult10)
    size_t col_width = 8; 
    
    // Imprime o cabecalho
    for (auto const& pair : reg_status) {
        std::cout << " | " << std::setw(std::max(col_width, pair.first.length())) << pair.first;
    }
    std::cout << " |" << std::endl;
    
    // Imprime os valores
    std::cout << "   ";
    for (auto const& pair : reg_status) {
        const std::string& status = pair.second;
        std::cout << " | " << std::setw(std::max(col_width, pair.first.length())) << (status.empty() ? "Pronto" : status);
    }
    std::cout << " |" << std::endl;

    // --- Fila de Instrucoes (PC) ---
    if (pc < instruction_queue.size()) {
        std::cout << "\n  --- Fila de Instrucoes (PC=" << pc+1 << "/" << instruction_queue.size() << ") ---" << std::endl;
        std::cout << "    Proxima a emitir: " << instruction_queue[pc].op << " " << instruction_queue[pc].dest 
                  << "," << instruction_queue[pc].src1 << "," << instruction_queue[pc].src2 << std::endl;
    } else {
        std::cout << "\n  --- Fila de Instrucoes ---" << std::endl;
        std::cout << "    Todas as instrucoes foram emitidas." << std::endl;
    }
}

void Simulator::print_final_registers() {
    std::cout << "Valores dos Registradores (F0 - F8):" << std::endl;
    std::cout << std::fixed << std::setprecision(4); 
    for (auto const& pair : reg_file) {
        const std::string& reg = pair.first;
        const double& val = pair.second;
        if (reg[0] == 'F' && reg.length() <= 3) {
             std::cout << "  " << std::setw(4) << reg << ": " << val << std::endl;
        }
    }
}