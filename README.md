# 🌱 GreenLife: Web Monitoramento de Umidade e Temperatura para Plantas

## 🎯 Objetivo Geral  
Criar um sistema embarcado que simula o monitoramento ambiental de uma planta, exibindo **temperatura** e **umidade** em tempo real via uma **interface web** acessível por Wi-Fi.  
O objetivo é proporcionar um **protótipo funcional e interativo**, utilizando a **Raspberry Pi Pico W** com o kit **BitDogLab**, aplicando conceitos práticos de **IoT, sensoriamento e conectividade embarcada**.

---

## ⚙️ Descrição Funcional

A aplicação é um **servidor HTTP embarcado**, escrito em C, que roda diretamente na Raspberry Pi Pico W. Seu fluxo funciona da seguinte forma:

1. **Inicialização dos Periféricos**
   - ADCs ativados para leitura de joystick.
   - Botão físico configurado com **interrupção**.
   - Conexão à rede Wi-Fi com **módulo CYW43**.

2. **Servidor TCP na porta 80**
   - Usa a **pilha lwIP** para lidar com conexões e responder a requisições HTTP.
   - A cada requisição, a função `receber_dados_tcp()` gera uma **resposta HTML dinâmica**.

3. **Leitura dos Sensores**
   - **Umidade**: sempre lida do eixo Y do joystick (ADC0).
   - **Temperatura**: 
     - Modo **automático**: sensor interno da Pico (ADC4).
     - Modo **manual**: eixo X do joystick (ADC1).
   - A troca de modo é feita por **botão físico** com `botao_modo_callback()`.

4. **Análise das Condições**
   - A função `interpretar_estado_planta()` analisa os dados.
   - Gera mensagens como:
     - `"Sua planta está feliz!"`
     - `"Sua planta está com sede!"`, etc.

5. **Interface Web**
   - Exibe:
     - 📡 Modo atual (Automático ou Manual)
     - 🌡️ Temperatura atual
     - 💧 Umidade atual
     - ✅ Status da planta
   - A página se **atualiza a cada segundo automaticamente** com `<meta refresh>`.

---

## 🧩 Uso dos Periféricos da BitDogLab

### 🎮 Joystick
- **Eixos Analógicos**:
  - **X (GPIO27)** → simula temperatura (modo manual).
  - **Y (GPIO26)** → simula umidade (modo fixo).
- Leitura feita via **ADC da Pico**, convertida para °C e %.
- Permite **simular diferentes condições ambientais** manualmente, de forma prática e precisa.

### 🔘 Botão Físico (GPIO5)
- Configurado com **pull-up e interrupção**.
- Alterna entre:
  - **Modo Automático** (sensor interno da Pico).
  - **Modo Manual** (joystick).
- A interação reflete **imediatamente na interface web**, permitindo testes ao vivo.

### 📶 Módulo Wi-Fi CYW43 (embutido na Pico W)
- Ativado com `cyw43_arch_enable_sta_mode()`.
- Conecta à rede e mantém comunicação com o navegador via **protocolo HTTP**.
- Responsável por:
  - Criar o **servidor TCP**.
  - Enviar a **interface HTML dinâmica** com os dados do sistema.
- Tornou possível o monitoramento **sem uso de displays físicos**, direto pelo navegador.

---
