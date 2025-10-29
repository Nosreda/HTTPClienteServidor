#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>  //Para o isdigit
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include "func.h"

int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Uso: %s <diretorio>\nExemplo: %s /home/aderson\n", argv[0], argv[0]);
        return 1;
    }

    char *diretorioBase = argv[1];
    int servidor;
    struct sockaddr_in enderecoServidor, enderecoCliente;
    int tamEnderecoServidor = sizeof(enderecoServidor);
    int tamEnderecoCliente = sizeof(enderecoCliente);

    //Cria o Socket
    if((servidor = socket(AF_INET, SOCK_STREAM, 0)) == 0){ //SOCK_STREAM, usa TCP
        perror("Ocorreu um erro na criacao do socket\n");
        exit(EXIT_FAILURE);
    }

    //Permite reusar a porta depois de um tempo após encerrar o processo
    int opt = 1;
    setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //Configurando o endereço do servidor // struct do socket
    enderecoServidor.sin_family = AF_INET; //Usar IPV4 
    enderecoServidor.sin_addr.s_addr = INADDR_ANY; //Configura o endereço IP.
    enderecoServidor.sin_port = htons(PORTA); //Define porta

    //Associa o socket a uma porta
    if(bind(servidor,(struct sockaddr *) &enderecoServidor, tamEnderecoServidor) < 0){
        perror("Erro ao associar o socket.\n");
        close(servidor);
        exit(EXIT_FAILURE);
    }

    //Aceita o número máximo de conexões pendentes - 10 aqui no caso
    if(listen(servidor, 10) < 0){
        perror("Erro ao colocar o socket em modo de escuta\n");
        close(servidor);
        exit(EXIT_FAILURE);
    }

    printf("Servidor HTTP iniciado na porta %d ...\n", PORTA);

    //Função que possui o loop para aceitar as conexões
    conexao(servidor, enderecoCliente, diretorioBase, tamEnderecoCliente);

    //Fechar o socket do servidor
    close(servidor);
    return 0;
}
