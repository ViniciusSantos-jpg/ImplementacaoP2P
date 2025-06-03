// TRABALHO 1 - REDES DE COMPUTADORES 2025.1
// ALUNO: VINÍCIUS SANTOS SILVA
// VERSÃO: FINAL - COMENTADA

#include <stdio.h>      // Para entrada e saída padrão (printf, scanf, perror, fopen, etc.)
#include <stdlib.h>     // Para funções utilitárias gerais (exit, malloc, free, etc.)
#include <stdint.h>     // Para tipos inteiros de tamanho fixo (uint8_t, uint32_t)
#include <string.h>     // Para manipulação de strings (strcpy, strncpy, strlen, strcmp, etc.)
#include <unistd.h>     // Para chamadas de sistema POSIX (close, read, write, fork, etc.)
#include <pthread.h>    // Para programação com threads POSIX (pthread_create, pthread_join, etc.)
#include <errno.h>      // Para acesso à variável errno, que indica erros de sistema
#include <dirent.h>     // Para manipulação de diretórios (opendir, readdir, closedir)
#include <sys/types.h>  // Para tipos de dados primitivos do sistema (usado por sys/stat.h e sys/socket.h)
#include <sys/stat.h>   // Para obter informações de arquivos (stat, S_ISREG)
#include <sys/socket.h> // Para programação com sockets (socket, bind, listen, accept, send, recv, etc.)
#include <arpa/inet.h>  // Para manipulação de endereços IP (inet_ntoa, inet_pton, htons, htonl)
#include <netinet/in.h> // Para estruturas de endereço de internet (struct sockaddr_in)
#include <sys/time.h>   // Para manipulação de tempo (struct timeval, usado em setsockopt SO_RCVTIMEO)
#include <time.h>       // Para funções de data e hora (time, ctime)
#include <fcntl.h>      // Para controle de arquivos (não usado explicitamente aqui, mas útil em Sockets)

#define PORTA 5555                // Porta que os peers vão usar pra se comunicar. 
#define TAM_NOME_ARQ 100          // Tamanho máximo pro nome de um arquivo. 
#define TAM_BUFFER 1024           // Tamanho do buffer pra ler e enviar pedaços de arquivos.

#define LISTA_USUARIOS 1          // Solicitação/Resposta para listar usuários ativos. 
#define LISTA_ARQUIVOS 2          // Solicitação/Resposta para listar arquivos de um peer. 
#define PROCURA_ARQUIVO 3         // Solicitação para procurar um arquivo específico. 
#define RESPOSTA_ARQUIVO 4        // Resposta indicando que um peer tem o arquivo e seu tamanho. 
#define TRANSFERENCIA_ARQUIVO 5   // Tipo usado no log para indicar uma transferência TCP. 

// Estrutura do pacote UDP.
// O __attribute__((packed)) tenta evitar que o compilador coloque "espaços" extras entre os campos. 
typedef struct {
    uint8_t tipo_msg;                   // Que tipo de mensagem é essa? (1 byte).
    char nome_arquivo[TAM_NOME_ARQ];    // Nome do arquivo (se aplicável à mensagem).
    uint32_t tamanho_arquivo;           // Tamanho do arquivo (usado na RESPOSTA_ARQUIVO, 4 bytes).
} __attribute__((packed)) mensagem_udp_t;

// Funçãozinha pra converter o número do tipo de mensagem em uma string legível.
const char* tipo_msg_str(uint8_t tipo) {
    switch (tipo) { // Checa o tipo...
        case LISTA_USUARIOS: return "LISTA_USUARIOS";       // Se for 1, é lista de usuários.
        case LISTA_ARQUIVOS: return "LISTA_ARQUIVOS";       // Se for 2, é lista de arquivos.
        case PROCURA_ARQUIVO: return "PROCURA_ARQUIVO";     // Se for 3, é procura de arquivo.
        case RESPOSTA_ARQUIVO: return "RESPOSTA_ARQUIVO";   // Se for 4, é resposta de arquivo.
        case TRANSFERENCIA_ARQUIVO: return "TRANSFERENCIA_ARQUIVO"; // Se for 5, é transferência.
        default: return "DESCONHECIDA";                     // Se não for nenhum desses, não sabemos o que é.
    }
}

// Função para registrar eventos importantes em um arquivo de log.
// IP de quem fez a solicitação, tipo da mensagem, nome do arquivo (se tiver) e tempo (se for transferência).
void registrar_log(const char* ip, uint8_t tipo_msg, const char* nome_arquivo, double tempo_transferencia) {
    FILE* f = fopen("log.txt", "a"); // Tenta abrir (ou criar se não existir) o log.txt para adicionar no final ("a" de append).
    if (!f) { // Se não conseguiu abrir...
        perror("Não foi possível abrir o arquivo de log"); // Avisa no console qual foi o problema.
        return; // E sai da função, não tem o que fazer.
    }

    time_t agora = time(NULL);          // Pega o timestamp atual (segundos desde 1970).
    char* data_str = ctime(&agora);     // Converte esse timestamp pra uma string formatada (ex: "Mon Apr 12 10:30:00 2021\n").
    data_str[strcspn(data_str, "\n")] = 0; // Remove o caractere de nova linha ('\n') que o ctime coloca no final.

    // Escreve a informação básica no log: Data, IP e Tipo da mensagem.
    fprintf(f, "[%s] IP: %s | Tipo: %s", data_str, ip, tipo_msg_str(tipo_msg));

    // Se um nome de arquivo foi fornecido e não está vazio...
    if (nome_arquivo && strlen(nome_arquivo) > 0) {
        fprintf(f, " | Arquivo: %s", nome_arquivo); // Adiciona o nome do arquivo no log.
    }

    // Se o tempo de transferência for um valor válido (>= 0)...
    // (-1.0 para indicar que não é uma transferência de arquivo)
    if (tempo_transferencia >= 0) {
        fprintf(f, " | Tempo: %.2f seg", tempo_transferencia); // Adiciona o tempo no log.
    }
    fprintf(f, "\n"); // Pula uma linha no arquivo de log para a próxima entrada.
    fclose(f); // Fecha o arquivo de log. Muito importante pra garantir que tudo foi escrito!
}

// Função para listar os arquivos no diretório corrente.
// Coloca os nomes dos arquivos num array e diz quantos são.
void listar_arquivos(char arquivos[][TAM_NOME_ARQ], int* total_arquivos_encontrados) {
    DIR* dir = opendir("."); // Tenta abrir o diretório atual (representado por ".").
    if (!dir) { // Se não conseguiu abrir...
        perror("Erro ao abrir o diretório atual pra listar arquivos"); // Mostra o erro.
        *total_arquivos_encontrados = 0; // Diz que não encontrou nenhum arquivo.
        return; // sai.
    }

    struct dirent* entry; // Estrutura para guardar informações sobre cada item no diretório.
    *total_arquivos_encontrados = 0; // Começa contando do zero.

    // Loop para ler cada entrada (arquivo/subdiretório) no diretório.
    // Continua enquanto readdir não retornar NULL (fim do diretório) e não passar de 100 arquivos (limite do array).
    while ((entry = readdir(dir)) != NULL && *total_arquivos_encontrados < 100) {
        struct stat st; // Estrutura para guardar informações detalhadas do arquivo (tipo, tamanho, etc.).
        // Pega as informações do item atual (entry->d_name é o nome do item).
        if (stat(entry->d_name, &st) == 0) { // Se stat funcionou (retornou 0)...
            // S_ISREG verifica se é um arquivo regular (não um diretório, link, etc.).
            if (S_ISREG(st.st_mode)) {
                // Copia o nome do arquivo para o array de arquivos.
                // strncpy é mais seguro que strcpy pra evitar estourar o buffer.
                strncpy(arquivos[*total_arquivos_encontrados], entry->d_name, TAM_NOME_ARQ -1);
                arquivos[*total_arquivos_encontrados][TAM_NOME_ARQ -1] = '\0'; // Garante terminação nula.
                (*total_arquivos_encontrados)++; // Incrementa o contador de arquivos.
            }
        }
    }
    closedir(dir); // Fecha o diretório.
}

// Thread para cuidar de uma conexão TCP de ENVIO de arquivo. 
// Cada cliente que pede um arquivo vai ter uma dessa rodando.
void* servidor_tcp_thread_envio(void* arg_socket_conectado) {
    int socket_conexao = *(int*)arg_socket_conectado; // Pega o número do socket da conexão que foi passado como argumento.
    free(arg_socket_conectado); // Libera a memória que foi alocada pra passar o argumento.

    char nome_arquivo_pedido[TAM_NOME_ARQ]; // Espaço para guardar o nome do arquivo que o cliente quer.
    // Tenta receber o nome do arquivo do cliente.
    if (recv(socket_conexao, nome_arquivo_pedido, TAM_NOME_ARQ, 0) <= 0) {
        // Se recv retornar 0 ou menos, o cliente desconectou ou deu erro.
        perror("[TCP-ENVIO] Erro ao receber nome do arquivo ou cliente desconectou");
        close(socket_conexao); // Fecha a conexão com esse cliente.
        return NULL; // Termina a thread.
    }
    nome_arquivo_pedido[TAM_NOME_ARQ -1] = '\0'; // Garante que o nome do arquivo é uma string válida.

    // Tenta abrir o arquivo pedido em modo de leitura binária ("rb").
    FILE* f = fopen(nome_arquivo_pedido, "rb");
    if (!f) { // Se não conseguiu abrir o arquivo (não existe, sem permissão, etc.)...
        perror("[TCP-ENVIO] Erro ao abrir arquivo para transferencia"); // Mostra o erro.
        close(socket_conexao); // Fecha a conexão.
        return NULL; // Termina a thread.
    }

    struct sockaddr_in endereco_peer; // Estrutura para guardar o endereço do cliente.
    socklen_t tamanho_endereco_peer = sizeof(endereco_peer); // Tamanho da estrutura.
    // Pega o endereço IP e porta do cliente conectado a este socket.
    getpeername(socket_conexao, (struct sockaddr*)&endereco_peer, &tamanho_endereco_peer);
    char* ip_peer = inet_ntoa(endereco_peer.sin_addr); // Converte o IP para string.

    char buffer_envio[TAM_BUFFER]; // Buffer para ler pedaços do arquivo e enviar.
    size_t bytes_lidos_do_arquivo; // Quantos bytes foram lidos do arquivo em cada passada.
    clock_t tempo_inicio_transferencia = clock(); // Marca o tempo de início da transferência.

    // Loop para ler do arquivo e enviar para o cliente.
    // fread lê 'TAM_BUFFER' blocos de 1 byte do arquivo 'f' e coloca no 'buffer_envio'.
    // Retorna quantos bytes realmente leu (pode ser menos que TAM_BUFFER no final do arquivo).
    while ((bytes_lidos_do_arquivo = fread(buffer_envio, 1, TAM_BUFFER, f)) > 0) {
        // Tenta enviar os bytes lidos para o cliente através do socket.
        if (send(socket_conexao, buffer_envio, bytes_lidos_do_arquivo, 0) < 0) {
            perror("[TCP-ENVIO] Erro ao enviar dados do arquivo"); // Se deu erro, avisa.
            break; // Interrompe o loop de envio.
        }
    }

    clock_t tempo_fim_transferencia = clock(); // Marca o tempo de fim da transferência.
    // Calcula o tempo total gasto em segundos.
    double tempo_gasto_segundos = (double)(tempo_fim_transferencia - tempo_inicio_transferencia) / CLOCKS_PER_SEC;
    // Registra no log que a transferência foi concluída.
    registrar_log(ip_peer, TRANSFERENCIA_ARQUIVO, nome_arquivo_pedido, tempo_gasto_segundos);

    fclose(f); // Fecha o arquivo que estava sendo lido.
    close(socket_conexao); // Fecha a conexão com o cliente.
    printf("[TCP-ENVIO] Transferência do arquivo '%s' para %s concluída em %.2f segundos.\n", nome_arquivo_pedido, ip_peer, tempo_gasto_segundos);
    return NULL; // Termina a thread.
}

// Thread principal do servidor TCP. Fica esperando por pedidos de conexão.
// Quando um cliente conecta, cria uma nova thread (servidor_tcp_thread_envio) só pra ele.
void* servidor_tcp(void* arg) {
    (void)arg; // Ignora o argumento, não usando.

    int socket_escuta_tcp; // Socket que vai ficar "escutando" por conexões TCP.
    int socket_nova_conexao; // Socket para a nova conexão quando um cliente se conectar.
    struct sockaddr_in endereco_servidor_tcp; // Endereço do nosso servidor TCP.
    struct sockaddr_in endereco_cliente_tcp;  // Endereço do cliente que se conectar.
    socklen_t tamanho_endereco_cliente = sizeof(endereco_cliente_tcp); // Tamanho da estrutura de endereço do cliente.

    // Cria o socket TCP: AF_INET (Internet IPv4), SOCK_STREAM (TCP, confiável).
    socket_escuta_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_escuta_tcp < 0) { // Se deu erro ao criar o socket...
        perror("[TCP] Erro ao criar socket TCP de escuta"); // Avisa.
        exit(EXIT_FAILURE); // Aborta o programa.
    }

    // Opção para permitir que o socket seja reutilizado rapido depois do programa fechar.
    // Para testes.
    int opt_reusar_endereco = 1;
    if (setsockopt(socket_escuta_tcp, SOL_SOCKET, SO_REUSEADDR, &opt_reusar_endereco, sizeof(opt_reusar_endereco)) < 0) {
        perror("[TCP] Erro no setsockopt SO_REUSEADDR"); // Se deu erro, avisa.
        close(socket_escuta_tcp); // Fecha o socket antes de sair.
        exit(EXIT_FAILURE); // Aborta.
    }

    // Configura o endereço do servidor TCP.
    endereco_servidor_tcp.sin_family = AF_INET;           // endereços IPv4.
    endereco_servidor_tcp.sin_port = htons(PORTA);        // Porta de escuta
    endereco_servidor_tcp.sin_addr.s_addr = INADDR_ANY; // Aceita conexões de qualquer interface de rede da máquina.

    // Associa (bind) o socket ao endereço e porta configurados.
    if (bind(socket_escuta_tcp, (struct sockaddr*)&endereco_servidor_tcp, sizeof(endereco_servidor_tcp)) < 0) {
        perror("[TCP] Erro no bind TCP"); // Se deu erro, avisa.
        close(socket_escuta_tcp); // Fecha o socket.
        exit(EXIT_FAILURE); // Aborta.
    }

    // Coloca o socket para escutar por conexões. O '10' é o tamanho da fila de espera de conexões.
    listen(socket_escuta_tcp, 10);

    printf("[TCP] Servidor TCP pronto para transferências na porta %d...\n", PORTA);

    // Loop principal: fica esperando (accept) por novas conexões.
    while (1) {
        // accept() bloqueia até que um cliente se conecte. Retorna um novo socket para essa conexão.
        socket_nova_conexao = accept(socket_escuta_tcp, (struct sockaddr*)&endereco_cliente_tcp, &tamanho_endereco_cliente);
        if (socket_nova_conexao < 0) { // Se deu erro no accept...
            perror("[TCP] Erro no accept TCP, tentando de novo..."); // Avisa.
            continue; // Volta pro início do loop e tenta de novo.
        }
        
        char* ip_cliente_conectado = inet_ntoa(endereco_cliente_tcp.sin_addr); // Pega o IP do cliente.
        printf("[TCP] Cliente %s conectou para pedir um arquivo.\n", ip_cliente_conectado);

        // Precisamos passar o 'socket_nova_conexao' para a thread.
        // Como a thread só aceita void*, alocamos memória para um int e passamos o ponteiro.
        int* p_socket_conexao_cliente = malloc(sizeof(int));
        if (!p_socket_conexao_cliente) { // Se não conseguiu alocar memória...
            perror("[TCP] Erro crítico no malloc para a thread TCP de envio"); // Avisa.
            close(socket_nova_conexao); // Fecha a conexão que não vai ser tratada.
            continue; // Volta pro loop.
        }
        *p_socket_conexao_cliente = socket_nova_conexao; // Guarda o socket na memória alocada.

        pthread_t tid_envio; // Identificador da nova thread.
        // Cria a thread que vai cuidar do envio do arquivo para este cliente.
        if (pthread_create(&tid_envio, NULL, servidor_tcp_thread_envio, p_socket_conexao_cliente) != 0) {
            perror("[TCP] Erro ao criar thread de envio TCP"); // Se deu erro, avisa.
            free(p_socket_conexao_cliente); // Libera a memória alocada.
            close(socket_nova_conexao); // Fecha a conexão.
        } else {
            pthread_detach(tid_envio); // Desvincula a thread para que ela rode independente.
        }
    }

    close(socket_escuta_tcp); // Fecha o socket de escuta.
    return NULL; // Fim da thread do servidor TCP.
}

// Thread do servidor UDP. Lida com mensagens de controle (descoberta, busca, etc.).
void* servidor_udp(void* arg) {
    (void)arg; // Ignora o argumento.

    int socket_udp; // Socket para comunicação UDP.
    // Cria o socket UDP: AF_INET (IPv4), SOCK_DGRAM (UDP, não confiável, datagramas).
    if ((socket_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[UDP] Erro crítico ao criar socket UDP"); // Avisa.
        exit(EXIT_FAILURE); // Aborta.
    }

    // Opção para permitir reuso do endereço. Útil pra testes.
    int opt_reusar_endereco_udp = 1;
    if (setsockopt(socket_udp, SOL_SOCKET, SO_REUSEADDR, &opt_reusar_endereco_udp, sizeof(opt_reusar_endereco_udp)) < 0) {
        perror("[UDP] Erro em setsockopt(SO_REUSEADDR) UDP"); // Avisa.
        close(socket_udp); // Fecha.
        exit(EXIT_FAILURE); // Aborta.
    }

    // Habilita o envio de mensagens em broadcast.
    // aqui é mais pro cliente.
    int opt_broadcast_udp = 1;
    if (setsockopt(socket_udp, SOL_SOCKET, SO_BROADCAST, &opt_broadcast_udp, sizeof(opt_broadcast_udp)) < 0) {
        perror("[UDP] Erro em setsockopt(SO_BROADCAST) UDP"); // Avisa.
        close(socket_udp); // Fecha.
        exit(EXIT_FAILURE); // Aborta.
    }
    
    struct sockaddr_in endereco_servidor_udp; // Endereço do nosso servidor UDP.
    struct sockaddr_in endereco_remetente_udp; // Endereço de quem enviou a mensagem UDP.
    socklen_t tamanho_endereco_remetente = sizeof(endereco_remetente_udp); // Tamanho da estrutura.
    mensagem_udp_t msg_recebida_udp; // Buffer para a mensagem UDP recebida.

    // Configura o endereço do servidor UDP.
    endereco_servidor_udp.sin_family = AF_INET;         // IPv4.
    endereco_servidor_udp.sin_port = htons(PORTA);      // Porta.
    endereco_servidor_udp.sin_addr.s_addr = INADDR_ANY; // Escuta em todas as interfaces.

    // Associa (bind) o socket UDP ao endereço e porta.
    if (bind(socket_udp, (struct sockaddr*)&endereco_servidor_udp, sizeof(endereco_servidor_udp)) < 0) {
        perror("[UDP] Erro crítico no bind UDP"); // Avisa.
        close(socket_udp); // Fecha.
        exit(EXIT_FAILURE); // Aborta.
    }
    printf("[UDP] Servidor UDP aguardando mensagens na porta %d...\n", PORTA);

    // Loop para receber e processar mensagens UDP.
    while (1) {
        // Espera (bloqueia) até uma mensagem UDP chegar.
        // recvfrom preenche msg_recebida_udp, endereco_remetente_udp e tamanho_endereco_remetente.
        ssize_t bytes_recebidos = recvfrom(socket_udp, &msg_recebida_udp, sizeof(msg_recebida_udp), 0,
                                       (struct sockaddr*)&endereco_remetente_udp, &tamanho_endereco_remetente);
        if (bytes_recebidos < 0) { // Se deu erro...
            perror("[UDP] Erro no recvfrom UDP, tentando de novo..."); // Avisa.
            continue; // Volta pro início do loop.
        }

        char* ip_remetente = inet_ntoa(endereco_remetente_udp.sin_addr); // Converte IP do remetente para string.
        printf("[UDP] Mensagem '%s' (%d bytes) recebida de %s\n",
               tipo_msg_str(msg_recebida_udp.tipo_msg), (int)bytes_recebidos, ip_remetente);

        // --- LOG DE SOLICITAÇÃO UDP ---
        // Nome do arquivo para o log: usa o da mensagem se for PROCURA_ARQUIVO ou RESPOSTA_ARQUIVO.
        // Para outros tipos, pode ser uma string vazia ou NULL. Tempo não se aplica (-1.0).
        const char* arquivo_para_log_udp = (msg_recebida_udp.tipo_msg == PROCURA_ARQUIVO || msg_recebida_udp.tipo_msg == RESPOSTA_ARQUIVO)
                                           ? msg_recebida_udp.nome_arquivo
                                           : "";
        registrar_log(ip_remetente, msg_recebida_udp.tipo_msg, arquivo_para_log_udp, -1.0);

        // --- LÓGICA DE PROCESSAMENTO DAS MENSAGENS UDP ---
        
        // Se a mensagem for um pedido para LISTAR USUÁRIOS...
        if (msg_recebida_udp.tipo_msg == LISTA_USUARIOS) {
            mensagem_udp_t resposta_udp; // Prepara uma mensagem de resposta.
            resposta_udp.tipo_msg = LISTA_USUARIOS; // Responde com o mesmo tipo (indicando presença).
            strncpy(resposta_udp.nome_arquivo, "", TAM_NOME_ARQ -1); // Nenhum nome de arquivo específico.
            resposta_udp.nome_arquivo[TAM_NOME_ARQ -1] = '\0';
            resposta_udp.tamanho_arquivo = 0; // Sem tamanho de arquivo.
            // Envia a resposta de volta para quem perguntou (endereco_remetente_udp).
            sendto(socket_udp, &resposta_udp, sizeof(resposta_udp), 0,
                   (struct sockaddr*)&endereco_remetente_udp, tamanho_endereco_remetente);
            printf("[UDP] Respondi a LISTA_USUARIOS de %s\n", ip_remetente);
        }
        // Se a mensagem for um pedido para LISTAR ARQUIVOS (deste peer)...
        else if (msg_recebida_udp.tipo_msg == LISTA_ARQUIVOS) {
            char lista_meus_arquivos[100][TAM_NOME_ARQ]; // Array para guardar nomes dos meus arquivos.
            int total_meus_arquivos; // Quantos arquivos eu tenho.
            listar_arquivos(lista_meus_arquivos, &total_meus_arquivos); // Pega a lista.

            printf("[UDP] %s pediu minha lista de arquivos. Tenho %d arquivos.\n", ip_remetente, total_meus_arquivos);
            // Para cada arquivo que eu tenho...
            for (int i = 0; i < total_meus_arquivos; i++) {
                mensagem_udp_t resposta_udp; // Prepara uma mensagem de resposta.
                resposta_udp.tipo_msg = LISTA_ARQUIVOS; // Tipo da resposta.
                strncpy(resposta_udp.nome_arquivo, lista_meus_arquivos[i], TAM_NOME_ARQ -1); // Copia nome do arquivo.
                resposta_udp.nome_arquivo[TAM_NOME_ARQ-1] = '\0';
                resposta_udp.tamanho_arquivo = 0; // só o nome.
                                                 // Poderia ser o tamanho real se o protocolo exigisse.
                // Envia uma mensagem para cada arquivo.
                sendto(socket_udp, &resposta_udp, sizeof(resposta_udp), 0,
                       (struct sockaddr*)&endereco_remetente_udp, tamanho_endereco_remetente);
            }
        }
        // Se a mensagem for uma PROCURA POR ARQUIVO...
        else if (msg_recebida_udp.tipo_msg == PROCURA_ARQUIVO) {
            printf("[UDP] %s está procurando por '%s'. Deixa eu ver se tenho...\n", ip_remetente, msg_recebida_udp.nome_arquivo);
            struct stat info_arquivo_procurado; // Estrutura para informações do arquivo.
            // stat verifica se o arquivo existe e pega informações sobre ele.
            if (stat(msg_recebida_udp.nome_arquivo, &info_arquivo_procurado) == 0 && S_ISREG(info_arquivo_procurado.st_mode)) {
                // Se o arquivo existe E é um arquivo regular...
                printf("[UDP] Achei '%s'! Tamanho: %ld bytes. Vou avisar %s.\n",
                       msg_recebida_udp.nome_arquivo, (long)info_arquivo_procurado.st_size, ip_remetente);
                mensagem_udp_t resposta_udp; // Prepara a resposta.
                resposta_udp.tipo_msg = RESPOSTA_ARQUIVO; // Tipo: RESPOSTA_ARQUIVO.
                strncpy(resposta_udp.nome_arquivo, msg_recebida_udp.nome_arquivo, TAM_NOME_ARQ -1); // Nome do arquivo.
                resposta_udp.nome_arquivo[TAM_NOME_ARQ-1] = '\0';
                resposta_udp.tamanho_arquivo = info_arquivo_procurado.st_size; // Tamanho do arquivo.
                // Envia a resposta para quem procurou.
                sendto(socket_udp, &resposta_udp, sizeof(resposta_udp), 0,
                       (struct sockaddr*)&endereco_remetente_udp, tamanho_endereco_remetente);
            } else {
                // Se não encontrou ou não é um arquivo regular.
                printf("[UDP] Não encontrei '%s' ou não é um arquivo válido para compartilhar.\n", msg_recebida_udp.nome_arquivo);
                // O protocolo não exige uma resposta negativa, então não enviamos nada.
            }
        }
        // Outros tipos de mensagem UDP podem ser tratados aqui se necessário.
    }

    close(socket_udp);
    return NULL; // Fim da thread do servidor UDP.
}

// Função principal da parte CLIENTE da aplicação (roda na thread main).
// Mostra o menu e interage com o usuário para fazer solicitações na rede P2P.
void cliente_udp() {
    int socket_cliente_udp; // Socket que o cliente usará para enviar mensagens UDP.
    // Cria o socket UDP.
    if ((socket_cliente_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Cliente: Erro ao criar socket UDP"); // Avisa.
        return; // Sai da função cliente, o programa principal pode decidir o que fazer.
    }

    // Permite reuso do endereço (bom para testes).
    int opt_reusar_addr_cliente = 1;
    setsockopt(socket_cliente_udp, SOL_SOCKET, SO_REUSEADDR, &opt_reusar_addr_cliente, sizeof(opt_reusar_addr_cliente));

    // Habilita capacidade de broadcast para este socket.
    int opt_broadcast_cliente = 1;
    if (setsockopt(socket_cliente_udp, SOL_SOCKET, SO_BROADCAST, &opt_broadcast_cliente, sizeof(opt_broadcast_cliente)) < 0) {
        perror("Cliente: Erro ao habilitar broadcast no socket UDP");
        close(socket_cliente_udp);
        return;
    }


    struct sockaddr_in endereco_broadcast_destino; // Endereço de destino para mensagens broadcast.
    endereco_broadcast_destino.sin_family = AF_INET;                     // IPv4.
    endereco_broadcast_destino.sin_port = htons(PORTA);                  // Porta dos peers. 
    endereco_broadcast_destino.sin_addr.s_addr = htonl(INADDR_BROADCAST); // Endereço especial de broadcast (255.255.255.255).

    // Loop do menu de opções do cliente.
    while (1) {
        // Mostra as opções para o usuário.
        printf("\n--- Cliente P2P ---\n");
        printf("1 - Listar usuários na rede (Broadcast)\n");     
        printf("2 - Listar TODOS os arquivos na rede (Broadcast)\n"); 
        printf("3 - Procurar por um arquivo específico (Broadcast)\n"); 
        printf("4 - Baixar um arquivo \n");
        printf("0 - Sair do programa\n");
        printf("Sua escolha: ");

        int opcao_usuario;
        // Lê a escolha do usuário.
        if (scanf("%d", &opcao_usuario) != 1) { // Se a leitura falhar
            printf("Opção inválida. Por favor, digite um número.\n");
            while (getchar() != '\n'); // Limpa o buffer de entrada pra não dar problema na próxima leitura.
            continue; // Volta pro início do loop do menu.
        }
        getchar(); // Consome o '\n' que ficou no buffer depois do scanf.

        if (opcao_usuario == 0) { // Se escolheu 0...
            printf("Saindo da parte cliente...\n");
            break; // Sai do loop do menu.
        }

        mensagem_udp_t msg_cliente_para_rede; // Mensagem que o cliente vai enviar.
        memset(&msg_cliente_para_rede, 0, sizeof(msg_cliente_para_rede)); // Zera a estrutura da mensagem
        msg_cliente_para_rede.nome_arquivo[0] = '\0'; // Garante que nome_arquivo está vazio inicialmente.
        msg_cliente_para_rede.tamanho_arquivo = 0;    // E tamanho_arquivo também.

        // Configura a mensagem de acordo com a opção.
        if (opcao_usuario == 1) { // Listar usuários.
            msg_cliente_para_rede.tipo_msg = LISTA_USUARIOS; // Define o tipo.
        } else if (opcao_usuario == 2) { // Listar todos os arquivos.
            msg_cliente_para_rede.tipo_msg = LISTA_ARQUIVOS; // Define o tipo.
        } else if (opcao_usuario == 3 || opcao_usuario == 4) { // Procurar ou Baixar arquivo.
            printf("Digite o nome do arquivo: ");
            // Lê o nome do arquivo digitado pelo usuário.
            if (fgets(msg_cliente_para_rede.nome_arquivo, TAM_NOME_ARQ, stdin) == NULL) {
                printf("Erro ao ler nome do arquivo.\n");
                continue;
            }
            // Remove o '\n' do final do nome do arquivo.
            msg_cliente_para_rede.nome_arquivo[strcspn(msg_cliente_para_rede.nome_arquivo, "\n")] = 0;
            
            if (opcao_usuario == 3) { // Se for só procurar...
                msg_cliente_para_rede.tipo_msg = PROCURA_ARQUIVO; // Define tipo PROCURA_ARQUIVO.
            } else { // Se for pra baixar (opção 4), primeiro PROCURA.
                msg_cliente_para_rede.tipo_msg = PROCURA_ARQUIVO; // Manda uma PROCURA primeiro.
                                                              // O download em si acontece após receber RESPOSTA_ARQUIVO.
            }
        } else { // Opção inválida.
            printf("Opção não reconhecida. Tente novamente.\n");
            continue; // Volta pro menu.
        }

        // Envia a mensagem de controle em broadcast para a rede.
        if (sendto(socket_cliente_udp, &msg_cliente_para_rede, sizeof(msg_cliente_para_rede), 0,
                   (struct sockaddr*)&endereco_broadcast_destino, sizeof(endereco_broadcast_destino)) < 0) {
            perror("Cliente: Erro no sendto ao enviar mensagem broadcast"); // Se deu erro, avisa.
            continue; // Tenta de novo no menu.
        }
        
        printf("Ok, mensagem '%s' enviada. Aguardando respostas...\n", tipo_msg_str(msg_cliente_para_rede.tipo_msg));
        
        // Configura um timeout de 2 segundos para esperar por respostas.
        struct timeval timeout_cliente;
        timeout_cliente.tv_sec = 2;  // 2 segundos.
        timeout_cliente.tv_usec = 0; // 0 microssegundos.
        // Aplica o timeout ao socket para operações de recebimento.
        if (setsockopt(socket_cliente_udp, SOL_SOCKET, SO_RCVTIMEO, &timeout_cliente, sizeof(timeout_cliente)) < 0) {
            perror("Cliente: Erro ao configurar timeout de recebimento (SO_RCVTIMEO)");
            // Pode continuar mesmo assim, mas sem timeout.
        }

        struct sockaddr_in endereco_resposta_peer; // Endereço de quem respondeu.
        socklen_t tamanho_endereco_resposta = sizeof(endereco_resposta_peer); // Tamanho da estrutura.
        mensagem_udp_t resposta_recebida_pelo_cliente; // Buffer para a resposta.
        int alguma_resposta_util = 0; // Flag para saber se recebemos algo interessante.

        // Loop para receber múltiplas respostas (de diferentes peers).
        while (1) {
            // Tenta receber uma resposta.
            ssize_t bytes_resposta = recvfrom(socket_cliente_udp, &resposta_recebida_pelo_cliente, sizeof(resposta_recebida_pelo_cliente),
                                              0, (struct sockaddr*)&endereco_resposta_peer, &tamanho_endereco_resposta);
            
            if (bytes_resposta < 0) { // Se deu erro no recvfrom...
                // Verifica se o erro foi por causa do timeout.
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    if (!alguma_resposta_util && msg_cliente_para_rede.tipo_msg != LISTA_USUARIOS) {
                         // Se não teve nenhuma resposta útil (exceto para lista de usuários, onde silêncio pode ser normal)
                        if (msg_cliente_para_rede.tipo_msg == PROCURA_ARQUIVO && strlen(msg_cliente_para_rede.nome_arquivo) > 0){
                            printf("Ninguém parece ter o arquivo '%s'.\n", msg_cliente_para_rede.nome_arquivo);
                        } else if (msg_cliente_para_rede.tipo_msg == LISTA_ARQUIVOS) {
                            printf("Nenhum peer respondeu com lista de arquivos.\n");
                        }
                    } else if (!alguma_resposta_util && msg_cliente_para_rede.tipo_msg == LISTA_USUARIOS){
                        printf("Nenhum outro usuário encontrado na rede.\n");
                    }
                    printf("Fim das respostas (timeout).\n");
                } else { // Se foi outro erro...
                    perror("Cliente: Erro no recvfrom ao esperar resposta");
                }
                break; // Sai do loop de esperar respostas.
            }

            // Se chegou aqui, uma resposta foi recebida.
            alguma_resposta_util = 1; // Marca que tivemos pelo menos uma resposta.
            char* ip_peer_respondeu = inet_ntoa(endereco_resposta_peer.sin_addr); // IP de quem respondeu.

            // Processa a resposta de acordo com o tipo.
            if (resposta_recebida_pelo_cliente.tipo_msg == LISTA_USUARIOS) { // Se for resposta de LISTA_USUARIOS...
                printf("-> Usuário ativo encontrado em: %s\n", ip_peer_respondeu); // Mostra o IP.
            } else if (resposta_recebida_pelo_cliente.tipo_msg == LISTA_ARQUIVOS) { // Se for resposta de LISTA_ARQUIVOS...
                // Formato: <IP> <nome do arquivo> 
                printf("-> %s possui o arquivo: %s\n", ip_peer_respondeu, resposta_recebida_pelo_cliente.nome_arquivo);
            } else if (resposta_recebida_pelo_cliente.tipo_msg == RESPOSTA_ARQUIVO) { // Se for RESPOSTA_ARQUIVO...
                printf("-> O peer %s tem o arquivo '%s' (Tamanho: %u bytes)\n",
                       ip_peer_respondeu, resposta_recebida_pelo_cliente.nome_arquivo, resposta_recebida_pelo_cliente.tamanho_arquivo);

                // Se a opção original era BAIXAR (4) e o nome do arquivo bate com o que procura...
                if (opcao_usuario == 4 && strcmp(msg_cliente_para_rede.nome_arquivo, resposta_recebida_pelo_cliente.nome_arquivo) == 0) {
                    // Verifica se o arquivo já existe localmente.
                    FILE* teste_existencia = fopen(resposta_recebida_pelo_cliente.nome_arquivo, "rb"); // Tenta abrir para leitura binária.
                    if (teste_existencia != NULL) { // Se conseguiu abrir, o arquivo existe!
                        fclose(teste_existencia); // Fecha o arquivo que só abrimos para testar.
                        printf("O arquivo '%s' já existe no seu diretório.\n", resposta_recebida_pelo_cliente.nome_arquivo);
                        printf("Quer baixar de %s e sobrescrever mesmo assim? (s/n): ", ip_peer_respondeu);
                        char confirmacao_sobrescrever[4]; // Espaço para 's', '\n', '\0' ou 'n', '\n', '\0'.
                        fgets(confirmacao_sobrescrever, sizeof(confirmacao_sobrescrever), stdin); // Lê a confirmação.
                        // Se não for 's' ou 'S', cancela o download DESTE peer.
                        if (confirmacao_sobrescrever[0] != 's' && confirmacao_sobrescrever[0] != 'S') {
                            printf("Download do arquivo '%s' de %s cancelado pelo usuário.\n", resposta_recebida_pelo_cliente.nome_arquivo, ip_peer_respondeu);
                            continue; // Pula para a próxima resposta UDP (se houver outro peer com o mesmo arquivo).
                        }
                        printf("Ok, baixando e sobrescrevendo '%s' de %s...\n", resposta_recebida_pelo_cliente.nome_arquivo, ip_peer_respondeu);
                    } else { // Se o arquivo não existe localmente...
                        printf("Iniciando download de '%s' (de %s)...\n", resposta_recebida_pelo_cliente.nome_arquivo, ip_peer_respondeu);
                    }

                    // baixar via TCP! 
                    int socket_tcp_cliente_download; // Socket para a conexão TCP.
                    // Cria o socket TCP.
                    socket_tcp_cliente_download = socket(AF_INET, SOCK_STREAM, 0);
                    if (socket_tcp_cliente_download < 0) { // Se deu erro...
                        perror("Cliente: Erro ao criar socket TCP para download");
                        continue; // Não consegue baixar deste, tenta o próximo.
                    }

                    struct sockaddr_in endereco_servidor_tcp_download; // Endereço do servidor TCP do peer.
                    endereco_servidor_tcp_download.sin_family = AF_INET; // IPv4.
                    endereco_servidor_tcp_download.sin_port = htons(PORTA); // Mesma porta.
                    // Converte o IP do peer (string) para o formato de endereço de socket.
                    if (inet_pton(AF_INET, ip_peer_respondeu, &endereco_servidor_tcp_download.sin_addr) <= 0) {
                        perror("Cliente: Erro ao converter IP do servidor para download");
                        close(socket_tcp_cliente_download);
                        continue;
                    }

                    // Tenta conectar ao servidor TCP do peer.
                    if (connect(socket_tcp_cliente_download, (struct sockaddr*)&endereco_servidor_tcp_download, sizeof(endereco_servidor_tcp_download)) < 0) {
                        perror("Cliente: Erro ao conectar via TCP com o peer");
                        close(socket_tcp_cliente_download);
                        continue;
                    }

                    // Conectou! Envia o nome do arquivo que queremos baixar.
                    if (send(socket_tcp_cliente_download, resposta_recebida_pelo_cliente.nome_arquivo, TAM_NOME_ARQ, 0) < 0) {
                        perror("Cliente: Erro ao enviar nome do arquivo para o peer via TCP");
                        close(socket_tcp_cliente_download);
                        continue;
                    }
                    
                    // Abre (ou cria) um arquivo local para salvar o conteúdo baixado ("wb" = write binary).
                    FILE* arquivo_baixado_local = fopen(resposta_recebida_pelo_cliente.nome_arquivo, "wb");
                    if (!arquivo_baixado_local) { // Se não conseguiu criar/abrir o arquivo local...
                        perror("Cliente: Erro ao criar arquivo local para salvar download");
                        close(socket_tcp_cliente_download);
                        continue;
                    }

                    char buffer_recebimento_tcp[TAM_BUFFER]; // Buffer para receber os dados do arquivo.
                    ssize_t bytes_recebidos_tcp; // Quantos bytes foram recebidos em cada pedaço.
                    printf("Baixando ");
                    // Loop para receber os dados do arquivo.
                    while ((bytes_recebidos_tcp = recv(socket_tcp_cliente_download, buffer_recebimento_tcp, TAM_BUFFER, 0)) > 0) {
                        // Escreve os bytes recebidos no arquivo local.
                        fwrite(buffer_recebimento_tcp, 1, bytes_recebidos_tcp, arquivo_baixado_local);
                        printf("."); // Feedback visual simples.
                        fflush(stdout); // Força a escrita do "." no console.
                    }
                    printf(" Fim!\n");

                    // Verifica se o loop terminou por erro no recv.
                    if (bytes_recebidos_tcp < 0) {
                        perror("Cliente: Erro durante o recebimento do arquivo via TCP");
                    }

                    fclose(arquivo_baixado_local); // Fecha o arquivo local.
                    close(socket_tcp_cliente_download); // Fecha a conexão TCP.
                    printf("Arquivo '%s' baixado com sucesso de %s!\n", resposta_recebida_pelo_cliente.nome_arquivo, ip_peer_respondeu);
                    
                    // Se o download foi bem-sucedido, não precisa mais esperar respostas de outros peers para ESTE arquivo.
                    goto fim_loop_respostas_cliente; // Pula para o final do loop de recebimento de respostas.
                }
            }
        } // Fim do while(1) de recebimento de respostas UDP.
        fim_loop_respostas_cliente:; // Label para o goto.
    } // Fim do while(1) do menu do cliente.

    close(socket_cliente_udp); // Fecha o socket UDP do cliente ao sair do menu.
}

// Função principal do programa.
int main() {
    pthread_t tid_servidor_udp; // ID da thread do servidor UDP.
    pthread_t tid_servidor_tcp; // ID da thread do servidor TCP.

    printf("Iniciando o programa P2P RCA...\n");

    // Cria a thread do servidor UDP.
    if (pthread_create(&tid_servidor_udp, NULL, servidor_udp, NULL) != 0) {
        perror("Erro crítico ao criar a thread do servidor UDP"); // Avisa.
        return EXIT_FAILURE; // Termina o programa com erro.
    }
    printf("Thread do servidor UDP iniciada.\n");

    // Cria a thread do servidor TCP.
    if (pthread_create(&tid_servidor_tcp, NULL, servidor_tcp, NULL) != 0) {
        perror("Erro crítico ao criar a thread do servidor TCP"); // Avisa.
        return EXIT_FAILURE; // Termina com erro.
    }
    printf("Thread do servidor TCP iniciada.\n");

    // Chama a função do cliente UDP
    cliente_udp();

    // Quando cliente_udp() terminar (usuário escolheu sair), tentamos encerrar as threads.
    printf("Encerrando os servidores (UDP e TCP)...\n");
    pthread_cancel(tid_servidor_udp); // Envia um pedido de cancelamento para a thread UDP.
    pthread_cancel(tid_servidor_tcp); // Envia um pedido de cancelamento para a thread TCP.

    // Espera que as threads realmente terminem.
    pthread_join(tid_servidor_udp, NULL);
    printf("Thread do servidor UDP finalizada.\n");
    pthread_join(tid_servidor_tcp, NULL);
    printf("Thread do servidor TCP finalizada.\n");

    printf("Programa P2P RCA finalizado.\n");

    return EXIT_SUCCESS;
}