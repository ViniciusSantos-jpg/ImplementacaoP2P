#include <stdio.h>      
#include <stdlib.h>     
#include <stdint.h>     
#include <string.h>     
#include <unistd.h>     
#include <pthread.h>    
#include <errno.h>      
#include <dirent.h>     
#include <sys/types.h>  
#include <sys/stat.h>   
#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <netinet/in.h> 
#include <sys/time.h>   
#include <time.h>      
#include <fcntl.h>   

#define PORTA 5555               
#define TAM_NOME_ARQ 100         
#define TAM_BUFFER 1024         

#define LISTA_USUARIOS 1   
#define LISTA_ARQUIVOS 2       
#define PROCURA_ARQUIVO 3         
#define RESPOSTA_ARQUIVO 4        
#define TRANSFERENCIA_ARQUIVO 5   

// Estrutura do pacote UDP
typedef struct {
    uint8_t tipo_msg;                   
    char nome_arquivo[TAM_NOME_ARQ];    
    uint32_t tamanho_arquivo;         
} __attribute__((packed)) mensagem_udp_t;

// Função para converter o tipo de mensagem em uma string
const char* tipo_msg_str(uint8_t tipo) {
    switch (tipo) {
        case LISTA_USUARIOS: return "LISTA_USUARIOS";
        case LISTA_ARQUIVOS: return "LISTA_ARQUIVOS";
        case PROCURA_ARQUIVO: return "PROCURA_ARQUIVO";
        case RESPOSTA_ARQUIVO: return "RESPOSTA_ARQUIVO";
        case TRANSFERENCIA_ARQUIVO: return "TRANSFERENCIA_ARQUIVO";
        default: return "DESCONHECIDA";
    }
}

void registrar_log(const char* ip, uint8_t tipo_msg, const char* nome_arquivo, double tempo) {
    // Abre o arquivo
    FILE* f = fopen("log.txt", "a");
    if (!f) {
        perror("Erro ao abrir o arquivo de log");
        return;
    }

    // Data e hora 
    time_t agora = time(NULL);
    char* data = ctime(&agora);
    data[strcspn(data, "\n")] = 0; 

    fprintf(f, "[%s] IP: %s | Tipo: %s | Arquivo: %s | Tempo: %.2f seg\n",
        data, ip, tipo_msg_str(tipo_msg), nome_arquivo, tempo);
    fclose(f);
}

// Função para listar os arquivos
void listar_arquivos(char arquivos[][TAM_NOME_ARQ], int* total) {
    // Abre o diretório atual 
    DIR* dir = opendir(".");
    if (!dir) {
        perror("Erro ao abrir o diretório atual");
        return;
    }
    struct dirent* entry;
    *total = 0;

    // Itera sobre todas as entradas do diretório
    while ((entry = readdir(dir)) != NULL && *total < 100) {
        struct stat st;
        if (stat(entry->d_name, &st) == 0 && S_ISREG(st.st_mode)) {
            strncpy(arquivos[*total], entry->d_name, TAM_NOME_ARQ);
            (*total)++;
        }
    }

    closedir(dir);
}

// Thread para conexão TCP de envio de arquivo.
void* servidor_tcp_thread(void* arg) {
    int connfd = *(int*)arg;
    free(arg);

    char nome_arquivo[TAM_NOME_ARQ];
    // Recebe o nome do arquivo
    if (recv(connfd, nome_arquivo, TAM_NOME_ARQ, 0) <= 0) {
        close(connfd);
        return NULL;
    }

    // Abre o arquivo
    FILE* f = fopen(nome_arquivo, "rb");
    if (!f) {
        perror("Erro ao abrir arquivo para transferencia");
        close(connfd);
        return NULL;
    }

    // Obtém o endereço IP do peer
    struct sockaddr_in peer;
    socklen_t len = sizeof(peer);
    getpeername(connfd, (struct sockaddr*)&peer, &len);
    char* ip = inet_ntoa(peer.sin_addr);

    char buffer[TAM_BUFFER];
    size_t lidos;
    clock_t inicio = clock();

    // Lê do arquivo e envia para o cliente em blocos
    while ((lidos = fread(buffer, 1, TAM_BUFFER, f)) > 0) {
        if (send(connfd, buffer, lidos, 0) < 0) {
            perror("Erro ao enviar dados do arquivo");
            break;
        }
    }

    clock_t fim = clock();
    double tempo = (double)(fim - inicio) / CLOCKS_PER_SEC;
    registrar_log(ip, TRANSFERENCIA_ARQUIVO, nome_arquivo, tempo);

    fclose(f);
    close(connfd);
    printf("[TCP] Transferência do arquivo '%s' para %s concluída.\n", nome_arquivo, ip);
    return NULL;
}

// Thread principal do servidor TCP
void* servidor_tcp(void* arg) {
    (void)arg;
    int sockfd, connfd;
    struct sockaddr_in serv, cliente;
    socklen_t len = sizeof(cliente);

    // Cria o socket TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erro ao criar socket TCP");
        exit(EXIT_FAILURE);
    }
    
    // Configura o endereço do servidor
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORTA);
    serv.sin_addr.s_addr = INADDR_ANY; // Aceita conexões de qualquer IP

    // Associa o socket ao endereço e porta (bind)
    if (bind(sockfd, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("Erro no bind TCP");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 10);

    printf("[TCP] Servidor pronto para transferências na porta %d...\n", PORTA);

    while (1) {
        connfd = accept(sockfd, (struct sockaddr*)&cliente, &len);
        if (connfd < 0) {
            perror("Erro no accept TCP");
            continue;
        }
        
        int* pconn = malloc(sizeof(int));
        if (!pconn) {
            perror("Erro no malloc para a thread TCP");
            close(connfd);
            continue;
        }
        *pconn = connfd;

        pthread_t tid;
        pthread_create(&tid, NULL, servidor_tcp_thread, pconn);
        pthread_detach(tid);
    }

    close(sockfd);
    return NULL;
}

// Thread do servidor UDP, que lida com mensagens de controle.
void* servidor_udp(void* arg) {
    (void)arg;
    int sockfd;
    // Cria o socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erro ao criar socket UDP");
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("Erro em setsockopt(SO_REUSEADDR)");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Habilita o envio de mensagens em broadcast
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("Erro em setsockopt(SO_BROADCAST)");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in serv, cli;
    socklen_t len = sizeof(cli);
    mensagem_udp_t msg;

    // Configura o endereço do servidor UDP
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORTA);
    serv.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        perror("Erro no bind UDP");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("[UDP] Servidor aguardando mensagens na porta %d...\n", PORTA);

    // Receber e processar mensagens UDP
    while (1) {
        // Espera por uma mensagem
        ssize_t n = recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&cli, &len);
        if (n < 0) {
            perror("Erro no recvfrom UDP");
            continue;
        }

        char* ip = inet_ntoa(cli.sin_addr);
        printf("[UDP] Mensagem '%s' recebida de %s\n", tipo_msg_str(msg.tipo_msg), ip);

        // --- LÓGICA DE PROCESSAMENTO DAS MENSAGENS ---
        
        if (msg.tipo_msg == LISTA_USUARIOS) {
            mensagem_udp_t resp;
            resp.tipo_msg = LISTA_USUARIOS; // Responde com o mesmo tipo
            strncpy(resp.nome_arquivo, "", TAM_NOME_ARQ);
            resp.tamanho_arquivo = 0;
            sendto(sockfd, &resp, sizeof(resp), 0, (struct sockaddr*)&cli, len);
        }

        if (msg.tipo_msg == LISTA_ARQUIVOS) {
            char lista_arquivos[100][TAM_NOME_ARQ];
            int total_arquivos;
            listar_arquivos(lista_arquivos, &total_arquivos);
            for (int i = 0; i < total_arquivos; i++) {
                mensagem_udp_t resp;
                resp.tipo_msg = LISTA_ARQUIVOS;
                strncpy(resp.nome_arquivo, lista_arquivos[i], TAM_NOME_ARQ);
                resp.tamanho_arquivo = 0;
                sendto(sockfd, &resp, sizeof(resp), 0, (struct sockaddr*)&cli, len);
            }
        }

        if (msg.tipo_msg == PROCURA_ARQUIVO) {
            struct stat st;
            if (stat(msg.nome_arquivo, &st) == 0) {
                mensagem_udp_t resp;
                resp.tipo_msg = RESPOSTA_ARQUIVO;
                strncpy(resp.nome_arquivo, msg.nome_arquivo, TAM_NOME_ARQ);
                resp.tamanho_arquivo = st.st_size;
                sendto(sockfd, &resp, sizeof(resp), 0, (struct sockaddr*)&cli, len);
            }
        }
    }

    close(sockfd);
    return NULL;
}

void cliente_udp() {
    int sockfd;
    // Cria o socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Cliente: Erro ao criar socket UDP");
        return;
    }

    // Permite reuso do endereço
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Habilita broadcast
    int broadcast = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORTA);
    dest.sin_addr.s_addr = htonl(INADDR_BROADCAST); // 255.255.255.255

    // Loop do menu do cliente
    while (1) {
        printf("\n[Cliente] Menu de Opções:\n");
        printf("1 - Listar usuários na rede\n");
        printf("2 - Listar arquivos na rede\n");
        printf("3 - Procurar por um arquivo\n");
        printf("4 - Baixar um arquivo\n");
        printf("0 - Sair\n> ");

        int op;
        if (scanf("%d", &op) != 1) { // Proteção contra entrada inválida
            while (getchar() != '\n'); // Limpa o buffer de entrada
            printf("Opção inválida. Tente novamente.\n");
            continue;
        }
        getchar(); 

        if (op == 0) break; // Sai do loop e encerra o cliente

        mensagem_udp_t msg;
        memset(&msg, 0, sizeof(msg)); // Zera a estrutura da mensagem

        if (op == 1) {
            msg.tipo_msg = LISTA_USUARIOS;
        } else if (op == 2) {
            msg.tipo_msg = LISTA_ARQUIVOS;
        } else if (op == 3 || op == 4) {
            printf("Digite o nome do arquivo: ");
            fgets(msg.nome_arquivo, TAM_NOME_ARQ, stdin);
            msg.nome_arquivo[strcspn(msg.nome_arquivo, "\n")] = 0;
            msg.tipo_msg = PROCURA_ARQUIVO;
        } else {
            printf("Opção inválida.\n");
            continue;
        }

        // Envia a mensagem de controle em broadcast para a rede
        if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
            perror("Cliente: Erro no sendto");
            continue;
        }
        
        printf("Aguardando respostas...\n");
        
        // Timeout de 2s
        struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        struct sockaddr_in resp_addr;
        socklen_t len = sizeof(resp_addr);
        mensagem_udp_t resposta;

        // Loop múltiplas respostas
        while (1) {
            ssize_t r = recvfrom(sockfd, &resposta, sizeof(resposta), 0, (struct sockaddr*)&resp_addr, &len);
            
            if (r < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    printf("Fim das respostas.\n");
                } else {
                    perror("Cliente: Erro no recvfrom");
                }
                break;
            }

            char* ip = inet_ntoa(resp_addr.sin_addr);

            if (resposta.tipo_msg == LISTA_USUARIOS) {
                printf("-> Usuário ativo encontrado em: %s\n", ip);
            } else if (resposta.tipo_msg == LISTA_ARQUIVOS) {
                printf("-> %s possui o arquivo: %s\n", ip, resposta.nome_arquivo);
            } else if (resposta.tipo_msg == RESPOSTA_ARQUIVO) {
                printf("-> [RESPOSTA] O peer %s possui o arquivo '%s' (Tamanho: %u bytes)\n",
                       ip, resposta.nome_arquivo, resposta.tamanho_arquivo);

                if (op == 4) {
                    printf("Iniciando download de %s...\n", ip);
                    int socktcp = socket(AF_INET, SOCK_STREAM, 0);
                    if (socktcp < 0) {
                        perror("Cliente: Erro ao criar socket TCP");
                        continue;
                    }

                    struct sockaddr_in server_addr;
                    server_addr.sin_family = AF_INET;
                    server_addr.sin_port = htons(PORTA);
                    inet_pton(AF_INET, ip, &server_addr.sin_addr);

                    if (connect(socktcp, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
                        perror("Cliente: Erro ao conectar via TCP");
                        close(socktcp);
                        continue;
                    }

                    // Envia o nome do arquivo
                    send(socktcp, resposta.nome_arquivo, TAM_NOME_ARQ, 0);

                    // Abre um arquivo local para salvar
                    FILE* f = fopen(resposta.nome_arquivo, "wb");
                    if (!f) {
                        perror("Cliente: Erro ao criar arquivo local para download");
                        close(socktcp);
                        continue;
                    }

                    char buffer[TAM_BUFFER];
                    ssize_t lido;
                    while ((lido = recv(socktcp, buffer, TAM_BUFFER, 0)) > 0) {
                        fwrite(buffer, 1, lido, f);
                    }

                    fclose(f);
                    close(socktcp);
                    printf("Arquivo '%s' baixado com sucesso!\n", resposta.nome_arquivo);
                    goto end_receive_loop; 
                }
            }
        }
        end_receive_loop:;
    }

    close(sockfd);
}

int main() {
    pthread_t tid_udp, tid_tcp;

    // Cria a thread UDP
    if (pthread_create(&tid_udp, NULL, servidor_udp, NULL) != 0) {
        perror("Erro ao criar a thread do servidor UDP");
        return EXIT_FAILURE;
    }

    // Cria a thread TCP
    if (pthread_create(&tid_tcp, NULL, servidor_tcp, NULL) != 0) {
        perror("Erro ao criar a thread do servidor TCP");
        return EXIT_FAILURE;
    }

    cliente_udp();

    printf("Encerrando os servidores...\n");
    pthread_cancel(tid_udp);
    pthread_cancel(tid_tcp);

    pthread_join(tid_udp, NULL);
    pthread_join(tid_tcp, NULL);

    printf("Programa finalizado.\n");

    return 0;
}