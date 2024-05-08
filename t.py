import socket
import datetime
import struct
import random

mensagens_motivacionais = [
    "Você é capaz de grandes coisas!",
    "O esforço de hoje é a recompensa de amanhã.",
    "Continue perseverando!"
]

contador_respostas = 0

def run_server():
    global contador_respostas
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
   # Criação do socket do servidor e bind com o IP e porta definidos
    server_socket.bind(('127.0.0.1', 50000))

    try:
        while True:
            data, addr = server_socket.recvfrom(1024)
            contador_respostas += 1
            print(f"Mensagem recebida de {addr}")

            req_res_tipo, identificador = struct.unpack('!BH', data[:3])
            tipo = req_res_tipo & 0x0F

            resposta = b"Tipo de requisicao desconhecido"  # Valor padrão para resposta

            if tipo == 0b0000:
                resposta = datetime.datetime.now().strftime("%a %b %d %H:%M:%S %Y\n").encode()

            elif tipo == 0b0001:
                mensagem_selecionada = random.choice(mensagens_motivacionais)
                resposta = mensagem_selecionada.encode()

            elif tipo == 0b0010:
                resposta = struct.pack('!I', contador_respostas)

            elif tipo == 0b0011:
                resposta = b"Requisicao invalida."

            tamanho_resposta = len(resposta)
            header_resposta = struct.pack('!BHB', (1 << 4) | tipo, identificador, tamanho_resposta)
            mensagem_completa = header_resposta + resposta

            server_socket.sendto(mensagem_completa, addr)
    except KeyboardInterrupt:
        print("Servidor encerrado.")
    finally:
        server_socket.close()

if __name__ == "__main__":
    run_server()