Simulador do Algoritmo de Tomasulo em C++ (COMPLETO)

Este projeto é um simulador de pipeline superescalar baseado no algoritmo de Tomasulo. Ele exibe o estado completo do pipeline (estações de reserva e status dos registradores) ciclo a ciclo diretamente no terminal.

Configuração do Simulador

Unidade Funcional

Estações de Reserva

Latência (Ciclos)

ADD/SUB

3

2

MUL

2

10

DIV

2

40

Como Compilar e Executar

Compilar: Use o g++ com o padrão C++11.

g++ -o simulador main.cpp simulator.cpp -std=c++11


Executar: Passe o arquivo de instruções como argumento.

./simulador instructions.txt


Explicação Detalhada do Código e dos Estágios

O código completo segue rigorosamente o algoritmo de Tomasulo, implementado nos três estágios principais:

1. Estágio de Issue (Emissão)

Lógica: A instrução é lida da fila (instruction_queue) e colocada na primeira Estação de Reserva (RS) livre.

Renomeação de Registradores: É o ponto onde as dependências RAW (Read After Write) são resolvidas.

Se um operando (ex: F0) estiver pronto (reg_status["F0"] == ""), seu valor (reg_file["F0"]) é copiado para Vj ou Vk da RS.

Se o operando estiver sendo produzido por outra RS (ex: reg_status["F2"] == "Mult1"), o nome do produtor (Mult1) é copiado para Qj ou Qk da RS, forçando a espera.

Status (Qi): O registrador de destino (DEST) da instrução é marcado com o nome da RS recém-emitida, forçando as instruções futuras a esperarem por ela.

2. Estágio de Execute (Execução)

Lógica: A execução só começa se a RS estiver busy E se ambos os campos Qj e Qk estiverem vazios (""), indicando que todos os operandos estão disponíveis (seja por valor Vj/Vk ou recebidos via CDB).

Latência:

Ao iniciar, cycles_remaining é definido (2 para ADD/SUB, 10 para MUL, 40 para DIV).

O contador é decrementado a cada ciclo.

Quando cycles_remaining chega a 0, o resultado (rs.result) é calculado e a RS é marcada como pronta para escrita (ready_to_writeback = true).

3. Estágio de Writeback (Escrita)

Lógica: Uma única RS marcada com ready_to_writeback = true é selecionada para escrever no Barramento Comum de Dados (CDB).

Broadcast (CDB): O resultado é transmitido.

Atualização dos Registradores: O resultado é gravado no reg_file[DEST], mas somente se o reg_status[DEST] ainda apontar para a RS que está escrevendo (proteção contra Waw e War). O status é limpo para Pronto.

Atualização das RSs em Espera: O resultado é copiado para os campos Vj ou Vk de todas as outras RSs que estavam esperando pelo nome do produtor (limpando o Qj ou Qk correspondente).

A RS que acabou de escrever é totalmente limpa e liberada (busy = false).

Como Ler a Saída do Log (Terminal)

O log é a parte mais importante, mostrando o estado em tempo real:

Coluna

Descrição

Valores de Exemplo

Busy

Indica se a RS está processando uma instrução.

Sim / Nao

Op

A operação em execução (ADD, MUL, etc.).

ADD / DIV

Vj/Vk

O valor do operando, se estiver pronto (foi lido do registrador ou via CDB).

10.00 / 5.50

Qj/Qk

O nome da RS que produzirá o operando, se a RS estiver esperando por ele.

Add1 / Mult2 /           (vazio=pronto)

Ciclos

Estado atual da execução.

WB (Pronto para Writeback) / 2 (Faltam 2 ciclos) / - (Execução não iniciada)

Qi

No Status dos Registradores, mostra quem produz o valor do registrador.

Pronto (valor já no registrador) / Mult1 (Mult1 produzirá o valor)

Simulação de Exemplo e Resultados Finais

O log de saída detalhado será gerado no terminal após a execução do programa.

1. Instruções de Entrada

As instruções no arquivo instructions.txt são:

ADD F6 F0 F2
SUB F7 F0 F3
MUL F2 F4 F5
ADD F1 F2 F0
SUB F8 F7 F1
DIV F3 F8 F2


2. Exemplo de Saída (Ciclo 1)

O simulador imprime o que aconteceu em cada estágio. No Ciclo 1, apenas a primeira instrução é emitida (ADD F6 F0 F2):

--- CICLO 1 ---
  [WB] Ninguem transmitindo resultado.
  [EXEC] Nenhuma RS pronta para iniciar/decrementar.
  [ISSUE] Emitindo ADD F6,F0,F2 para Add1
  
  Estacoes de Reserva (Add/Sub):
    Nome  | Busy  | Op   |    Vj    |    Vk    |  Qj    |  Qk    | Ciclos
    ----------------------------------------------------------------------
    Add1  | Sim   | ADD  |  10.00   |   2.00   |        |        | -
    Add2  | Nao   |      |          |          |        |        | -
    Add3  | Nao   |      |          |          |        |        | -
  
  Status dos Registradores (Qi):
    | F0: Pronto | F1: Pronto | F2: Pronto | F3: Pronto | F4: Pronto | F5: Pronto | F6: Add1 | F7: Pronto | F8: Pronto |


3. Exemplo de Resultados Finais

Ao final da simulação (após todos os estágios de Writeback), o simulador exibirá o tempo total e os valores finais dos registradores. O formato final será:

--- Simulacao Concluida ---
Valores Finais dos Registradores:
  F0: 10.0000
  F1: [Valor final de F1]
  F2: [Valor final de F2]
  F3: [Valor final de F3]
  ...
