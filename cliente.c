#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "func.h"

int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Uso: %s http://[host][:portaa]/[caminho]\n", argv[0]);
        return 1;
    }

    char host[256], caminho[512];
    int porta;
    parseURL(argv[1], host, &porta, caminho);

    printf("Host: %s\nPorta: %d\nCaminho: %s\n", host, porta, caminho);

    //Obtém o Ip correspondente ao nome do host
    struct hostent *servidor = gethostbyname(host);
    if(!servidor){
        printf("Erro: host '%s' não encontrado.\n", host);
        return 2;
    }

    //Cria o socket
    int sock = socket(AF_INET, SOCK_STREAM, 0); //SOCK_STREAM, usa TCP
    if(sock < 0){ 
        perror("Ocorreu um erro na criacao do socket\n");
        exit(EXIT_FAILURE);
    }

    //Configura o endereço do servidor
    struct sockaddr_in enderecoServidor;
    enderecoServidor.sin_family = AF_INET;
    enderecoServidor.sin_port = htons(porta);

    memcpy(&enderecoServidor.sin_addr, servidor->h_addr, servidor->h_length); //Copia do Ip obtido no gethostbyname()

    memset(&(enderecoServidor.sin_zero), 0, 8);

    //Conecta ao servidor
    if(connect(sock, (struct sockaddr *)&enderecoServidor, sizeof(enderecoServidor)) < 0){
        perror("Erro na conexao\n");
        close(sock);
        return 4;
    }

    //Monta requisição GET
    char requisicao[1024];
    snprintf(requisicao, sizeof(requisicao),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "User-Agent: meu_navegador/1.0\r\n"
             "Connection: close\r\n\r\n",
             caminho, host);

    //Envia requisição
    if(send(sock, requisicao, strlen(requisicao), 0) < 0){
        perror("Erro ao enviar requisição");
        close(sock);
        return 5;
    }

    FILE *fp = abrirArquivo(caminho, sock);
    if(!fp) return 6;

    receberRespostaHTTP(sock, fp);
    printf("Arquivo salvo com sucesso!\n");

    return 0;
}
