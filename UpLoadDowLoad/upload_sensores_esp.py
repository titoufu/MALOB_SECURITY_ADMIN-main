import os
import requests
import getpass

ESP_HOST = "http://alarme.local"  # ou substitua pelo IP direto, ex: http://192.168.1.100
ARQUIVO = "sensores.json"
# Verifica se a variável de ambiente ESP_PASS está definida
if 'ESP_PASS' not in os.environ:
    # Solicita a senha ao usuário se a variável não estiver definida
    SENHA_ADMIN = getpass.getpass("Digite a senha: ")
    # Você pode então usar a variável senha no lugar de os.environ['ESP_PASS']
else:
    SENHA_ADMIN = os.environ['ESP_PASS']


def enviar_sensores():
    if not os.path.exists(ARQUIVO):
        print(f"[ERRO] Arquivo '{ARQUIVO}' não encontrado.")
        return

    with open(ARQUIVO, "r", encoding="utf-8") as f:
        conteudo = f.read()

    url = f"{ESP_HOST}/sensores.json?senha={SENHA_ADMIN}"

    try:
        res = requests.post(url, data=conteudo, headers={"Content-Type": "application/json"})
        if res.ok:
            print("[OK] Upload enviado com sucesso.")
        else:
            print(f"[ERRO] Falha no upload: {res.status_code} - {res.text}")
    except Exception as e:
        print(f"[FALHA] Erro na comunicação com o ESP: {e}")

if __name__ == "__main__":
    enviar_sensores()
