// -----------------------------------------------------------------------------
// Projeto: Green Life Webserver - Monitoramento Ambiental com a Raspberry Pi Pico W
// Autor: Levi Silva Freitas
// Descricao: Servidor HTTP embarcado com alternancia de modo de monitoramento
// -----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

// ----------------------------- Definicoes de Hardware ------------------------------
#define WIFI_SSID "Casa1"
#define WIFI_PASSWORD "40302010"

#define LED_PIN CYW43_WL_GPIO_LED_PIN
#define ADC_UMIDADE_PIN 26      // ADC0 (GPIO26)
#define ADC_TEMPERATURA_PIN 27  // ADC1 (GPIO27)
#define BOTAO_MODO 5            // GPIO5 - Alternar modo de leitura

// ----------------------------- Variaveis Globais ----------------------------------
volatile bool modo_manual = false; // False = automatico (sensor interno), True = joystick
volatile uint64_t ultima_troca_modo = 0;

// ----------------------------- Prototipos de Funcoes ------------------------------
void configurar_botao_modo(void);
void botao_modo_callback(uint gpio, uint32_t eventos);

float ler_temperatura_interna(void);
float ler_temperatura_joystick(void);
float ler_umidade_joystick(void);

void interpretar_estado_planta(float temp, float umid, char *msg, size_t tamanho);

static err_t aceitar_conexao_tcp(void *arg, struct tcp_pcb *novo_pcb, err_t err);
static err_t receber_dados_tcp(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

// ---------------------------------- Main ------------------------------------------
int main() {
    stdio_init_all();

    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_gpio_init(ADC_UMIDADE_PIN);
    adc_gpio_init(ADC_TEMPERATURA_PIN);

    configurar_botao_modo();

    while (cyw43_arch_init()) {
        printf("Erro ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    cyw43_arch_gpio_put(LED_PIN, 0);
    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        printf("Falha na conexao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }
    printf("Conectado com IP: %s\n", ipaddr_ntoa(&netif_default->ip_addr));

    struct tcp_pcb *server = tcp_new();
    if (!server || tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao configurar servidor TCP\n");
        return -1;
    }

    server = tcp_listen(server);
    tcp_accept(server, aceitar_conexao_tcp);
    printf("Servidor HTTP ativo na porta 80\n");

    while (true) {
        cyw43_arch_poll();
        sleep_ms(100);
    }

    cyw43_arch_deinit();
    return 0;
}

// ----------------------------- Inicializacao de GPIO ------------------------------
void configurar_botao_modo(void) {
    gpio_init(BOTAO_MODO);
    gpio_set_dir(BOTAO_MODO, GPIO_IN);
    gpio_pull_up(BOTAO_MODO);
    gpio_set_irq_enabled_with_callback(BOTAO_MODO, GPIO_IRQ_EDGE_FALL, true, botao_modo_callback);
}

// ----------------------------- Leitura de Sensores --------------------------------
float ler_temperatura_interna(void) {
    adc_select_input(4);
    uint16_t valor_bruto = adc_read();
    const float fator_conv = 3.3f / (1 << 12);
    return 27.0f - ((valor_bruto * fator_conv) - 0.706f) / 0.001721f;
}

float ler_temperatura_joystick(void) {
    adc_select_input(1);
    return (adc_read() / 4095.0f) * 60.0f;
}

float ler_umidade_joystick(void) {
    adc_select_input(0);
    return (adc_read() / 4095.0f) * 100.0f;
}

// ----------------------------- Logica de Avaliacao --------------------------------
void interpretar_estado_planta(float temp, float umid, char *msg, size_t tamanho) {
    bool temp_ok = temp >= 20.0 && temp <= 40.0;
    bool umid_ok = umid >= 20.0 && umid <= 80.0;

    if (temp_ok && umid_ok)
        strncpy(msg, "Sua planta est&aacute; feliz!", tamanho);
    else if (!temp_ok && !umid_ok)
        strncpy(msg, "Sua planta est&aacute; em perigo!", tamanho);
    else if (!temp_ok)
        strncpy(msg, temp < 20.0 ? "Sua planta est&aacute; com frio!" : "Sua planta est&aacute; com calor!", tamanho);
    else if (!umid_ok)
        strncpy(msg, umid < 20.0 ? "Sua planta est&aacute; com sede!" : "Excesso de &aacute;gua detectado!", tamanho);

    msg[tamanho - 1] = '\0';
}

// ----------------------------- Controle via Botao ---------------------------------
void botao_modo_callback(uint gpio, uint32_t eventos) {
    uint64_t agora = to_us_since_boot(get_absolute_time());
    if ((agora - ultima_troca_modo) > 300000) {
        modo_manual = !modo_manual;
        ultima_troca_modo = agora;
    }
}

// ----------------------------- HTTP e TCP -----------------------------------------
static err_t aceitar_conexao_tcp(void *arg, struct tcp_pcb *novo_pcb, err_t err) {
    tcp_recv(novo_pcb, receber_dados_tcp);
    return ERR_OK;
}

static err_t receber_dados_tcp(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    float temperatura = modo_manual ? ler_temperatura_joystick() : ler_temperatura_interna();
    float umidade = ler_umidade_joystick();

    char status_msg[64];
    interpretar_estado_planta(temperatura, umidade, status_msg, sizeof(status_msg));
    const char *modo_str = modo_manual ? "Manual" : "Autom&aacute;tico";

    char html[1024];
    snprintf(html, sizeof(html),
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
        "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='1'>"
        "<title>Green Life</title><style>body{background:#e6ffe6;font-family:Arial;text-align:center;padding:20px;}"
        "h1{font-size:48px;color:#228B22;}.sensor{font-size:28px;margin-top:20px;}"
        ".status{font-size:32px;color:#333;margin-top:30px;font-weight:bold;}"
        "img{width:200px;margin-top:20px;border-radius:10px;}</style>"
        "<script>setTimeout(function(){ location.reload(); }, 1000);</script>"
        "</head><body>"
        "<h1>Green Life</h1><p class='sensor'><strong>Modo de monitoramento:</strong> %s</p>"
        "<img src='https://cdn-icons-png.flaticon.com/512/628/628324.png' alt='Planta'>"
        "<p class='sensor'>Temperatura atual: %.2f &deg;C</p><p class='sensor'>Umidade atual: %.1f%%</p>"
        "<p class='status'>%s</p></body></html>", modo_str, temperatura, umidade, status_msg);

    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    free(request);
    pbuf_free(p);

    return ERR_OK;
}
