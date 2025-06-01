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

// Tipos de mensagens
#define LISTA_USUARIOS 1
#define LISTA_ARQUIVOS 2
#define PROCURA_ARQUIVO 3
#define RESPOSTA_ARQUIVO 4
#define TRANSFERENCIA_ARQUIVO 5



const char* tipo_msg_str(uint8_t tipo) {
    switch (tipo) {
        case 1: return "LISTA_USUARIOS";
        case 2: return "LISTA_ARQUIVOS";
        case 3: return "PROCURA_ARQUIVO";
        case 4: return "RESPOSTA_ARQUIVO";
        case 5: return "TRANSFERENCIA_ARQUIVO";
        default: return "DESCONHECIDA";
    }
}

typedef struct {
    uint8_t tipo_msg;
    char nome_arquivo[TAM_NOME_ARQ];
    uint32_t tamanho_arquivo;
} __attribute__((packed)) mensagem_udp_t;

// Util: registrar log
void registrar_log(const char* ip, uint8_t tipo_msg, const char* nome_arquivo, double tempo) {
    FILE* f = fopen("log.txt", "a");
    if (!f) return;

    time_t agora = time(NULL);
    char* data = ctime(&agora);
    data[strcspn(data, "\n")] = 0;

    fprintf(f, "[%s] IP: %s | Tipo: %s | Arquivo: %s | Tempo: %.2f seg\n",
        data, ip, tipo_msg_str(tipo_msg), nome_arquivo, tempo);
    fclose(f);
}

// Listar arquivos locais
void listar_arquivos(char arquivos[][TAM_NOME_ARQ], int* total) {
    DIR* dir = opendir(".");
    struct dirent* entry;
    *total = 0;
    if (!dir) return;

    while ((entry = readdir(dir)) != NULL && *total < 100) {
        struct stat st;
        if (stat(entry->d_name, &st) == 0 && S_ISREG(st.st_mode)) {
            strncpy(arquivos[*total], entry->d_name, TAM_NOME_ARQ);
            (*total)++;
        }
    }

    closedir(dir);
}

// Envio de arquivo TCP
void* servidor_tcp_thread(void* arg) {
    int connfd = *(int*)arg;
    free(arg);
    char nome_arquivo[TAM_NOME_ARQ];
    recv(connfd, nome_arquivo, TAM_NOME_ARQ, 0);

    FILE* f = fopen(nome_arquivo, "rb");
    if (!f) {
        close(connfd);
        return NULL;
    }

    struct sockaddr_in peer;
    socklen_t len = sizeof(peer);
    getpeername(connfd, (struct sockaddr*)&peer, &len);
    char* ip = inet_ntoa(peer.sin_addr);

    char buffer[TAM_BUFFER];
    size_t lidos;
    clock_t inicio = clock();

    while ((lidos = fread(buffer, 1, TAM_BUFFER, f)) > 0) {
        send(connfd, buffer, lidos, 0);
    }

    clock_t fim = clock();
    double tempo = (double)(fim - inicio) / CLOCKS_PER_SEC;
    registrar_log(ip, TRANSFERENCIA_ARQUIVO, nome_arquivo, tempo);

    fclose(f);
    close(connfd);
    return NULL;
}

// Aceita conexões TCP
void* servidor_tcp(void* arg) {
    int sockfd, connfd;
    struct sockaddr_in serv, cliente;
    socklen_t len = sizeof(cliente);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORTA);
    serv.sin_addr.s_addr = INADDR_ANY;

    bind(sockfd, (struct sockaddr*)&serv, sizeof(serv));
    listen(sockfd, 10);

    printf("[TCP] Servidor pronto para transferências...\n");

    while (1) {
        connfd = accept(sockfd, (struct sockaddr*)&cliente, &len);
        int* pconn = malloc(sizeof(int));
        *pconn = connfd;
        pthread_t tid;
        pthread_create(&tid, NULL, servidor_tcp_thread, pconn);
        pthread_detach(tid);
    }

    close(sockfd);
    return NULL;
}

// Servidor UDP
void* servidor_udp(void* arg) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int broadcast = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in serv, cli;
    socklen_t len = sizeof(cli);
    mensagem_udp_t msg;

    serv.sin_family = AF_INET;
    serv.sin_port = htons(PORTA);
    serv.sin_addr.s_addr = INADDR_ANY;

    bind(sockfd, (struct sockaddr*)&serv, sizeof(serv));
    printf("[UDP] Servidor aguardando mensagens...\n");

    while (1) {
        recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&cli, &len);
        char* ip = inet_ntoa(cli.sin_addr);

        if (msg.tipo_msg == LISTA_ARQUIVOS) {
            char lista[100][TAM_NOME_ARQ];
            int total;
            listar_arquivos(lista, &total);
            for (int i = 0; i < total; i++) {
                mensagem_udp_t resp;
                resp.tipo_msg = LISTA_ARQUIVOS;
                strncpy(resp.nome_arquivo, lista[i], TAM_NOME_ARQ);
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

// Cliente UDP
void cliente_udp() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    int broadcast = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORTA);
    dest.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    while (1) {
        printf("\n[Cliente] Menu:\n");
        printf("1 - Listar usuários\n");
        printf("2 - Listar arquivos\n");
        printf("3 - Procurar arquivo\n");
        printf("4 - Baixar arquivo\n");
        printf("0 - Sair\n> ");

        int op;
        scanf("%d", &op);
        getchar();
        if (op == 0) break;

        mensagem_udp_t msg;
        memset(&msg, 0, sizeof(msg));

        if (op == 1) {
            msg.tipo_msg = LISTA_USUARIOS;
        } else if (op == 2) {
            msg.tipo_msg = LISTA_ARQUIVOS;
        } else if (op == 3 || op == 4) {
            printf("Nome do arquivo: ");
            fgets(msg.nome_arquivo, TAM_NOME_ARQ, stdin);
            msg.nome_arquivo[strcspn(msg.nome_arquivo, "\n")] = 0;
            msg.tipo_msg = PROCURA_ARQUIVO;
        } else {
            continue;
        }

        sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&dest, sizeof(dest));

        struct timeval tv = {.tv_sec = 2, .tv_usec = 0};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        struct sockaddr_in resp_addr;
        socklen_t len = sizeof(resp_addr);
        mensagem_udp_t resposta;

        while (1) {
            ssize_t r = recvfrom(sockfd, &resposta, sizeof(resposta), 0,
                                 (struct sockaddr*)&resp_addr, &len);
            if (r < 0) break;

            char* ip = inet_ntoa(resp_addr.sin_addr);

            if (resposta.tipo_msg == LISTA_ARQUIVOS) {
                printf("%s %s\n", ip, resposta.nome_arquivo);
            } else if (resposta.tipo_msg == RESPOSTA_ARQUIVO) {
                printf("[RESPOSTA] %s tem %s (%u bytes)\n",
                       ip, resposta.nome_arquivo, resposta.tamanho_arquivo);

                if (op == 4) {
                    int socktcp = socket(AF_INET, SOCK_STREAM, 0);
                    struct sockaddr_in server;
                    server.sin_family = AF_INET;
                    server.sin_port = htons(PORTA);
                    inet_pton(AF_INET, ip, &server.sin_addr);

                    if (connect(socktcp, (struct sockaddr*)&server, sizeof(server)) < 0) {
                        perror("Erro ao conectar via TCP");
                        continue;
                    }

                    send(socktcp, resposta.nome_arquivo, TAM_NOME_ARQ, 0);
                    FILE* f = fopen(resposta.nome_arquivo, "wb");
                    char buffer[TAM_BUFFER];
                    ssize_t lido;
                    clock_t inicio = clock();

                    while ((lido = recv(socktcp, buffer, TAM_BUFFER, 0)) > 0) {
                        fwrite(buffer, 1, lido, f);
                    }

                    clock_t fim = clock();
                    double tempo = (double)(fim - inicio) / CLOCKS_PER_SEC;
                    registrar_log(ip, TRANSFERENCIA_ARQUIVO, resposta.nome_arquivo, tempo);

                    fclose(f);
                    close(socktcp);
                    printf("Arquivo %s baixado com sucesso.\n", resposta.nome_arquivo);
                }
            }
        }
    }

    close(sockfd);
}

int main() {
    pthread_t tid_udp, tid_tcp;

    pthread_create(&tid_udp, NULL, servidor_udp, NULL);
    pthread_create(&tid_tcp, NULL, servidor_tcp, NULL);

    cliente_udp();

    pthread_cancel(tid_udp);
    pthread_cancel(tid_tcp);
    pthread_join(tid_udp, NULL);
    pthread_join(tid_tcp, NULL);

    return 0;
}
