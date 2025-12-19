import os
import json
import requests
import getpass

# Verifica se a variável de ambiente ESP_PASS está definida
if 'ESP_PASS' not in os.environ:
    # Solicita a senha ao usuário se a variável não estiver definida
    SENHA_ADMIN = getpass.getpass("Digite a senha: ")
    # Você pode então usar a variável senha no lugar de os.environ['ESP_PASS']
else:
    SENHA_ADMIN = os.environ['ESP_PASS']

ARQUIVO_LOCAL = "sensores.json"

def baixar_do_esp():
    if not SENHA_ADMIN:
        print("[FALHA] A variável de ambiente ESP_PASS não foi definida.")
        return

    url = f"http://alarme.local/sensores.json?senha={SENHA_ADMIN}"
    print(f"[INFO] Baixando sensores.json de {url} ...")

    try:
        r = requests.get(url, timeout=5)
        r.raise_for_status()
        with open(ARQUIVO_LOCAL, "w", encoding="utf-8") as f:
            json.dump(r.json(), f, ensure_ascii=False, indent=2)
        print("[OK] Arquivo sensores.json salvo com sucesso.")
    except Exception as e:
        print(f"[ERRO] Falha ao baixar: {e}")

if __name__ == "__main__":
    baixar_do_esp()
