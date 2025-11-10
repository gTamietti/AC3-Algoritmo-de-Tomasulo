# 💻 Simulador do Algoritmo de Tomasulo em C++

Este projeto implementa um simulador de pipeline superescalar com execução fora de ordem, seguindo o rigoroso algoritmo de Tomasulo. Ele é projetado para exibir o estado completo do pipeline (Estações de Reserva e Status dos Registradores) **ciclo a ciclo** no terminal.

## ⚙️ Configuração do Simulador

A arquitetura simulada possui a seguinte capacidade e latência:

| Unidade Funcional | Estações de Reserva (Slots) | Latência (Ciclos) |
| :---------------- | :-------------------------- | :---------------- |
| ADD/SUB           | 3                           | 2                 |
| MUL               | 2                           | 10                |
| DIV               | 2                           | 40                |

## 🚀 Como Compilar e Executar

1.  **Compilar:** Utilize o compilador `g++` com o padrão C++11 (ou superior).

    ```bash
    g++ -o simulador main.cpp simulator.cpp -std=c++11
    ```

2.  **Executar:** Passe o arquivo de texto contendo a lista de instruções (ex: `instructions.txt`) como argumento:

    ```bash
    ./simulador instructions.txt
    ```

***

## 🧠 Explicação Detalhada do Algoritmo e dos Estágios

O simulador implementa os três estágios de pipeline do algoritmo de Tomasulo, que permitem a execução fora de ordem e o tratamento de dependências.

### 1. Estágio de *Issue* (Emissão)

-   **Lógica:** A instrução é lida da fila e alocada na primeira Estação de Reserva (RS) livre.
-   **Renomeação de Registradores (Resolução de RAW):**
    -   Se o operando estiver pronto, seu **valor** é copiado para `Vj` ou `Vk`.
    -   Se o operando estiver sendo produzido por outra RS (ex: `Mult1`), o **nome do produtor** é copiado para `Qj` ou `Qk`, forçando a espera.
-   **Status (Qi):** O registrador de destino (`DEST`) é marcado com o nome da RS recém-emitida (ex: `F6: Add1`), implementando a renomeação.

### 2. Estágio de *Execute* (Execução)

-   **Condição de Início:** A execução só começa se a RS estiver `busy` **E** se `Qj` e `Qk` estiverem vazios (`""`).
-   **Latência de Execução:** O contador regressivo (`cycles_remaining`) é iniciado com a latência da UF. Quando chega a `0`, o resultado é calculado e a RS é marcada com `ready_to_writeback = true`.

### 3. Estágio de *Writeback* (Escrita)

-   **Lógica:** Uma única RS pronta é selecionada para escrever no Barramento Comum de Dados (CDB).
-   **Broadcast (CDB):** O resultado é transmitido para:
    1.  **Banco de Registradores:** O resultado é gravado no `reg_file[DEST]` **somente** se o `reg_status[DEST]` ainda apontar para a RS que está escrevendo (proteção contra perigos WAW e WAR).
    2.  **Estações de Reserva em Espera:** O resultado é copiado para os campos `Vj` ou `Vk` de todas as outras RSs que estavam esperando por esse valor (o campo `Qj` ou `Qk` é limpo).
-   A RS que transmitiu é limpa e liberada (`busy = false`).

***

## 📊 Como Ler a Saída do Log (Terminal)

O log é a parte mais importante, mostrando o estado em tempo real:

| Coluna | Descrição | Valores de Exemplo |
| :----- | :--- | :--- |
| **Busy** | Indica se a RS está ocupada. | `Sim` / `Nao` |
| **Op** | A operação em execução. | `ADD` / `DIV` |
| **Vj/Vk** | O **valor** do operando, se estiver pronto. | `10.00` / `5.50` |
| **Qj/Qk** | O **nome da RS** que produzirá o operando (se a RS estiver esperando). | `Add1` / `Mult2` / *vazio* (`""`) |
| **Ciclos** | Estado atual da execução. | `WB` (Pronto p/ Escrita) / `2` (Faltam 2 ciclos) |
| **Qi** | No Status dos Registradores, indica o produtor do valor. | `Pronto` / `Mult1` |

***

## 📝 Exemplo de Simulação

### 1. Instruções de Entrada (`instructions.txt`)

ADD F6 F0 F2 SUB F7 F0 F3 MUL F2 F4 F5 ADD F1 F2 F0 SUB F8 F7 F1 DIV F3 F8 F2


### 2. Exemplo de Saída (Ciclo 1)

O log mostra a emissão da primeira instrução e o mapeamento do registrador de destino (`F6: Add1`):

--- CICLO 1 --- [WB] Ninguem transmitindo resultado. [EXEC] Nenhuma RS pronta para iniciar/decrementar. [ISSUE] Emitindo (1/6) ADD F6,F0,F2 para Add1

### 📝 Estações de Reserva (Janela de Instruções)

| Nome | Busy | Op | Vj | Vk | Qj | Qk | Ciclos |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| Add1 | Sim | ADD | 10.00 | 2.00 | "" | "" | - |
| Add2 | Nao | "" | "" | "" | "" | "" | - |
| Add3 | Nao | "" | "" | "" | "" | "" | - |
| Mult1 | Nao | "" | "" | "" | "" | "" | - |
| Mult2 | Nao | "" | "" | "" | "" | "" | - |

***

#### Legenda da Tabela:

* **Vj/Vk**: O **valor** do operando, se estiver pronto para uso. Se houver uma dependência, será substituído por `""`.
* **Qj/Qk**: **Nome da Estação de Reserva** que irá produzir o operando necessário (dependência RAW). `""` (vazio) significa que o operando está pronto.
* **Ciclos**:
    * `-`: Não iniciado, ou a RS está livre.
    * `Nº > 0`: Ciclos restantes de execução.
    * `WB`: A execução terminou e a instrução está pronta para o *Writeback* no CDB.
--- Status dos Registradores (Qi) --- | F0: Pronto | F1: Pronto | F2: Pronto | F3: Pronto | ... | F6: Add1 | ... |

--- Fila de Instrucoes (PC=2/6) --- Proxima a emitir: SUB F7,F0,F3


### 4. Exemplo de Resultados Finais

Após o processamento de todas as instruções e o término do último estágio de *Writeback*, o simulador exibe o tempo total da execução e o estado final do **Banco de Registradores** (`reg_file`).

| Detalhe | Valor | Significado |
| :--- | :--- | :--- |
| **Total de Ciclos** | **58** | O tempo total de clock (latência) necessário para a conclusão de todo o programa. |

#### Valores Finais dos Registradores (F0 - F8)

Esta tabela mostra o resultado de todos os cálculos realizados pelas instruções:

F0: 10.0000 (Valor Inicial) F1: 30.0000 F2: 20.0000 F3: -1.5000 F4: 4.0000 (Valor Inicial) F5: 5.0000 (Valor Inicial) F6: 12.0000 F7: 7.0000 F8: -30.0000


***

#### Interpretação dos Resultados Finais:

* **Valores Modificados:** Registradores como `F1`, `F2`, `F3`, `F6`, `F7` e `F8` contêm os resultados finais das operações (ADD, SUB, MUL, DIV), demonstrando que as instruções foram executadas com sucesso, mesmo que **fora de ordem**.
* **Valores Iniciais Preservados:** Registradores como `F0`, `F4` e `F5` mantiveram seus valores originais porque não foram alvos de nenhuma instrução de escrita (`DEST`).
* **Prova de Tomasulo:** O fato de o tempo total ser **58 ciclos** (e não a soma sequencial de todas as latências) prova que a execução foi **paralela** e **fora de ordem**, com o algoritmo Tomasulo eliminando as dependências de forma eficiente.
