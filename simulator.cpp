#include "simulator.h"
#include <iomanip>
#include <algorithm>
#include <regex>
#include <cstring>


// --- Funcao Auxiliar para Mapeamento de Registradores ---
std::string Simulator::get_register_name(int index) {
    return "F" + std::to_string(index);
}

// --- Construtor ---
// --- Construtor ---
Simulator::Simulator() 
    : cycle(0), pc(0), simulation_complete(false), committed_inst_count(0) {

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

    // Inicializa Registradores F0 a F8 com valores iniciais
    for (int i = 0; i <= 8; ++i) {
        std::string reg_name = get_register_name(i);
        reg_file[reg_name] = i + 10.0; 
        reg_status[reg_name] = ""; 
    }

    // --- Valores iniciais específicos para teste ---
    reg_file["F1"] = 100.0;   // Base address (para LOADs)
    reg_file["F8"] = 2.0;     // Multiplicador

    // --- Inicializa memória com dados reais ---
    memory[132] = 10.0;   // endereço base 100 + offset 32
    memory[136] = 20.0;   // endereço base 100 + offset 36
    memory[1000] = 50.0;
    memory[1004] = 60.0;
    memory[1008] = 70.0;

    std::cout << "[Inicialização] Registradores e memória configurados:\n";
    std::cout << "  F1 = 100.0 (base)\n";
    std::cout << "  F8 = 2.0\n";
    std::cout << "  mem[132] = 10.0\n";
    std::cout << "  mem[136] = 20.0\n";
    std::cout << "  mem[1000] = 50.0\n";
    std::cout << "  mem[1004] = 60.0\n";
    std::cout << "  mem[1008] = 70.0\n";
}


// --- Carregamento de Instrucoes (Popula ID e Estado Inicial) ---

bool Simulator::load_instructions(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Erro ao abrir arquivo de instrucoes: " << filename << std::endl;
        return false;
    }
    std::string line;
    int id_counter = 0;
    std::regex r_load(R"(^\s*(LOAD|STORE)\s+(\w+)\s*,\s*([+-]?\d+)\((\w+)\)\s*$)", std::regex::icase);
    std::regex r_rtype(R"(^\s*(\w+)\s+(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*$)", std::regex::icase);
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto posc = line.find('#');
        if (posc != std::string::npos) line = line.substr(0, posc);
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        std::smatch m;
        Instruction inst;
        inst.id = id_counter++;
        inst.state = NOT_ISSUED;
        inst.value = 0.0;
        inst.has_value = false;
        inst.producer_tag = "";
        inst.is_store = false;
        if (std::regex_search(line, m, r_load)) {
            inst.op = m[1];
            std::string dest = m[2];
            std::string offset = m[3];
            std::string base = m[4];
            inst.dest = dest;
            inst.src1 = base; // base register
            inst.src2 = offset; // store/load offset as string
            if (strcasecmp(inst.op.c_str(), "STORE") == 0) inst.is_store = true;
        } else if (std::regex_search(line, m, r_rtype)) {
            inst.op = m[1];
            inst.dest = m[2];
            inst.src1 = m[3];
            inst.src2 = m[4];
        } else {
            std::cerr << "Formato invalido de instrucao (ignorando): " << line << std::endl;
            continue;
        }
        auto ensure_reg = [&](const std::string &r) {
            if (r.empty()) return;
            if (!reg_file.count(r)) { reg_file[r] = 0.0; reg_status[r] = ""; }
        };
        ensure_reg(inst.dest);
        ensure_reg(inst.src1);
        ensure_reg(inst.src2);
        instruction_queue.push_back(inst);
    }
    std::cout << "Loaded " << instruction_queue.size() << " instructions from " << filename << std::endl;
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
    // Commit in program order (instruction_queue as ROB)
    while (committed_inst_count < (int)instruction_queue.size()) {
        Instruction &inst = instruction_queue[committed_inst_count];
        if (inst.state != WRITE_RESULT || !inst.has_value) break;

        if (inst.is_store) {
            // --- STORE ---
            if (inst.address >= 0) {
                memory[inst.address] = inst.value;
                std::cout << "  [COMMIT] STORE ID" << inst.id
                          << " mem[" << inst.address << "] = "
                          << inst.value << std::endl;
            } else {
                std::cout << "  [COMMIT] STORE ID" << inst.id
                          << " address not set." << std::endl;
            }

        } else {
            // --- LOAD / ALU instruction ---
            if (!inst.dest.empty()) {
                if (reg_status[inst.dest] == inst.producer_tag) {
                    reg_file[inst.dest] = inst.value;
                    reg_status[inst.dest] = "";

                    // ✅ NOVO: Broadcast para liberar dependentes ainda esperando
                    for (auto &r : add_rs) {
                        if (r.second.qj == inst.producer_tag) {
                            r.second.vj = inst.value;
                            r.second.qj = "";
                        }
                        if (r.second.qk == inst.producer_tag) {
                            r.second.vk = inst.value;
                            r.second.qk = "";
                        }
                    }
                    for (auto &r : mul_rs) {
                        if (r.second.qj == inst.producer_tag) {
                            r.second.vj = inst.value;
                            r.second.qj = "";
                        }
                        if (r.second.qk == inst.producer_tag) {
                            r.second.vk = inst.value;
                            r.second.qk = "";
                        }
                    }
                    for (auto &r : ls_rs) {
                        if (r.second.base_producer == inst.producer_tag) {
                            r.second.base_value = inst.value;
                            r.second.base_producer = "";
                        }
                        if (r.second.store_producer == inst.producer_tag) {
                            r.second.store_value = inst.value;
                            r.second.store_producer = "";
                        }
                    }

                    std::cout << "  [COMMIT] WRITE " << inst.dest
                              << " = " << inst.value << " (ID"
                              << inst.id << ")" << std::endl;
                } else {
                    std::cout << "  [COMMIT] Skipped write to "
                              << inst.dest
                              << " because reg_status changed."
                              << std::endl;
                }
            }
        }

        inst.state = COMMITTED;
        inst.commit_cycle = cycle;
        committed_inst_count++;
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
    // 1️⃣ Percorre todas as estações de reserva (Add/Sub e Mul/Div)
    std::vector<std::pair<std::string, RS_Entry*>> all_rs;
    for (auto &p : add_rs) all_rs.push_back({p.first, &p.second});
    for (auto &p : mul_rs) all_rs.push_back({p.first, &p.second});

    // 2️⃣ Escolhe uma RS aritmética pronta para escrever (CDB)
    for (auto &[name, rs] : all_rs) {
        if (!rs->busy) continue;
        if (!rs->ready_to_writeback) continue;

        int inst_id = rs->instruction_id;
        if (inst_id < 0 || inst_id >= (int)instruction_queue.size()) continue;
        Instruction &inst = instruction_queue[inst_id];

        double result = rs->result;
        std::string tag = name;

        // Guarda resultado (aguarda commit)
        inst.value = result;
        inst.has_value = true;
        inst.producer_tag = tag;
        inst.write_cycle = cycle;
        inst.state = WRITE_RESULT;

        std::cout << "  [WB] " << tag << " transmitiu resultado "
                  << std::fixed << std::setprecision(4) << result
                  << " (aguardando commit)" << std::endl;

        // Broadcast (atualiza operandos dependentes)
        for (auto &r : add_rs) {
            if (r.second.qj == tag) { r.second.vj = result; r.second.qj = ""; }
            if (r.second.qk == tag) { r.second.vk = result; r.second.qk = ""; }
        }
        for (auto &r : mul_rs) {
            if (r.second.qj == tag) { r.second.vj = result; r.second.qj = ""; }
            if (r.second.qk == tag) { r.second.vk = result; r.second.qk = ""; }
        }
        for (auto &r : ls_rs) {
            if (r.second.base_producer == tag) {
                r.second.base_value = result;
                r.second.base_producer = "";
            }
            if (r.second.store_producer == tag) {
                r.second.store_value = result;
                r.second.store_producer = "";
            }
        }

        // Libera estação
        rs->busy = false;
        rs->ready_to_writeback = false;
        rs->instruction_id = -1;
        rs->cycles_remaining = -1;
        rs->result = 0.0;
        break; // apenas um broadcast por ciclo
    }

    // 3️⃣ Agora processa as estações de LOAD/STORE
    for (auto &pair : ls_rs) {
        LS_Entry &ls = pair.second;
        if (!ls.busy) continue;
        if (!ls.ready_to_writeback) continue;

        int inst_id = ls.instruction_id;
        if (inst_id < 0 || inst_id >= (int)instruction_queue.size()) continue;
        Instruction &inst = instruction_queue[inst_id];

        if (ls.op == "LOAD") {
            inst.value = ls.result;
            inst.has_value = true;
            inst.producer_tag = ls.name;
            inst.write_cycle = cycle;
            inst.state = WRITE_RESULT;
            std::cout << "  [WB] " << ls.name << " (LOAD) leu valor "
                      << std::fixed << std::setprecision(4) << ls.result
                      << " e liberou buffer" << std::endl;
        } else if (ls.op == "STORE") {
            // STORE não escreve nada no registrador, só sinaliza commit futuro
            inst.value = ls.store_value;
            inst.has_value = true;
            inst.producer_tag = ls.name;
            inst.write_cycle = cycle;
            inst.state = WRITE_RESULT;
            std::cout << "  [WB] " << ls.name << " (STORE) pronto para commit" << std::endl;
        }

        // Libera a estação L/S
        ls.busy = false;
        ls.op = "";
        ls.dest_reg = "";
        ls.base_reg = "";
        ls.base_producer = "";
        ls.store_producer = "";
        ls.address_ready = false;
        ls.ready_to_writeback = false;
        ls.cycles_remaining = -1;
        ls.instruction_id = -1;
        break; // um por ciclo
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