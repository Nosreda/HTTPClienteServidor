#ifndef FUNC_H
#define FUNC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> 
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>

//Porta escutada pelo servidor
#define PORTA 8080
//Tamanho buffer
#define TAMBUFFER 1024
//Tamanho buffer2
#define TAMBUFFER2 4096

char* lerArquivo(const char *caminho, long *tamanho); // Protótipo atualizado

void parseURL(const char *url, char *host, int *porta, char *caminho){
    *porta = 80; 
    if(strncmp(url, "http://", 7) == 0)
        url += 7;

    const char *comecoCaminho = strchr(url, '/');

    if(comecoCaminho){
        strncpy(host, url, comecoCaminho - url);
        host[comecoCaminho - url] = '\0';
        strcpy(caminho, comecoCaminho);
    }else{
        strcpy(host, url);
        strcpy(caminho, "/");
    }

    // Verifica se tem uma porta especificada
    char *colon = strchr(host, ':');
    if(colon){
        *colon = '\0';
        *porta = atoi(colon + 1);
    }
}

FILE* abrirArquivo(const char *caminho, int sock){
    const char *nomeArquivo = strrchr(caminho, '/');

    if(nomeArquivo && strlen(nomeArquivo) > 1){
        nomeArquivo = nomeArquivo + 1;
    }else{
        nomeArquivo = "index.html";
    }

    FILE *fp = fopen(nomeArquivo, "wb");
    if(!fp){
        perror("Erro ao abrir arquivo\n");
        close(sock);
        return NULL;
    }

    printf("Salvando conteudo em '%s'\n", nomeArquivo);
    return fp;
}

void receberRespostaHTTP(int sock, FILE *fp) {
    char buffer[TAMBUFFER2];
    ssize_t recebido;
    char headerTemp[TAMBUFFER2 * 4] = {0}; 
    size_t tamHeader = 0;
    char *comecoCabecalho = NULL;

    //Recebe e Analisa o Cabeçalho
    while(!comecoCabecalho && (recebido = recv(sock, buffer, TAMBUFFER2, 0)) > 0){
        
        //Verifica o limite de headerTemp (corrigido o warning de signed/unsigned)
        if(tamHeader + (size_t)recebido >= sizeof(headerTemp)){
            fprintf(stderr, "Erro: Cabeçalho da resposta HTTP é muito grande!\n");
            fclose(fp);
            close(sock);
            return;
        }

        //Copia a nova parte recebida para o buffer temporário
        memcpy(headerTemp + tamHeader, buffer, recebido);
        tamHeader += recebido;
        headerTemp[tamHeader] = '\0'; 

        //Procura o fim do cabeçalho
        comecoCabecalho = strstr(headerTemp, "\r\n\r\n");
    }

    if(!comecoCabecalho){
        fprintf(stderr, "Erro: Resposta HTTP incompleta ou inválida (sem fim de cabeçalho).\n");
        fclose(fp);
        close(sock);
        return;
    }


    printf("\n--- Cabeçalho da Resposta ---\n");
    *comecoCabecalho = '\0'; 
    printf("%s\n", headerTemp);
    printf("-----------------------------\n\n");
    *comecoCabecalho = '\r'; //Restaura o buffer

    //Verifica o Status Code (ex: HTTP/1.1 200 OK)
    int statusCode = 0;
    if(sscanf(headerTemp, "HTTP/%*f %d", &statusCode) == 1 && statusCode != 200){
        fprintf(stderr, "Aviso: Status HTTP não é 200 OK. Status: %d\n", statusCode);
    }
    
    //Escreve um pedaço do corpo que veio no mesmo pacote do cabeçalho
    char *inicioCorpo = comecoCabecalho + 4; // Pula o \r\n\r\n
    size_t corpoBytes = tamHeader - (inicioCorpo - headerTemp);
    
    if(corpoBytes > 0){
        fwrite(inicioCorpo, 1, corpoBytes, fp);
    }


    //Recebe o restante
    while((recebido = recv(sock, buffer, TAMBUFFER2, 0)) > 0){
        fwrite(buffer, 1, recebido, fp);
    }
    
    if(recebido < 0){
        perror("Aviso: Erro ao receber dados do corpo");
    }

    printf("Arquivo salvo com sucesso!\n");
    fclose(fp);
    close(sock);
}


// Função lerArquivo atualizada para modo binário ("rb") e retornar o tamanho
char* lerArquivo(const char *caminho, long *tamanho){
    FILE *arquivo = fopen(caminho, "rb");
    if(!arquivo) {
        *tamanho = 0;
        return NULL;
    }

    fseek(arquivo, 0, SEEK_END);
    *tamanho = ftell(arquivo);
    rewind(arquivo);

    char *conteudo = malloc(*tamanho + 1);
    if(!conteudo) {
        fclose(arquivo);
        *tamanho = 0;
        return NULL;
    }
    
    fread(conteudo, 1, *tamanho, arquivo);
    conteudo[*tamanho] = '\0';
    fclose(arquivo);
    return conteudo;
}

char* listarDiretorio(const char *diretorio, const char *caminhoRelativo){
    DIR *dir = opendir(diretorio);
    if(!dir) return NULL;

    struct dirent *entrada;
    char *html = malloc(8192);
    if(!html){
        closedir(dir);
        return NULL;
    }

    strcpy(html,
        "<!DOCTYPE html>"
        "<html><head><meta charset='UTF-8'>"
        "<title>Listagem de Arquivos</title>"
        "<style>"
        "body { font-family: Arial, sans-serif; background-color: #f2f2f2; padding: 20px; }"
        "h1 { color: #333; }"
        "table { border-collapse: collapse; width: 100%; background-color: #fff; }"
        "th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }"
        "tr:hover { background-color: #f5f5f5; }"
        "a { text-decoration: none; color: #0066cc; }"
        "a:hover { text-decoration: underline; }"
        "</style>"
        "</head><body>"
        "<h1>Listagem de arquivos</h1>"
        "<ul>" 
    );

    while((entrada = readdir(dir)) != NULL){
        if(strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0) continue;

        strcat(html, "<li><a href=\"/");
        if(strlen(caminhoRelativo) > 0){
            strcat(html, caminhoRelativo);
            strcat(html, "/");
        }
        strcat(html, entrada->d_name);
        strcat(html, "\">");
        strcat(html, entrada->d_name);
        strcat(html, "</a></li>");
    }

    strcat(html, "</ul></body></html>");
    closedir(dir);
    return html;
}


int diretorio(const char *caminho){
    struct stat st;
    if(stat(caminho, &st) == 0 && S_ISDIR(st.st_mode)) return 1;
    return 0;
}

void conexao(int servidor, 
            struct sockaddr_in enderecoCliente, 
            char *diretorioBase,
            int tamEnderecoCliente){

    char buffer[TAMBUFFER];

    while(1){
        int novoCliente;

        if((novoCliente = accept(servidor, (struct sockaddr *) &enderecoCliente, (socklen_t *) &tamEnderecoCliente)) < 0){
            perror("Erro ao aceitar conexao.\n");
            continue;
        }

        int leituraSocket = read(novoCliente, buffer, TAMBUFFER-1);
        if(leituraSocket <= 0){
            close(novoCliente);
            continue;
        }

        buffer[leituraSocket] = '\0';

        char metodo[8], caminho[512];
        sscanf(buffer, "%s %s", metodo, caminho);

        char caminhoLimpo[512];
        if(strcmp(caminho, "/") == 0){
            strcpy(caminhoLimpo, "");
        }else if (caminho[0] == '/'){
            strcpy(caminhoLimpo, caminho + 1);
        }else strcpy(caminhoLimpo, caminho);

        char caminhoCompleto[1024];
        snprintf(caminhoCompleto, sizeof(caminhoCompleto), "%s/%s", diretorioBase, caminhoLimpo);

        char *respostaConteudo = NULL;
        long tamanhoConteudo = 0;

        if(diretorio(caminhoCompleto)){
            char indexPath[1060];
            snprintf(indexPath, sizeof(indexPath), "%s/index.html", caminhoCompleto);
            
            //Leitura de index.html (passa o ponteiro de tamanho)
            respostaConteudo = lerArquivo(indexPath, &tamanhoConteudo); 

            if(!respostaConteudo) {
                respostaConteudo = listarDiretorio(caminhoCompleto, caminhoLimpo); 
                // Se for listagem, tamanhoConteudo é o strlen do HTML gerado
                if (respostaConteudo) tamanhoConteudo = (long)strlen(respostaConteudo);
            }
        }else{
            //Leitura de arquivo (incluindo binários como PNG, passa o ponteiro de tamanho)
            respostaConteudo = lerArquivo(caminhoCompleto, &tamanhoConteudo);
        }


        char *html = NULL;
        
        if(!respostaConteudo){
            html = malloc(8192);
            strcpy(html,
                "<!DOCTYPE html>"
                "<html><head><meta charset='UTF-8'>"
                "<title>404 Not Found</title>"
                "<style>"
                "body { font-family: Arial, sans-serif; background-color: #f2f2f2; text-align: center; padding: 50px; }"
                "h1 { color: #cc0000; font-size: 48px; }"
                "img { margin-top: 20px; width: 300px; max-width: 80%; }"
                "</style>"
                "</head>"
                "<body>"
                "<h1>404 Not Found</h1>"
                "</body>"
                "</html>"
            );

            //Resposta 404 (usa strlen pois é texto HTML)
            char cabecalho[512];
            sprintf(cabecalho,
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: %ld\r\n"
                    "\r\n",
                    (long)strlen(html));

            write(novoCliente, cabecalho, strlen(cabecalho));
            write(novoCliente, html, strlen(html));
            free(html);
        }else{
            const char *contentType = "text/html";

            if(strstr(caminhoCompleto, ".txt")) contentType = "text/plain";
            else if(strstr(caminhoCompleto, ".jpg")) contentType = "image/jpeg";
            else if(strstr(caminhoCompleto, ".png")) contentType = "image/png";
            else if(strstr(caminhoCompleto, ".css")) contentType = "text/css";
            else if(strstr(caminhoCompleto, ".js")) contentType = "application/javascript";
            
            //Impressão do cabeçalho "ok"
            char cabecalho[256];
            sprintf(cabecalho,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: %s\r\n"
                    "Content-Length: %ld\r\n"
                    "\r\n",
                    contentType,
                    tamanhoConteudo); 

            write(novoCliente, cabecalho, strlen(cabecalho));

            write(novoCliente, respostaConteudo, tamanhoConteudo); 
            free(respostaConteudo);
        }

        close(novoCliente);
    }
}

#endif