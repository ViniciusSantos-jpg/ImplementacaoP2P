# Rede de Compartilhamento de Arquivos - RCA

Este projeto é uma implementação de uma rede de compartilhamento de arquivos ponto-a-ponto (P2P) em C, desenvolvida para a disciplina de Redes de Computadores. Cada instância do programa funciona simultaneamente como cliente e servidor, permitindo a descoberta de peers, busca e transferência de arquivos em uma rede local.


---

## Funcionalidades

O sistema permite que um usuário realize as seguintes ações:

- **Descoberta de Peers:** Listar todos os usuários (peers) ativos na rede local através de uma solicitação em broadcast.
- **Listagem de Arquivos:** Solicitar e visualizar uma lista consolidada de todos os arquivos compartilhados por todos os peers ativos na rede.
- **Busca de Arquivos:** Procurar por um arquivo específico pelo nome em toda a rede. Os peers que possuem o arquivo respondem com o nome e o tamanho do arquivo.
- **Download de Arquivos:** Baixar um arquivo diretamente de outro peer após localizá-lo. A transferência é feita de forma confiável utilizando o protocolo TCP.
- **Logging:** Todas as solicitações recebidas pelo servidor e as transferências de arquivos concluídas são registradas em um arquivo `log.txt`, incluindo IP do solicitante, tipo de solicitação, data/hora e tempo da transferência.

---

## Tecnologias e Arquitetura

- **Linguagem:** C
- **Protocolos de Rede:**
  - **UDP:** Utilizado para todas as mensagens de controle, como descoberta de peers e busca de arquivos, aproveitando sua capacidade de broadcast.
  - **TCP:** Utilizado para a transferência confiável de arquivos entre os peers.
- **Concorrência:**
  - O programa utiliza **Pthreads (POSIX Threads)** para gerenciar a concorrência.
  - Uma thread principal gerencia a interface do cliente, enquanto threads separadas em background cuidam das funcionalidades de servidor UDP e servidor TCP.
  - Para cada transferência de arquivo via TCP, uma nova thread dedicada é criada, permitindo que múltiplos downloads ocorram simultaneamente sem bloquear o servidor.

---

## Requisitos do Ambiente

Para compilar e executar este projeto, você precisará de um ambiente com as seguintes características:

- **Sistema Operacional:** Linux (o programa foi projetado e testado para este ambiente, conforme especificado).
- **Compilador:** `gcc`.
- **Ferramentas de Build:** `make`.
- **Bibliotecas:** Um ambiente de desenvolvimento C padrão para Linux, que geralmente inclui as bibliotecas necessárias. Em sistemas baseados em Debian/Ubuntu, a instalação do pacote `build-essential` deve ser suficiente para prover todas as dependências (como `pthreads`).

```bash
# Em sistemas Debian/Ubuntu, para instalar as ferramentas necessárias:
sudo apt update
sudo apt install build-essential
```

---

## Compilação e Execução

### 1. Clone o Repositório (se aplicável):
```bash
git clone <url-do-seu-repositorio>
cd <nome-do-repositorio>
```

### 2. Compile o Programa:
O projeto inclui um Makefile que automatiza o processo de compilação. Para compilar, execute:
```bash
make
```
Isso criará um executável chamado `rca`.

### 3. Execute o Programa:
Para simular a rede P2P, você deve executar o programa em múltiplos terminais ou em diferentes Máquinas Virtuais (VMs) que estejam na mesma rede local (configuradas como "Adaptador em Ponte" ou "Rede Interna").

Execute o programa em cada terminal/VM com o seguinte comando:
```bash
./rca
```

**Importante:** O programa compartilha os arquivos que estão no mesmo diretório de onde ele é executado. Portanto, coloque os arquivos que deseja compartilhar nesta pasta antes de executar.

### 4. Limpando os Arquivos Gerados:
Para remover o executável compilado (`rca`), os arquivos objeto (`.o`) e o arquivo de log (`log.txt`), utilize o comando:
```bash
make clean
```

---

## Estrutura do Projeto

```
.
├── rca                 # Executável (gerado após a compilação)
├── rca.c               # Código fonte principal da aplicação
├── Makefile            # Arquivo de compilação
└── README.md           # Este arquivo
```

**log.txt:** Será gerado no diretório durante a execução, contendo os logs das atividades do servidor.
