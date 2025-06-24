# Implementação de Rede P2P em C


##  Visão Geral

Este projeto consiste na implementação de uma rede P2P (ponto-a-ponto) em linguagem C, desenvolvida para a disciplina de Redes de Computadores. O sistema permite que cada nó na rede atue tanto como cliente quanto como servidor, possibilitando a descoberta de outros pares, a busca e a transferência de arquivos de forma descentralizada.

##  Funcionalidades

* **Descoberta de Pares:** Nós descobrem outros pares ativos na rede local através de requisições em broadcast.
* **Lista de Arquivos:** Visualização de uma lista consolidada de arquivos compartilhados por todos os pares ativos na rede.
* **Busca de Arquivos:** Busca por arquivos específicos pelo nome. Os pares que possuem o arquivo respondem com seus detalhes.
* **Download de Arquivos:** Suporte ao download de arquivos diretamente de outros pares utilizando TCP para uma transferência de dados confiável.
* **Logs de Operações:** Todas as requisições e transferências são registradas em `log.txt`, incluindo IP do requisitante, tipo de requisição, data, hora e duração da transferência.

##  Como usar

O projeto utiliza um `Makefile` para facilitar a compilação e execução.

1.  **Clone o repositório:**
    ```bash
    git clone [https://github.com/ViniciusSantos-jpg/ImplementacaoP2P.git](https://github.com/ViniciusSantos-jpg/ImplementacaoP2P.git)
    cd ImplementacaoP2P
    ```

2.  **Compile o projeto:**
    Este comando utiliza o `gcc` para compilar o `main.c` e gerar o executável `p2p`.
    ```bash
    make
    ```

3.  **Execute o programa:**
    Este comando inicia o nó P2P.
    ```bash
    make executar
    ```
    *Alternativamente, você pode executar diretamente com `./p2p` após a compilação.*

4.  **Limpar arquivos gerados:**
    Para remover o executável, o arquivo de log e os arquivos baixados, utilize o comando:
    ```bash
    make limpar
    ```
## Estrutura do Projeto

* `main.c`: Código principal da aplicação.
* `files/`: Diretório que contém os arquivos a serem compartilhados.
* `downloads/`: Diretório onde os arquivos baixados são salvos.
* `log.txt`: Arquivo de log das operações.
