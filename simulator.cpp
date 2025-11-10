#include "simulator.h"
#include <iomanip>
#include <algorithm>

// --- Funcao Auxiliar para Mapeamento de Registradores ---
std::string Simulator::get_register_name(int index) {
    return "F" + std::to_string(index);
}

// --- Construtor ---
Simulator::Simulator() : cycle(0), pc(0), simulation_complete(false), committed_inst_count(0) {
    // Inicializa RS de ADD/SUB
    for (int i = 1; i <= ADD_RS_COUNT; ++i) {
        std::string name = "Add" + std::to_string(i);
        add_rs[name] = RS_Entry();
        add_rs[name].name = name;
    }

    // Inicializa RS de MUL/DIV
    for (int i = 1; i <= MUL_RS_COUNT; ++i) {
        std::string name = "Mult" + std::to_string(i);
        mul_rs[name] = RS_Entry();
        mul_rs[name].name = name;
    }

    // Inicializa Buffers de Load/Store (LS)
    for (int i = 1; i <= LS_COUNT; ++i) {
        std::string name = "L/S" + std::to_string(i);
        ls_rs[name] = LS_Entry();
        ls_rs[name].name = name;
    }

    // Inicializa Registradores F0 a F8
    for (int i = 0; i <= 8; ++i) {
        std::string reg_name = get_register_name(i);
        reg_file[reg_name] = i + 10.0; 
        reg_status[reg_name] = ""; 
    }
    
    // Inicializa memoria (simulada)
    memory[1000] = 50.0;
    memory[1004] = 60.0;
    memory[1008] = 70.0;
}

// --- Carregamento de Instrucoes (Popula ID e Estado Inicial) ---
bool Simulator::load_instructions(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Erro: Nao foi possivel abrir o arquivo: " << filename << std::endl;
        return false;
    }

    std::string line;
    int id_counter = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; 

        std::stringstream ss(line);
        Instruction inst;
        inst.id = id_counter++;
        inst.state = NOT_ISSUED;
        
        std::string token;
        ss >> inst.op >> token; 

        if (inst.op == "LOAD" || inst.op == "STORE") {
            inst.dest = token; 
            std::string offset_base;
            ss >> offset_base; 

            size_t open_paren = offset_base.find('(');
            size_t close_paren = offset_base.find(')');

            if (open_paren != std::string::npos && close_paren != std::string::npos) {
                inst.src2 = offset_base.substr(0, open_paren);
                inst.src1 = offset_base.substr(open_paren + 1, close_paren - open_paren - 1);
            } else {
                std::cerr << "Erro de sintaxe em instrucao de memoria: " << inst.op << " " << inst.dest << ", " << offset_base << std::endl;
                continue;
            }

        } else {
            size_t comma1 = token.find(',');
            if (comma1 != std::string::npos) {
                inst.dest = token.substr(0, comma1);
                size_t comma2 = token.find(',', comma1 + 1);
                if (comma2 != std::string::npos) {
                    inst.src1 = token.substr(comma1 + 1, comma2 - comma1 - 1);
                    inst.src2 = token.substr(comma2 + 1);
                } else { 
                    inst.src1 = token.substr(comma1 + 1);
                    ss >> inst.src2;
                }
            } else { 
                inst.dest = token;
                ss >> inst.src1 >> inst.src2;
            }
        }

        instruction_queue.push_back(inst);
    }
    file.close();
    return true;
}

// --- Loop Principal da Simulacao ---
void Simulator::run() {
    std::cout << "Iniciando Simulacao de Tomasulo com Committment..." << std::endl;
    
    // Imprime estado inicial
    std::system("clear");
    std::cout << "\n--- Estado Inicial (Ciclo 0) ---" << std::endl;
    print_instruction_status();
    print_state();
    std::cout << "\nPressione ENTER para o proximo ciclo..." << std::endl;
    std::cin.ignore(10000, '\n');

    while (!simulation_complete) {
        cycle++;
        
        std::system("clear"); 
        std::cout << "\n--- CICLO " << cycle << " ---" << std::endl;

        // Ordem dos estagios (COMMIT -> WB -> EXEC -> ISSUE)
        commit(); // Novo Estagio
        writeback();
        execute();
        issue();

        print_instruction_status();
        print_state();
        

        if (check_completion()) {
            simulation_complete = true;
        }

        if (cycle > 500) { 
            std::cout << "Simulacao interrompida (limite de ciclos atingido)." << std::endl;
            break;
        }
        
        std::cout << "\nPressione ENTER para o proximo ciclo..." << std::endl;
        std::cin.ignore(10000, '\n'); 
    }

    std::cout << "\n--- Simulacao Concluida em " << cycle << " Ciclos ---" << std::endl;
    print_final_registers();
    print_memory_state();
}


// --- NOVO ESTÁGIO: Commit (Comprometimento) ---
void Simulator::commit() {
    if (committed_inst_count >= instruction_queue.size()) {
        std::cout << "  [COMMIT] Todas as instrucoes ja foram commited." << std::endl;
        return;
    }

    Instruction& inst = instruction_queue[committed_inst_count];

    // Condicao de Commit: In Order e Writeback Completo
    if (inst.state == WRITE_RESULT) {
        
        // 1. Commit da instrucao (atualiza estado e tempo)
        inst.state = COMMITTED;
        inst.commit_cycle = cycle;
        committed_inst_count++;

        // 2. Acoes de Commit (principalmente para STORE)
        if (inst.op == "STORE") {
            // STORE nao escreve em registradores, mas o COMMIT garante que a memoria sera atualizada.
            // A operacao de memoria (escrita) ja foi concluida no Writeback, o commit so formaliza.
            std::cout << "  [COMMIT] STORE F" << inst.dest << " em [" << inst.address << "] formalmente Commited." << std::endl;
        } else {
             // Aritmetica/LOAD: O valor ja foi escrito no Register File durante o WB
             std::cout << "  [COMMIT] " << inst.op << " " << inst.dest << " Commited. Resultado finalizado." << std::endl;
        }

    } else {
        std::cout << "  [COMMIT] Parado: Instrucao " << inst.id << " (" << inst.op << ") nao esta pronta para commit (Estado: " << (inst.state == ISSUED ? "ISSUED" : (inst.state == EXECUTING ? "EXECUTING" : "NOT_ISSUED")) << ")" << std::endl;
    }
}


// --- Estagio de Emissao (Issue) - ATUALIZADO ---
void Simulator::issue() {
    if (pc >= instruction_queue.size()) {
        std::cout << "  [ISSUE] Todas as instrucoes ja foram emitidas." << std::endl;
        return;
    }

    Instruction& inst = instruction_queue[pc];

    // --- 1. Aritmetica/Logica (ADD, SUB, MUL, DIV) ---
    if (inst.op == "ADD" || inst.op == "SUB" || inst.op == "MUL" || inst.op == "DIV") {
        std::map<std::string, RS_Entry>* target_rs_map = (inst.op == "ADD" || inst.op == "SUB") ? &add_rs : &mul_rs;
        
        std::string free_rs_name = "";
        for (auto& pair : *target_rs_map) {
            if (!pair.second.busy) {
                free_rs_name = pair.first;
                break;
            }
        }

        if (free_rs_name.empty()) {
            std::cout << "  [ISSUE] Parado: Perigo Estrutural em " << inst.op << " (Sem RS Livre)" << std::endl;
            return; 
        }

        std::cout << "  [ISSUE] Emitindo (" << pc+1 << "/" << instruction_queue.size() << ") " << inst.op << " " << inst.dest << "," << inst.src1 << "," << inst.src2 << " para " << free_rs_name << std::endl;

        // Preencher a RS
        RS_Entry& rs = (*target_rs_map)[free_rs_name];
        rs.busy = true;
        rs.op = inst.op;
        rs.instruction_id = inst.id; // NOVO: Rastreamento do ID
        rs.cycles_remaining = -1; 
        rs.ready_to_writeback = false;

        // Preencher Vj/Qj e Vk/Qk (logica de renomeacao)
        if (reg_status.count(inst.src1) && reg_status[inst.src1].empty()) { rs.vj = reg_file[inst.src1]; rs.qj = ""; } 
        else if (reg_status.count(inst.src1)) { rs.qj = reg_status[inst.src1]; }

        if (reg_status.count(inst.src2) && reg_status[inst.src2].empty()) { rs.vk = reg_file[inst.src2]; rs.qk = ""; } 
        else if (reg_status.count(inst.src2)) { rs.qk = reg_status[inst.src2]; }

        // Renomear o registrador de destino
        reg_status[inst.dest] = free_rs_name;
        
        // NOVO: Atualizar estado da instrucao
        inst.state = ISSUED;
        inst.issue_cycle = cycle;
        pc++;
        
    // --- 2. Load/Store (LOAD, STORE) ---
    } else if (inst.op == "LOAD" || inst.op == "STORE") {
        std::string free_ls_name = "";
        for (auto& pair : ls_rs) {
            if (!pair.second.busy) {
                free_ls_name = pair.first;
                break;
            }
        }

        if (free_ls_name.empty()) {
            std::cout << "  [ISSUE] Parado: Perigo Estrutural em " << inst.op << " (Sem L/S Buffer Livre)" << std::endl;
            return; 
        }

        std::cout << "  [ISSUE] Emitindo (" << pc+1 << "/" << instruction_queue.size() << ") " << inst.op << " " << inst.dest << ", " << inst.src2 << "(" << inst.src1 << ") para " << free_ls_name << std::endl;

        LS_Entry& ls = ls_rs[free_ls_name];
        ls.busy = true;
        ls.op = inst.op;
        ls.dest_reg = inst.dest; 
        ls.base_reg = inst.src1; 
        ls.offset = std::stoi(inst.src2); 
        ls.instruction_id = inst.id; // NOVO: Rastreamento do ID
        ls.cycles_remaining = -1; 
        ls.address_ready = false;
        ls.ready_to_writeback = false;
        ls.store_value = 0.0;
        ls.store_producer = "";

        // A. Base Register (SRC1)
        if (reg_status.count(ls.base_reg) && reg_status[ls.base_reg].empty()) { ls.base_value = reg_file[ls.base_reg]; ls.base_producer = ""; } 
        else if (reg_status.count(ls.base_reg)) { ls.base_producer = reg_status[ls.base_reg]; }

        // B. Store Value (Dest Reg) - Apenas para STORE
        if (ls.op == "STORE") {
            std::string store_reg = inst.dest; 
            if (reg_status.count(store_reg) && reg_status[store_reg].empty()) { ls.store_value = reg_file[store_reg]; ls.store_producer = ""; } 
            else if (reg_status.count(store_reg)) { ls.store_producer = reg_status[store_reg]; }
        }

        // C. Renomeacao: Apenas LOAD renomeia o registrador de destino
        if (ls.op == "LOAD") {
            reg_status[ls.dest_reg] = free_ls_name;
        }

        // NOVO: Atualizar estado da instrucao
        inst.state = ISSUED;
        inst.issue_cycle = cycle;
        pc++;
    } else {
        std::cout << "  [ISSUE] ERRO: OP desconhecido: " << inst.op << std::endl;
        pc++;
    }
}

// --- Estagio de Execucao (Execute) - ATUALIZADO ---
void Simulator::execute() {
    bool exec_activity = false;
    
    // --- 1. Processa RS de Aritmetica/Logica ---
    auto process_rs_map = [&](std::map<std::string, RS_Entry>& rs_map) {
        for (auto& pair : rs_map) {
            RS_Entry& rs = pair.second;
            if (!rs.busy || rs.ready_to_writeback) continue;

            if (rs.qj.empty() && rs.qk.empty()) {
                
                // NOVO: 1. Iniciar execucao (se cycles_remaining == -1)
                if (rs.cycles_remaining == -1) {
                    int latency = (rs.op == "ADD" || rs.op == "SUB") ? ADD_LATENCY : ((rs.op == "MUL") ? MUL_LATENCY : DIV_LATENCY);
                    rs.cycles_remaining = latency;
                    instruction_queue[rs.instruction_id].state = EXECUTING; // Atualiza estado
                    instruction_queue[rs.instruction_id].exec_start_cycle = cycle; // Atualiza tempo
                    std::cout << "  [EXEC] Iniciando " << rs.name << " (" << rs.op << ") | Latencia: " << latency << std::endl;
                    exec_activity = true;
                }
                
                // 2. Decrementar contador
                if (rs.cycles_remaining > 0) {
                    rs.cycles_remaining--;
                    exec_activity = true;
                }

                // NOVO: 3. Execucao concluida (cycles_remaining == 0)
                if (rs.cycles_remaining == 0) {
                    // Calculo do resultado
                    if (rs.op == "ADD") rs.result = rs.vj + rs.vk;
                    else if (rs.op == "SUB") rs.result = rs.vj - rs.vk;
                    else if (rs.op == "MUL") rs.result = rs.vj * rs.vk;
                    else if (rs.op == "DIV") rs.result = (rs.vk == 0) ? 0.0 : rs.vj / rs.vk;
                    
                    std::cout << "  [EXEC] Concluindo " << rs.name << " | Resultado: " << std::fixed << std::setprecision(4) << rs.result << std::endl;
                    rs.ready_to_writeback = true;
                    instruction_queue[rs.instruction_id].exec_end_cycle = cycle; // Atualiza tempo
                    exec_activity = true;
                }
            }
        }
    };

    process_rs_map(add_rs);
    process_rs_map(mul_rs);


    // --- 2. Processa Buffers de Load/Store (L/S) - ATUALIZADO ---
    for (auto& pair : ls_rs) {
        LS_Entry& ls = pair.second;
        if (!ls.busy || ls.ready_to_writeback) continue;

        Instruction& inst = instruction_queue[ls.instruction_id];

        // A. Calculo de Endereco (Execucao do Endereco)
        if (!ls.address_ready && ls.base_producer.empty()) {
            ls.calculated_address = (long)(ls.base_value + ls.offset);
            ls.address_ready = true;
            ls.cycles_remaining = MEM_ACCESS_LATENCY; // Inicia a latencia de memoria
            
            // NOVO: Marca o inicio da execucao
            inst.state = EXECUTING;
            inst.exec_start_cycle = cycle; 
            inst.address = ls.calculated_address; // Salva endereco na instrucao
            
            std::cout << "  [EXEC] Endereco de " << ls.name << " (" << ls.op << ") calculado: " << ls.calculated_address << std::endl;
            exec_activity = true;
        } 
        
        // B. Acesso a Memoria (Execucao de Memoria)
        if (ls.address_ready) {
            
            // Perigo de Memoria (RAW Store-Load ou WAW/WAR)
            LS_Entry* hazard_rs = find_address_hazard(ls.calculated_address, ls.name);
            if (hazard_rs != nullptr) {
                std::cout << "  [EXEC] " << ls.name << " (" << ls.op << ") PARADO: Perigo de Memoria com " << hazard_rs->name << std::endl;
                continue; 
            }

            // Para STORE: Deve esperar o valor a ser armazenado (RAW no valor)
            if (ls.op == "STORE" && !ls.store_producer.empty()) {
                 std::cout << "  [EXEC] " << ls.name << " (STORE) PARADO: Esperando valor do produtor " << ls.store_producer << std::endl;
                 continue; 
            }
            
            // Decrementar contador de acesso a memoria
            if (ls.cycles_remaining > 0) {
                ls.cycles_remaining--;
                exec_activity = true;
            }

            // Acesso a Memoria Concluido
            if (ls.cycles_remaining == 0) {
                // NOVO: Marca o fim da execucao
                inst.exec_end_cycle = cycle; 

                if (ls.op == "LOAD") {
                    ls.result = memory.count(ls.calculated_address) ? memory[ls.calculated_address] : 0.0;
                    std::cout << "  [EXEC] Concluindo " << ls.name << " (LOAD). Valor lido: " << ls.result << std::endl;
                } else if (ls.op == "STORE") {
                    std::cout << "  [EXEC] Concluindo " << ls.name << " (STORE). Pronto para escrever na memoria." << std::endl;
                }
                ls.ready_to_writeback = true;
                exec_activity = true;
            }
        }
    }


    if (!exec_activity) {
        std::cout << "  [EXEC] Nenhuma RS/LS iniciou ou avancou a execucao." << std::endl;
    }
}

// --- Estagio de Escrita (Writeback) - ATUALIZADO E CORRIGIDO ---
void Simulator::writeback() {
    std::string cleared_rs_name = ""; 
    
    // CORREÇÃO: Usar um template na lambda para aceitar std::map<..., RS_Entry> ou std::map<..., LS_Entry>
    auto find_ready = []<typename T>(std::map<std::string, T>& rs_map, bool is_ls = false) -> std::pair<std::string, double> {
        for (auto& pair : rs_map) {
            if (pair.second.ready_to_writeback) {
                if (!is_ls) {
                    return {pair.first, pair.second.result};
                } else {
                    // Para LS (T = LS_Entry), precisamos verificar se e LOAD ou STORE
                    if (pair.second.op == "LOAD") {
                        return {pair.first, pair.second.result};
                    } else { // STORE
                        return {pair.first, -1.0}; // -1.0 para sinalizar STORE (nao escreve CDB)
                    }
                }
            }
        }
        return {"", 0.0};
    };

    std::string broadcasting_rs_name = "";
    double broadcasting_result = 0.0;
    
    // Chamadas corrigidas
    auto add_result = find_ready(add_rs);
    auto mul_result = find_ready(mul_rs);
    auto ls_result = find_ready(ls_rs, true);

    if (!add_result.first.empty()) {
        broadcasting_rs_name = add_result.first;
        broadcasting_result = add_result.second;
    } else if (!mul_result.first.empty()) {
        broadcasting_rs_name = mul_result.first;
        broadcasting_result = mul_result.second;
    } else if (!ls_result.first.empty()) {
        broadcasting_rs_name = ls_result.first;
        broadcasting_result = ls_result.second;
    }
    
    cleared_rs_name = broadcasting_rs_name;


    if (broadcasting_rs_name.empty()) {
        std::cout << "  [WB] Ninguem transmitindo resultado." << std::endl;
        return;
    }

    bool is_store = (cleared_rs_name.find("L/S") != std::string::npos && ls_rs[cleared_rs_name].op == "STORE");
    
    int inst_id = -1;
    if (cleared_rs_name.find("L/S") == std::string::npos) {
        // Aritmetica
        RS_Entry& rs = (cleared_rs_name.find("Add") != std::string::npos) ? add_rs[cleared_rs_name] : mul_rs[cleared_rs_name];
        inst_id = rs.instruction_id;
    } else {
        // Load/Store
        LS_Entry& ls = ls_rs[cleared_rs_name];
        inst_id = ls.instruction_id;
    }

    // --- 1. Acoes de Escrita Efetivas ---
    if (is_store) {
        // STORE: Escreve na Memoria, NÃO no CDB (por isso o result é -1.0)
        LS_Entry& ls = ls_rs[cleared_rs_name];
        memory[ls.calculated_address] = ls.store_value; 
        std::cout << "  [WB] " << cleared_rs_name << " (STORE) concluido. Valor " << ls.store_value << " escrito em [" << ls.calculated_address << "]." << std::endl;
    } else {
        // LOAD ou Aritmetica: Transmite o resultado para o CDB e Registradores
        std::cout << "  [WB] " << cleared_rs_name << " transmitindo resultado " << std::fixed << std::setprecision(4) << broadcasting_result << std::endl;

        // Atualizar o banco de registradores (WAW protection implicita pelo reg_status)
        for (auto& pair : reg_status) {
            std::string reg = pair.first;
            std::string& status = pair.second;
            if (status == cleared_rs_name) {
                reg_file[reg] = broadcasting_result;
                // Vamos manter o status limpo apos a escrita para a proxima instrucao poder emitir.
                status = ""; 
                std::cout << "  [WB] CDB: Registrador " << reg << " atualizado e liberado (Qi limpo)." << std::endl;
            }
        }
    }

    // --- 2. Transmitir para todas as RSs/LSs em espera (CDB) ---
    auto update_rs = [&](std::map<std::string, RS_Entry>& rs_map) {
        for (auto& pair : rs_map) {
            RS_Entry& rs = pair.second;
            if (rs.busy && !rs.ready_to_writeback) { 
                if (rs.qj == cleared_rs_name) { rs.vj = broadcasting_result; rs.qj = ""; }
                if (rs.qk == cleared_rs_name) { rs.vk = broadcasting_result; rs.qk = ""; }
            }
        }
    };
    update_rs(add_rs);
    update_rs(mul_rs);
    
    for (auto& pair : ls_rs) {
        LS_Entry& ls = pair.second;
        if (ls.busy && !ls.ready_to_writeback) {
            if (ls.base_producer == cleared_rs_name) { ls.base_value = broadcasting_result; ls.base_producer = ""; }
            if (ls.op == "STORE" && ls.store_producer == cleared_rs_name) { ls.store_value = broadcasting_result; ls.store_producer = ""; }
        }
    }

    // --- 3. Limpar a RS/LS e Atualizar Estado ---
    if (cleared_rs_name.find("L/S") == std::string::npos) {
        // Limpar RS Aritmetica
        RS_Entry& rs = (cleared_rs_name.find("Add") != std::string::npos) ? add_rs[cleared_rs_name] : mul_rs[cleared_rs_name];
        rs.busy = false; rs.op = ""; rs.vj = 0.0; rs.vk = 0.0; rs.qj = ""; rs.qk = "";
        rs.cycles_remaining = -1; rs.ready_to_writeback = false; rs.result = 0.0; rs.instruction_id = -1;
    } else {
        // Limpar LS Buffer
        LS_Entry& ls = ls_rs[cleared_rs_name];
        ls.busy = false; ls.op = ""; ls.dest_reg = ""; ls.base_reg = "";
        ls.base_value = 0.0; ls.base_producer = ""; ls.offset = 0; ls.calculated_address = -1;
        ls.address_ready = false; ls.store_value = 0.0; ls.store_producer = "";
        ls.cycles_remaining = -1; ls.result = 0.0; ls.ready_to_writeback = false; ls.instruction_id = -1;
    }
    
    // NOVO: Atualizar estado da instrucao para WRITE_RESULT (Pronta para Commit)
    if (inst_id != -1) {
        Instruction& inst = instruction_queue[inst_id];
        inst.state = WRITE_RESULT;
        inst.write_cycle = cycle;
    }
}

// --- Funcao de Checagem de Perigo de Memoria ---
LS_Entry* Simulator::find_address_hazard(long address, const std::string& current_name) {
    for (auto& pair : ls_rs) {
        LS_Entry& hazard_rs = pair.second;
        
        if (!hazard_rs.busy || hazard_rs.name == current_name) continue;

        if (hazard_rs.address_ready && hazard_rs.calculated_address == address) {
            
            // RAW (Store antes de Load no mesmo endereco)
            if (hazard_rs.op == "STORE" && !hazard_rs.ready_to_writeback) {
                 return &hazard_rs;
            }
        }
    }
    return nullptr;
}

// --- Funcoes de Checagem e Impressao (Atualizadas) ---

bool Simulator::check_completion() {
    return committed_inst_count == instruction_queue.size();
}

// NOVO: Imprime o status detalhado das instrucoes
void Simulator::print_instruction_status() {
    std::cout << "\n  --- STATUS DAS INSTRUCOES (ROB Implicito) ---" << std::endl;
    std::cout << "    " << std::setw(3) << "ID" << " | " << std::setw(4) << "OP" << " | " << std::setw(12) << "Estado" << " | "
              << std::setw(4) << "Issue" << " | " << std::setw(4) << "ExecS" << " | " << std::setw(4) << "ExecE" << " | " 
              << std::setw(4) << "Write" << " | " << std::setw(4) << "Commit" << std::endl;
    std::cout << "    " << std::string(60, '-') << std::endl;

    auto state_to_string = [](InstrState state) -> std::string {
        switch (state) {
            case NOT_ISSUED: return "NOT_ISSUED";
            case ISSUED: return "ISSUED";
            case EXECUTING: return "EXECUTING";
            case WRITE_RESULT: return "WRITE_RESULT";
            case COMMITTED: return "COMMITTED";
            default: return "UNKNOWN";
        }
    };

    for (const auto& inst : instruction_queue) {
        std::string issue_c = inst.issue_cycle > 0 ? std::to_string(inst.issue_cycle) : "-";
        std::string execs_c = inst.exec_start_cycle > 0 ? std::to_string(inst.exec_start_cycle) : "-";
        std::string exece_c = inst.exec_end_cycle > 0 ? std::to_string(inst.exec_end_cycle) : "-";
        std::string write_c = inst.write_cycle > 0 ? std::to_string(inst.write_cycle) : "-";
        std::string commit_c = inst.commit_cycle > 0 ? std::to_string(inst.commit_cycle) : "-";

        std::cout << "    " << std::setw(3) << inst.id << " | " << std::setw(4) << inst.op << " | " << std::setw(12) << state_to_string(inst.state) << " | "
                  << std::setw(4) << issue_c << " | " << std::setw(4) << execs_c << " | " << std::setw(4) << exece_c << " | "
                  << std::setw(4) << write_c << " | " << std::setw(4) << commit_c << std::endl;
    }
}


void Simulator::print_state() {
    std::cout << std::fixed << std::setprecision(4); 

    // --- Tabela de Estacoes de Reserva Aritmeticas ---
    std::cout << "\n  --- Estacoes de Reserva (Aritmeticas) ---" << std::endl;
    std::cout << "    " << std::setw(6) << "Nome" << " | " << std::setw(4) << "Busy" << " | " << std::setw(4) << "Op" << " | " << std::setw(10) << "Vj" << " | " << std::setw(10) << "Vk" << " | " << std::setw(6) << "Qj" << " | " << std::setw(6) << "Qk" << " | " << "Ciclos/ID" << std::endl;
    std::cout << "    " << std::string(75, '-') << std::endl;
    
    auto print_rs_map = [&](std::map<std::string, RS_Entry>& rs_map) {
        for (auto const& pair : rs_map) {
            const std::string& name = pair.first;
            const RS_Entry& rs = pair.second;
            
            std::string vj_str = "";
            std::string vk_str = "";
            if (rs.busy) {
                if (rs.qj.empty()) vj_str = std::to_string(rs.vj); else vj_str = " ";
                if (rs.qk.empty()) vk_str = std::to_string(rs.vk); else vk_str = " ";
            }

            std::string cycles_str = "-";
            if (!rs.busy) cycles_str = "-";
            else if (rs.ready_to_writeback) cycles_str = "WB/ID"+std::to_string(rs.instruction_id);
            else if (rs.cycles_remaining >= 0) cycles_str = std::to_string(rs.cycles_remaining) + "/ID"+std::to_string(rs.instruction_id);
            else if (rs.cycles_remaining == -1 && (rs.qj.empty() && rs.qk.empty())) cycles_str = "RTS/ID"+std::to_string(rs.instruction_id); 

            std::string busy_str = rs.busy ? "Sim" : "Nao";

            std::cout << "    " << std::setw(6) << name << " | " << std::setw(4) << busy_str << " | " << std::setw(4) << rs.op << " | "
                      << std::setw(10) << vj_str << " | "
                      << std::setw(10) << vk_str << " | "
                      << std::setw(6) << rs.qj << " | " << std::setw(6) << rs.qk << " | "
                      << std::setw(10) << cycles_str << std::endl;
        }
    };
    
    print_rs_map(add_rs);
    print_rs_map(mul_rs);
    
    // --- Tabela de Buffers de Load/Store (Memoria) ---
    std::cout << "\n  --- Buffers de Load/Store (Memoria) ---" << std::endl;
    std::cout << "    " << std::setw(6) << "Nome" << " | " << std::setw(4) << "Op" << " | " << std::setw(6) << "FDest" << " | " << std::setw(6) << "End_Calc" << " | " << std::setw(6) << "QBase" << " | " << std::setw(6) << "QStore" << " | " << "Ciclos/ID" << std::endl;
    std::cout << "    " << std::string(75, '-') << std::endl;
    
    for (auto const& pair : ls_rs) {
        const std::string& name = pair.first;
        const LS_Entry& ls = pair.second;
        
        if (!ls.busy) {
            std::cout << "    " << std::setw(6) << name << " | " << std::setw(4) << "Nao" << " | " << std::setw(6) << "" << " | " << std::setw(6) << "-" << " | " << std::setw(6) << "" << " | " << std::setw(6) << "" << " | " << std::setw(10) << "-" << std::endl;
            continue;
        }

        std::string op_str = ls.op;
        std::string reg_str = ls.dest_reg; 
        std::string addr_str = (ls.calculated_address != -1) ? std::to_string(ls.calculated_address) : "Calc";
        
        std::string cycles_str = "Wait/ID"+std::to_string(ls.instruction_id);
        if (ls.ready_to_writeback) cycles_str = (ls.op == "LOAD") ? "WB/ID"+std::to_string(ls.instruction_id) : "Done/ID"+std::to_string(ls.instruction_id);
        else if (ls.cycles_remaining >= 0) cycles_str = std::to_string(ls.cycles_remaining) + "/ID"+std::to_string(ls.instruction_id);
        else if (ls.address_ready) cycles_str = "MemWait/ID"+std::to_string(ls.instruction_id);
        else if (ls.base_producer.empty() && ls.op == "STORE" && ls.store_producer.empty()) cycles_str = "RTS/ID"+std::to_string(ls.instruction_id);
        else if (ls.base_producer.empty() && ls.op == "LOAD") cycles_str = "RTS/ID"+std::to_string(ls.instruction_id);


        std::cout << "    " << std::setw(6) << name << " | " << std::setw(4) << op_str << " | " << std::setw(6) << reg_str << " | " 
                  << std::setw(6) << addr_str << " | " << std::setw(6) << ls.base_producer << " | " 
                  << std::setw(6) << (ls.op == "STORE" ? ls.store_producer : "") << " | " << std::setw(10) << cycles_str << std::endl;
    }


    // --- Tabela de Status dos Registradores (Qi) ---
    std::cout << "\n  --- Status dos Registradores (Qi) ---" << std::endl;
    std::cout << "   ";
    for (int i = 0; i <= 8; ++i) {
        std::string reg_name = get_register_name(i);
        std::cout << " | " << std::setw(8) << reg_name;
    }
    std::cout << " |" << std::endl;
    std::cout << "   ";
    for (int i = 0; i <= 8; ++i) {
        std::string reg_name = get_register_name(i);
        const std::string& status = reg_status[reg_name];
        std::cout << " | " << std::setw(8) << (status.empty() ? "Pronto" : status);
    }
    std::cout << " |" << std::endl;

    // --- Fila de Instrucoes (PC) ---
    if (pc < instruction_queue.size()) {
        const Instruction& next_inst = instruction_queue[pc];
        std::cout << "\n  --- Fila de Instrucoes (PC=" << pc+1 << "/" << instruction_queue.size() << ") ---" << std::endl;
        std::cout << "    Proxima a emitir (ID " << next_inst.id << "): " << next_inst.op << " " << next_inst.dest << (next_inst.op == "LOAD" || next_inst.op == "STORE" ? ", " + next_inst.src2 + "(" + next_inst.src1 + ")" : ", " + next_inst.src1 + "," + next_inst.src2) << std::endl;
    } else {
        std::cout << "\n  --- Fila de Instrucoes ---" << std::endl;
        std::cout << "    Todas as instrucoes foram emitidas." << std::endl;
    }
}

void Simulator::print_final_registers() {
    std::cout << "\nValores Finais dos Registradores:" << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    for (int i = 0; i <= 8; ++i) {
        std::string reg_name = get_register_name(i);
        std::cout << "  " << reg_name << ": " << reg_file[reg_name] << std::endl;
    }
}

void Simulator::print_memory_state() {
    std::cout << "\nConteudo Final da Memoria (Enderecos Modificados):" << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    for (auto const& pair : memory) {
        std::cout << "  [" << pair.first << "]: " << pair.second << std::endl;
    }
}