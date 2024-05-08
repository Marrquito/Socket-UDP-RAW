#include <iostream>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

using namespace std;

#define SERVER_IP   "15.228.191.109"
#define SERVER_PORT 50000
#define BUFFER_SIZE 1024

// enum com os tipos de requisição aceitas pelo servidor
enum REQUEST_TYPE_ENUM {
    DATE = 0x00,
    MOTIVATIONAL_PHRASE,
    RESPONSE_COUNTER,
};

// conversão de array de bytes para numero inteiro sem sinal de 4 bytes
uint32_t bytesToUInt32(const char* bytes) {
    return ((static_cast<uint32_t>(bytes[0]) & 0xFF) << 24) |
           ((static_cast<uint32_t>(bytes[1]) & 0xFF) << 16) |
           ((static_cast<uint32_t>(bytes[2]) & 0xFF) << 8)  |
           ((static_cast<uint32_t>(bytes[3]) & 0xFF));
}

void clienteUDP()
{
    int socket_desc = 0;
    int choice      = 0;
    int identifier  = 0;
    
    struct sockaddr_in server   = {0};
    char buffer[BUFFER_SIZE]    = {0};

    uint8_t req[3] = {0};

    // criando um socket UDP
    if((socket_desc = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        cerr << "Erro ao criar o socket" << endl;
        return;
    }

    // configuração das informações do servidor
    server.sin_family       = AF_INET;
    server.sin_addr.s_addr  = inet_addr(SERVER_IP);
    server.sin_port         = htons(SERVER_PORT);

    while (true)
    {
        cout << "Escolha uma opção:\n";
        cout << "1. Data e hora atual\n";
        cout << "2. Mensagem motivacional\n";
        cout << "3. Quantidade de respostas emitidas pelo servidor\n";
        cout << "4. Sair\n";
        cout << "Opção: ";
        
        cin >> choice;
        cin.ignore(); // limpa o buffer de entrada

        switch (choice)
        {
            case 1:
                req[0] = DATE;
                break;
            case 2:
                req[0] = MOTIVATIONAL_PHRASE;
                break;
            case 3:
                req[0] = RESPONSE_COUNTER;
                break;
            case 4:
                cout << "Saindo...\n";
                close(socket_desc);
                return;
            default:
                cerr << "Opção inválida\n";
                continue;
        }

        identifier = rand() % 65535 + 1;

        req[1] = (identifier >> 8) & 0xFF; // passando um uint16_t para uint8_t[]
        req[2] = identifier & 0xFF;

        if(sendto(socket_desc, req, sizeof(req), 0, (struct sockaddr *)&server, sizeof(server)) == -1) // enviando request para o servidor
        {
            cerr << "Erro ao enviar mensagem: " << strerror(errno) << endl;
            continue;
        }

        if (0 > recvfrom(socket_desc, buffer, BUFFER_SIZE, 0, NULL, NULL)) // resposta do servidor
        {
            cerr << "Erro ao receber resposta: " << strerror(errno) << endl;
            continue;
        }

        // tratamento necessário para diferenciar o dado em formato string e em formato inteiro de 4 bytes
        (req[0] != RESPONSE_COUNTER) ? 
        cout << "Resposta do servidor: " << &buffer[4] << endl :
        cout << "Resposta do servidor: " << bytesToUInt32(&buffer[4]) << endl;

        puts(""); // linha vazia para melhorar o espaçamento no terminal
    }

    close(socket_desc);
}

int main() {
    thread clienteThread(clienteUDP); // criando um thread para melhor manipulação do socket UDP

    clienteThread.join();

    return 0;
}