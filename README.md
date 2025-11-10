# 🚀 Simulador do Algoritmo de Tomasulo com Comprometimento (Commitment)

Este projeto implementa um simulador de pipeline **superescalar** com **Execução Fora-de-Ordem** utilizando o algoritmo de Tomasulo, estendido com um mecanismo de **Comprometimento (Commitment)**. Esta arquitetura é essencial para processadores modernos, pois garante a **Terminação Em-Ordem** das instruções, resolvendo exceções e garantindo a exatidão do estado arquitetural (registradores e memória).

---

## ⚙️ Configuração do Simulador e Arquitetura

A arquitetura simulada possui a seguinte capacidade, latência e componentes:

| Componente | Estações de Reserva (Slots) | Latência (Ciclos) | Notas |
| :--- | :--- | :--- | :--- |
| **ADD/SUB** | 3 | 2 | Unidade Funcional de Adição/Subtração |
| **MUL/DIV** | 2 | 10 (MUL) / 40 (DIV) | Unidade Funcional de Multiplicação/Divisão |
| **L/S Buffers** | 2 | 3 (Memória) | Buffers para operações LOAD/STORE |
| **ROB (Implícito)** | Fila de Instruções | - | Rastreia o estado para garantir o Commit In-Order |

---

## 🚀 Como Compilar e Executar

1.  **Compilar:** Utilize o compilador `g++` com o padrão C++14 ou superior para suportar recursos modernos da linguagem.

    ```bash
    g++ -o simulador main.cpp simulator.cpp -std=c++14
    ```

2.  **Executar:** Passe o arquivo de texto contendo a lista de instruções (ex: `instructions.txt`) como argumento:

    ```bash
    ./simulador instructions.txt
    ```

---

## 🧠 Explicação Detalhada do Algoritmo e dos Estágios (Ciclo a Ciclo)

O pipeline é executado rigorosamente na ordem: **COMMIT $\rightarrow$ WRITEBACK $\rightarrow$ EXECUTE $\rightarrow$ ISSUE**.

### 1. Estágio de *Commit* (Comprometimento)

Este estágio final garante que os resultados permanentes sejam escritos no estado arquitetural na ordem sequencial, semelhante à função do *Reorder Buffer (ROB)*:

* **Lógica:** Analisa a instrução mais antiga na Fila de Instruções.
* **Condição:** A instrução deve estar no estado **WRITE\_RESULT**.
* **Ação:** A instrução é promovida para **COMMITTED**. Para **STORE**, a alteração na memória é formalmente confirmada neste ponto.
* **Terminação Em-Ordem:** As instruções devem comitar na ordem em que foram emitidas.

### 2. Estágio de *Writeback* (Escrita)

* **Lógica:** Uma única RS ou L/S (LOAD/STORE) pronta é selecionada para transmitir seu resultado pelo **Barramento Comum de Dados (CDB)**.
* **Broadcast (CDB):**
    * O resultado é gravado no **Banco de Registradores** (`reg_file`), resolvendo a renomeação para o destino.
    * Todas as RSs/L/S em espera capturam o valor, limpando seus campos de produtor (`Qj`/`Qk`).
* **Atualização de Estado:** A instrução é marcada como **WRITE\_RESULT** (Pronta para Commit).
* A RS/L/S é liberada (`busy = false`).

### 3. Estágio de *Execute* (Execução)

* **Aritmética:** Inicia ou continua se os valores dos operandos estiverem disponíveis (`Qj` e `Qk` vazios).
* **Load/Store (L/S):**
    * **Cálculo de Endereço:** Calculado assim que a base estiver disponível.
    * **Perigo de Memória (RAW):** Para **LOAD**, a execução é paralisada se um **STORE** anterior para o mesmo endereço estiver esperando o Writeback, garantindo a ordem dos acessos à memória.

### 4. Estágio de *Issue* (Emissão)

* **Lógica:** A próxima instrução é alocada na primeira Estação de Reserva/Buffer L/S livre.
* **Renomeação:** Dependências de dados são resolvidas: operando pronto gera **valor** (`Vj`/`Vk`); operando pendente gera **nome do produtor** (`Qj`/`Qk`).
* **Status (Qi):** O registrador de destino é marcado com o nome da RS/L/S recém-emitida.

---

## 🛠️ Detalhes da Implementação (C++ Pseudocódigo)

Abaixo estão trechos de pseudocódigo em C++ que ilustram a lógica central dos estágios críticos do simulador.

### Função `Simulator::commit()`

A lógica de Commit verifica a instrução mais antiga (`committed_inst_count`) para garantir a Terminação Em-Ordem.

```cpp
void Simulator::commit() {
    if (committed_inst_count < instruction_queue.size()) {
        Instruction& inst = instruction_queue[committed_inst_count];
        
        // Só pode comitar se o resultado já foi escrito (WRITE_RESULT)
        if (inst.state == InstrState::WRITE_RESULT) {
            
            // Lógica de confirmação da escrita na memória (STORE)
            if (inst.op_code == "STORE") {
                // Se fosse um ROB explícito, a escrita na Memória ocorreria aqui.
                // No modelo implícito, confirmamos o estado.
                // Memory[inst.address] = inst.value; 
            }

            inst.state = InstrState::COMMITTED;
            inst.commit_cycle = current_cycle;
            committed_inst_count++; // Avança a janela de Commit
        }
    }
}
```

## 📊 Status da Instrução e Tempos (Log)

O log final exibe a rastreabilidade completa de cada instrução através do pipeline estendido:

| Coluna | Descrição |
| :--- | :--- |
| **Issue** | Ciclo em que a instrução foi emitida para RS/L/S. |
| **ExecS** | Ciclo em que a execução (na UF ou Endereço) começou. |
| **ExecE** | Ciclo em que a execução terminou (último ciclo antes do WB). |
| **Write** | Ciclo em que o resultado foi transmitido no CDB (Writeback). |
| **Commit** | **Ciclo em que a instrução foi formalmente finalizada no estado arquitetural.** |

---

## 📝 Resultado da Simulação Final (Ciclo 50)

### 1. Instruções de Entrada

O *trace* de instruções demonstrou o tratamento de dependências de dados (`RAW`) e um perigo de memória (`LOAD` após `STORE` no mesmo endereço).

LOAD F6, 4(F1) LOAD F2, 8(F1) ADD F0, F6, F2 MUL F4, F0, F8 SUB F8, F0, F4 STORE F8, 1000(F0) LOAD F6, 1000(F0) // Dependência de memória do STORE acima DIV F4, F0, F2


### 2. Resultados Finais

A simulação completa do *trace* foi concluída em **50 Ciclos**.

--- Simulacao Concluida em 50 Ciclos ---

Valores Finais dos Registradores:
  F0: 0.0000
  F1: 11.0000
  F2: 12.0000
  F3: 13.0000
  F4: 0.0000
  F5: 15.0000
  F6: 16.0000
  F7: 17.0000
  F8: 0.0000

Conteudo Final da Memoria (Enderecos Modificados):
  [1000]: 0.0000
  [1004]: 60.0000
  [1008]: 70.0000

### 3. Tabela Detalhada do Status da Instrução

## 📊 Tabela Detalhada do Status da Instrução

Esta tabela rastreia o ciclo exato em que cada instrução completou os estágios do pipeline estendido (Issue, Execução, Writeback e **Commit**).

| ID | OP | Estado Final | Issue | ExecS | ExecE | Write | **Commit** | Comportamento Observado |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| 0 | LOAD | **COMMITTED** | 1 | 2 | 5 | 5 | **5** | Primeiro Commit. |
| 1 | LOAD | **COMMITTED** | 1 | 2 | 5 | 5 | **6** | Commit em ordem, esperando ID 0. |
| 2 | ADD | **COMMITTED** | 2 | 6 | 7 | 8 | **9** | RAW resolvido. |
| 3 | MUL | **COMMITTED** | 3 | 8 | 17 | 18 | **19** | Longa Execução (10 ciclos). |
| 4 | SUB | **COMMITTED** | 4 | 18 | 19 | 20 | **20** | RAW resolvido. |
| 5 | STORE | **COMMITTED** | 5 | 20 | 23 | 24 | **25** | Escrita na Memória [1000] formalizada. |
| 6 | LOAD | **COMMITTED** | 6 | 25 | 28 | 29 | **30** | **Perigo de Memória:** Esperou o **Commit** do STORE 5 (C.25). |
| 7 | DIV | **COMMITTED** | 7 | 20 | 49 | 50 | **50** | **Execução Fora-de-Ordem:** Começou no C.20, mas só Comitou no C.50. |
