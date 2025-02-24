## 🚀 Assistente Pessoal  

**Descrição:**  
Este projeto implementa um assistente de voz embarcado utilizando a placa **BitDogLab** baseada no **Raspberry Pi Pico W**. O assistente captura áudio, converte para PCM, envia para um serviço externo via Wi-Fi e recebe comandos JSON em resposta.  

## 📋 Pré-requisitos  

Antes de começar, certifique-se de ter instalado:  

- [CMake](https://cmake.org/download/)  
- [Ninja Build System](https://ninja-build.org/)  
- [Extensão Raspberry Pi Pico para VS Code](https://marketplace.visualstudio.com/items?itemName=RaspberryPi.rp2040-dev)  
- [Putty (para depuração via serial)](https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html)  

## 📥 Clonagem do repositório  

```bash
git clone https://github.com/jairopaiva/assistente_pessoal.git
cd assistente_pessoal
```

## 🔨 Build do projeto  

No diretório raiz do projeto, execute o seguinte comando para gerar os arquivos de build:  

```bash
cmake -S . -B build -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DPICO_BOARD=pico_w
```

## ▶️ Executando no Raspberry Pi Pico W  

1. Com a extensão **Raspberry Pi Pico** no **VS Code**, clique em **Run Project (USB)**.  
2. Após a gravação do firmware, desconecte e reconecte a placa.  

## 🛠️ Depuração  

Para depurar via **serial**, conecte-se à porta da placa usando o **Putty** com a velocidade de **115200 baud**.  

1. Abra o **Putty**.  
2. Selecione "Serial" e insira a porta correspondente (exemplo: `COM3` no Windows ou `/dev/ttyUSB0` no Linux).  
3. Configure a velocidade para **115200**.  
4. Clique em "Open" para iniciar a sessão.  
