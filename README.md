# 🚀 Simulador do Algoritmo de Tomasulo com Comprometimento (Commitment)

Este projeto implementa um **simulador completo do algoritmo de Tomasulo** com suporte à **Execução Fora de Ordem (Out-of-Order Execution)** e **Comprometimento em Ordem (In-Order Commitment)**.  
A simulação reproduz o comportamento de um **pipeline superescalar**, evidenciando como dependências de dados, perigos estruturais e de memória são tratados dinamicamente.

---

## ⚙️ Arquitetura Simulada e Configurações

| Componente | Estações de Reserva (Slots) | Latência (Ciclos) | Observações |
| :--- | :---: | :---: | :--- |
| **ADD/SUB** | 3 | 2 | Unidade de adição e subtração |
| **MUL/DIV** | 2 | 10 (MUL) / 40 (DIV) | Unidade de multiplicação e divisão |
| **L/S Buffers** | 2 | 3 | Buffers para LOAD e STORE |
| **Commit Queue (ROB implícito)** | Fila de Instruções | — | Mantém a ordem de término (*commit in-order*) |

---

## 💾 Estado Inicial do Sistema

### 🧮 Registradores (F0–F8)
Inicializados sequencialmente do F1 ao F7:
```
F0 = 100.0
F1 = 11.0
F2 = 12.0
F3 = 13.0
F4 = 14.0
F5 = 15.0
F6 = 16.0
F7 = 17.0
F8 = 2.0
```

### 🧠 Memória Simulada
A memória foi inicializada com os seguintes valores:
```
[132]  = 10.0     // endereço base 100 + offset 32
[136]  = 20.0     // endereço base 100 + offset 36
[1000] = 50.0
[1004] = 60.0
[1008] = 70.0
```

Esses valores são utilizados por instruções LOAD e STORE durante a simulação.

---

## 🚀 Compilação e Execução

### 🧩 Compilar:
```bash
g++ -o simulador main.cpp simulator.cpp -std=c++14
```

### ▶️ Executar:
```bash
./simulador instructions.txt
```

O arquivo `instructions.txt` deve conter uma lista de instruções em formato texto, por exemplo:

```
LOAD F6, 32(F1)
LOAD F2, 36(F1)
ADD F0, F6, F2
MUL F4, F0, F8
SUB F8, F0, F4
STORE F8, 1000(F0)
LOAD F6, 1000(F0)
DIV F4, F0, F2
```

---

## 🔁 Ciclo de Execução do Pipeline

A execução do simulador segue rigorosamente a sequência:

> **COMMIT → WRITEBACK → EXECUTE → ISSUE**

---

### 1️⃣ Commit (Comprometimento)
- Garante a **terminação em ordem** (In-Order).
- Apenas instruções no estado `WRITE_RESULT` são promovidas para `COMMITTED`.
- **STORE** modifica efetivamente a memória somente neste ponto, garantindo consistência arquitetural.
- Implementa a função de um **ROB implícito**.

---

### 2️⃣ Writeback (CDB)
- Escolhe **uma** estação de reserva ou buffer de memória pronto e transmite o resultado pelo **Barramento Comum de Dados (CDB)**.
- Propaga o valor para todas as estações que dependem dele (`Qj`, `Qk`).
- Libera a estação de reserva após o broadcast.
- Atualiza o estado da instrução para `WRITE_RESULT`.

---

### 3️⃣ Execute
- Inicia ou continua a execução se os operandos (`Qj`, `Qk`) estiverem resolvidos.
- **LOAD/STORE:**
  - Calcula o endereço quando o registrador base está pronto.
  - Detecta e respeita **perigos de memória** (RAW, WAR).
  - LOAD aguarda STOREs pendentes para o mesmo endereço.

---

### 4️⃣ Issue
- Emite a próxima instrução para uma **RS** (ADD/SUB/MUL/DIV) ou **L/S buffer** disponível.
- Faz a **renomeação de registradores** (`Qi`) para tratar dependências de dados.
- Marca o início da instrução (`issue_cycle`).

---

## 📊 Status da Simulação Final (Ciclo 50)

```
--- Simulação Concluída em 50 Ciclos ---
```

### 🧩 Tabela Completa das Instruções

| ID | OP | Estado Final | Issue | ExecS | ExecE | Write | Commit | Observação |
| :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-: | :-- |
| 0 | LOAD | **COMMITTED** | 1 | 2 | 4 | 5 | 6 | Carregou F6 ← [132] = 10.0 |
| 1 | LOAD | **COMMITTED** | 2 | 3 | 5 | 6 | 7 | Carregou F2 ← [136] = 20.0 |
| 2 | ADD | **COMMITTED** | 3 | 7 | 8 | 9 | 10 | F0 ← F6 + F2 = 30.0 |
| 3 | MUL | **COMMITTED** | 4 | 9 | 18 | 19 | 20 | F4 ← F0 * F8 = 30 * 18 = 540.0 |
| 4 | SUB | **COMMITTED** | 5 | 19 | 20 | 21 | 22 | F8 ← F0 - F4 = 30 - 540 = -510.0 |
| 5 | STORE | **COMMITTED** | 6 | 9 | 23 | 24 | 25 | Mem[1000 + F0(30)] = Mem[1030] ← -30.0 |
| 6 | LOAD | **COMMITTED** | 7 | 9 | 25 | 26 | 27 | F6 ← [1030] = -30.0 |
| 7 | DIV | **COMMITTED** | 8 | 9 | 48 | 49 | 50 | F4 ← F0 / F2 = 1.5 |

---

## 🧾 Resultado Final da Simulação

### 📈 Valores dos Registradores
```
F0 = 30.0000
F1 = 100.0000
F2 = 20.0000
F3 = 13.0000
F4 = 1.5000
F5 = 15.0000
F6 = -30.0000
F7 = 17.0000
F8 = -30.0000
```

---

### 💾 Conteúdo Final da Memória
```
[132]  = 10.0000
[136]  = 20.0000
[1000] = 50.0000
[1004] = 60.0000
[1008] = 70.0000
[1030] = -30.0000
```

> 💡 O endereço **[1030]** é calculado pela instrução  
> `STORE F8, 1000(F0)` → 1000 + F0(30) = **1030**  
> e o valor armazenado foi `F8 = -30.0`.

### 🧩 Exemplo de Execução (Resumo Final)

```
--- CICLO 50 ---
[COMMIT] WRITE F4 = 1.5000 (ID7)
--- Simulação Concluída em 50 Ciclos ---
```

---

### ✅ Resultado Final
| Registrador | Valor Final |
| :----------- | -----------: |
| F0 | 30.0000 |
| F1 | 100.0000 |
| F2 | 20.0000 |
| F4 | 1.5000 |
| F6 | -30.0000 |
| F8 | -30.0000 |

| Endereço | Valor Final |
| :-------- | ----------: |
| 132 | 10.0000 |
| 136 | 20.0000 |
| 1030 | -30.0000 |
