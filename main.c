#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "pico/cyw43_arch.h" // Para Wi-Fi no RP2040W

// ------------------------------------------------
// DEFINES
// ------------------------------------------------
#define BTN_PIN 5 // Botão no GPIO 5
#define MIC_CHANNEL 2
#define MIC_PIN (26 + MIC_CHANNEL) // 28
#define SAMPLE_RATE 8000
#define RECORD_SECONDS 3
#define SAMPLES (SAMPLE_RATE * RECORD_SECONDS)
#define ADC_BIAS 2048

// Wi-Fi
#define WIFI_SSID "VIVOFIBRA-61D8"
#define WIFI_PASSWORD "ACA4D43DDD"

// URL base
#define BASE_URL "https://bitdoglab.milleniumsolutions.workers.dev/command"

// LED RGB (definindo os pinos)
#define LED_R_PIN       11
#define LED_G_PIN       12
#define LED_B_PIN       13

// Buffers de captura
static uint16_t adc_buf[SAMPLES]; // Recebe amostras 12 bits do ADC
static int16_t pcm_buf[SAMPLES];  // Armazena PCM 16 bits (-32768..32767)

// DMA
static uint dma_channel;
static dma_channel_config dma_cfg;

// ------------------------------------------------
// Inicializa ADC e DMA
// ------------------------------------------------
static void init_adc_dma(void)
{
    adc_init();
    adc_set_temp_sensor_enabled(false);
    adc_select_input(MIC_CHANNEL);

    gpio_init(MIC_PIN);
    gpio_set_dir(MIC_PIN, false);

    // Configura FIFO do ADC
    adc_fifo_setup(
        true,  // Write each completed conversion to the FIFO
        true,  // Enable DMA data request (DREQ)
        1,     // DREQ (Request) a cada sample
        false, // No ERR
        false  // 12 bits (não shiftar para 8)
    );

    // Exemplo: 48 MHz / 6000 = 8 kHz
    adc_set_clkdiv(6000.0f);

    // Aloca canal de DMA
    dma_channel = dma_claim_unused_channel(true);
    dma_cfg = dma_channel_get_default_config(dma_channel);

    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_cfg, false);
    channel_config_set_write_increment(&dma_cfg, true);
    channel_config_set_dreq(&dma_cfg, DREQ_ADC);

    dma_channel_configure(dma_channel, &dma_cfg,
                          adc_buf,       // Destino
                          &adc_hw->fifo, // Origem (FIFO do ADC)
                          SAMPLES,       // Número de amostras
                          false);        // Não inicia agora
}

// ------------------------------------------------
/// Inicializa LED RGB
// ------------------------------------------------
static void init_led_rgb(void)
{

    gpio_init(LED_R_PIN);
    gpio_set_dir(LED_R_PIN, GPIO_OUT);
    gpio_put(LED_R_PIN, 0); // Inicialmente desligado

    gpio_init(LED_G_PIN);
    gpio_set_dir(LED_G_PIN, GPIO_OUT);
    gpio_put(LED_G_PIN, 0); // Inicialmente desligado

    gpio_init(LED_B_PIN);
    gpio_set_dir(LED_B_PIN, GPIO_OUT);
    gpio_put(LED_B_PIN, 0); // Inicialmente desligado
}

// ------------------------------------------------
// Captura SAMPLES amostras via DMA
// ------------------------------------------------
static void record_mic(uint16_t *buf)
{
    adc_fifo_drain();
    adc_run(false);

    dma_channel_configure(dma_channel, &dma_cfg,
                          buf,
                          &adc_hw->fifo,
                          SAMPLES,
                          true); // Inicia DMA

    adc_run(true);
    dma_channel_wait_for_finish_blocking(dma_channel);
    adc_run(false);
}

// ------------------------------------------------
// Converte amostras ADC (0..4095) para PCM 16 bits
// PCM[-32768..32767], subtraindo 2048 e shiftando 4 bits
// ------------------------------------------------
static void pcm_convert_audio(int16_t *dest, const uint16_t *src)
{
    for (size_t i = 0; i < SAMPLES; i++)
    {
        int32_t val12 = (src[i] & 0x0FFF) - ADC_BIAS;
        dest[i] = (int16_t)(val12 << 4);
    }
}

/**
 * Encodes binary data into a Base64-encoded string.
 *
 * @param data     Pointer to the input data.
 * @param length   Length (in bytes) of the input data.
 * @param out_len  Pointer to a size_t that will hold the length
 *                 of the Base64-encoded string (excluding the null terminator).
 * @return         A pointer to a newly allocated null-terminated
 *                 Base64 string. Caller is responsible for freeing it.
 */
static char *my_base64_encode(const uint8_t *data, size_t length, size_t *out_len)
{
    static const char b64_table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    // Each group of 3 bytes becomes 4 Base64 characters.
    // If the total length is not a multiple of 3, then we'll have '=' padding.
    // Base64 output length formula: 4 * ceil(length/3)
    size_t encoded_len = 4 * ((length + 2) / 3);

    // Allocate enough space for the output (plus null terminator).
    char *encoded = (char *)malloc(encoded_len + 1);
    if (!encoded)
    {
        if (out_len)
            *out_len = 0;
        return NULL;
    }

    size_t i = 0, j = 0;
    // Process data in chunks of 3 bytes
    while (i < length)
    {
        uint32_t octet_a = i < length ? data[i++] : 0;
        uint32_t octet_b = i < length ? data[i++] : 0;
        uint32_t octet_c = i < length ? data[i++] : 0;

        // Combine the three bytes into a single 24-bit value
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | (octet_c);

        // Extract and map each 6-bit group
        encoded[j++] = b64_table[(triple >> 18) & 0x3F];
        encoded[j++] = b64_table[(triple >> 12) & 0x3F];
        encoded[j++] = b64_table[(triple >> 6) & 0x3F];
        encoded[j++] = b64_table[triple & 0x3F];
    }

    // Add padding if necessary
    size_t mod = length % 3;
    if (mod > 0)
    {
        // For each missing byte in the last 3-byte block, replace the last character(s) with '='
        encoded[encoded_len - 1] = (mod == 1) ? '=' : encoded[encoded_len - 1];
        encoded[encoded_len - 2] = (mod == 1 || mod == 2) ? '=' : encoded[encoded_len - 2];
    }

    // Null-terminate the string
    encoded[encoded_len] = '\0';

    // Optionally return the output length (excluding the null terminator).
    if (out_len)
    {
        *out_len = encoded_len;
    }

    return encoded;
}

// ------------------------------------------------
// Exemplo bem simplificado de GET HTTP (pseudocódigo).
// ------------------------------------------------
static bool http_get_with_audio(const char *base64_audio, char *response, size_t resp_size)
{
    // Monta a URL final, enviando em querystring
    // ex: https://.../command?audio_data=base64.....
    // CUIDADO: Se for muito grande, excede limite do GET
    char url[1024];
    snprintf(url, sizeof(url), "%s?audio_data=%s", BASE_URL, base64_audio);

    // *** AQUI entram as rotinas de socket/HTTP de verdade ***
    // Por ex. lwIP raw, ou usando pico-curl, etc.
    // Exemplo fictício de "fazer GET" e retornar a resposta JSON.

    printf("[DEBUG] GET -> %s\n", url);
    // Supondo que a API retorne algo como {"command":"LIGAR_LUZ"}
    // pOR ORA IREMOS SIMULAR O PROCESSAMENTO EXTERNO
    if(!gpio_get(LED_R_PIN) && !gpio_get(LED_R_PIN) && !gpio_get(LED_R_PIN)){
        snprintf(response, resp_size, "{\"command\":\"LIGAR_LUZ\"}");
    }
    else{
        snprintf(response, resp_size, "{\"command\":\"APAGAR_LUZ\"}");
    }
    return true;
}

// ------------------------------------------------
// MAIN
// ------------------------------------------------
int main()
{
    stdio_init_all();

    // Inicia Wi-Fi (lib cyw43 do Pico W)
    if (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi.\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();

    // Conecta na rede
    printf("Conectando ao Wi-Fi %s...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_MIXED_PSK) != 0)
    {
        printf("Falha ao conectar Wi-Fi.\n");
    }
    else
    {
        printf("Conectado com sucesso!\n");
    }

    // Inicializa ADC + DMA
    init_adc_dma();

    // Inicializa LED
    init_led_rgb();

    // Inicializa o pino do botão (GPIO 5)
    gpio_init(BTN_PIN);
    gpio_set_dir(BTN_PIN, GPIO_IN);
    gpio_pull_up(BTN_PIN); // Exemplo: se seu hardware exigir pull-up

    printf("Aguardando pressionamento do botão (GPIO 5)...\n");

    while (true)
    {
        // Se o botão estiver em nível BAIXO (caso seja pull-up),
        // significa que está pressionado
        if (gpio_get(BTN_PIN) == 0)
        {
            // Debounce simples
            sleep_ms(50);
            if (gpio_get(BTN_PIN) == 0)
            {
                printf("[INFO] Botão pressionado! Iniciando gravação.\n");

                // 1) Grava audio
                record_mic(adc_buf);

                // 2) Converte para PCM
                pcm_convert_audio(pcm_buf, adc_buf);

                // 3) Codifica em Base64
                size_t b64_len = 0;
                char *b64_audio = my_base64_encode(
                    (const uint8_t *)pcm_buf,
                    (SAMPLES * sizeof(int16_t)),
                    &b64_len);
                if (!b64_audio)
                {
                    printf("[ERRO] Falha ao gerar base64.\n");
                    continue;
                }

                // 4) Envia via GET e obtém a resposta da API
                char resp[256] = {0};
                if (http_get_with_audio(b64_audio, resp, sizeof(resp)))
                {
                    printf("[INFO] Resposta da API:\n%s\n", resp);

                    // Se a resposta contiver "LIGAR_LUZ", acende o LED RGB
                    if (strstr(resp, "LIGAR_LUZ") != NULL)
                    {
                        printf("[INFO] Comando LIGAR_LUZ detectado. Acendendo LED RGB.\n");
                        gpio_put(LED_R_PIN, 1);
                        gpio_put(LED_G_PIN, 1);
                        gpio_put(LED_B_PIN, 1);
                    }
                    // Se a resposta contiver "LIGAR_LUZ", acende o LED RGB
                    else if (strstr(resp, "APAGAR_LUZ") != NULL)
                    {
                        printf("[INFO] Comando APAGAR_LUZ detectado. Acendendo LED RGB.\n");
                        gpio_put(LED_R_PIN, 0);
                        gpio_put(LED_G_PIN, 0);
                        gpio_put(LED_B_PIN, 0);
                    }
                }
                else
                {
                    printf("[ERRO] Falha ao fazer GET.\n");
                }

                free(b64_audio);

                // Aguarda o botão ser solto antes de continuar
                while (gpio_get(BTN_PIN) == 0)
                {
                    tight_loop_contents();
                }
            }
        }

        tight_loop_contents();
    }

    cyw43_arch_deinit();
    return 0;
}
