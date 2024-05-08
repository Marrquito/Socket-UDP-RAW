#include <iostream>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <string.h>

using namespace std;

#define SERVER_IP       "15.228.191.109"
#define SERVER_PORT     50000
#define SOURCE_PORT     59155
#define BUFFER_SIZE     1024
#define REQUEST_SIZE    3
#define UDP_HEADER_LEN  11

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

uint16_t inverterEndianness(uint16_t value) {
    return (value << 8) | (value >> 8);
}

uint16_t calcularChecksum(const char* source_ip, const char* dest_ip, struct udphdr* udp_header, uint8_t* payload, size_t payload_length)
{
    uint32_t sum = 0;

    // pseudo header IP
    struct pseudo_header_ip {
        uint32_t source_addr;
        uint32_t dest_addr;
        uint8_t zero;
        uint8_t protocol;
        uint16_t udp_length;
    };

    pseudo_header_ip pseudo_hdr = {0};

    // convertendo o ip em formato de string para uint32_t
    inet_pton(AF_INET, source_ip, &pseudo_hdr.source_addr);
    inet_pton(AF_INET, dest_ip, &pseudo_hdr.dest_addr);

    pseudo_hdr.zero         = 0;
    pseudo_hdr.protocol     = IPPROTO_UDP;
    pseudo_hdr.udp_length   = htons(sizeof(struct udphdr) + payload_length);

    // casting para tratar o dado como inteiro de 2 bytes e não no formato struct udphdr
    const uint16_t* pseudo_ptr = reinterpret_cast<const uint16_t*>(&pseudo_hdr);
    for (size_t i = 0; i < sizeof(pseudo_hdr) / sizeof(uint16_t); ++i) sum += ntohs(pseudo_ptr[i]);
    
    // somando os dados na variavel final
    sum += ntohs(udp_header->source);
    sum += ntohs(udp_header->dest);
    sum += ntohs(udp_header->len);

    // casting para trator o dado do payload uint8_t como uint16_t
    // como vamos pegar de 2 em 2 bytes, limitamos o for até (payload_length/2) para não haver estouro de memória
    for (size_t i = 0; i < (payload_length / 2); ++i)                    sum += ntohs(*reinterpret_cast<const uint16_t*>(&payload[i * 2]));
    

    // se o tamanho do payload for ímpar, soma o último byte
    if (payload_length % 2 != 0) {
        sum += static_cast<uint16_t>(payload[payload_length - 1]) << 8;
    }

    // complemento de um para obter o checksum
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // retorna o dado com endianness invertido e com os bits invertidos pelo operador ~
    return inverterEndianness(static_cast<uint16_t>(~sum));
}

void clientRAW()
{
    int socket_raw  = 0;
    int choice      = 0;
    int identifier  = 0;

    char buffer[BUFFER_SIZE]                = {0}; // buffer para receber a resposta
    uint8_t req[REQUEST_SIZE]               = {0}; // request a ser feita para o servidor
    uint8_t udpHeaderBytes[UDP_HEADER_LEN]  = {0}; // header udp em array de bytes

    struct sockaddr_in local    = {0};
    struct sockaddr_in server   = {0};
    struct udphdr udp_header    = {0};
    socklen_t len               = sizeof(local);

    // criando socket raw
    if((socket_raw = socket(AF_INET, SOCK_RAW, IPPROTO_UDP)) == -1)
    {
        cerr << "Falha ao criar socket: " << strerror(errno) << endl;
        return;
    }

    // configuração das informações do servidor
    server.sin_family       = AF_INET;
    server.sin_addr.s_addr  = inet_addr(SERVER_IP);
    server.sin_port         = htons(SERVER_PORT);

    connect(socket_raw, (struct sockaddr *)&server, sizeof(server));
    getsockname(socket_raw, (struct sockaddr *)&local, &len); // populando a struct "local" para pegar usar o ip da maquina posteriormente

    while(true)
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
                close(socket_raw);
                return;
            default:
                cerr << "Opção inválida\n";
                continue;
        }

        identifier = rand() % 65535 + 1;

        req[1] = (identifier >> 8) & 0xFF; // passando um uint16_t para uint8_t[]
        req[2] = identifier & 0xFF;

        // cabeçalho UDP
        udp_header.source   = htons(SOURCE_PORT);
        udp_header.dest     = htons(SERVER_PORT);
        udp_header.len      = htons(sizeof(struct udphdr) + 3);
        udp_header.check    = 0;                                // checksum (calculado posteriormente)
        udp_header.check    = calcularChecksum(inet_ntop(AF_INET, &local.sin_addr, buffer, sizeof(buffer)), SERVER_IP, &udp_header, req, sizeof(req));

        memset(buffer, 0, BUFFER_SIZE); // zerando os bytes do buffer
        memcpy(udpHeaderBytes,(uint8_t *) &udp_header, sizeof(udp_header)); // copiando a struct udp_header para um array de uint8_t
        memcpy(udpHeaderBytes + sizeof(udp_header), req, sizeof(req)); // copiando a requisição a ser feita para o servidor para um array final de uint8_t

        if(0 > sendto(socket_raw, udpHeaderBytes, sizeof(udpHeaderBytes), 0, (struct sockaddr *)&server, sizeof(server))) // enviando request para o servidor
        {
            cerr << "Erro ao enviar mensagem: " << strerror(errno) << endl;
            continue;
        }

        if(0 > recvfrom(socket_raw, buffer, BUFFER_SIZE, 0, NULL, NULL)) // esperando resposta do servidor
        {
            cerr << "Erro ao receber resposta: " << strerror(errno) << endl;
            continue;
        }

        // operação ternaria para tratar o dado numerico (contador de requisições (inteiro de 4 bytes sem sinal -> uint32_t)) e dado de texto
        (req[0] != RESPONSE_COUNTER) ? 
        cout << "Resposta do servidor: " << &buffer[32] << endl :
        cout << "Resposta do servidor: " << bytesToUInt32(&buffer[32]) << endl;

        memset(buffer, 0, BUFFER_SIZE); // limpa o buffer utilizado
        puts(""); // linha vazia para melhorar o espaçamento no terminal
    }

    return;
}

int main()
{
    thread clienteThread(clientRAW); // criando um thread para melhor manipulação do socket RAW

    clienteThread.join();

    return 0;
}