//Incluindo as bibliotecas que serão utilizadas
#include "hardware/pio.h"  
#include "hardware/i2c.h" 
#include "pico/stdlib.h" 
#include "ws2818b.pio.h"
#include "ssd1306.h"   
#include <stdlib.h> 
#include <stdio.h>   
#include <math.h>   
#include "font.h"  

// Definições as portas utilizadas 
#define I2C_PORT i2c1    // Porta I2C utilizada
#define I2C_SDA 14      
#define I2C_SCL 15     
#define ENDERECO 0x3C   // Endereço do display SSD1306

#define BOTAO_A 5      
#define BOTAO_B 6     
#define azul 12              //RGB
#define verde 11            //RGB
#define vermelho 13        //RGB

volatile uint32_t last_time = 0; 
bool led_green = false, led_blue = false, cor = true;
ssd1306_t ssd;  
char valor_atual = 0; 

//Inicializar os pinos do led RGB
void iniciar_rgb() {
    gpio_init(vermelho);
    gpio_init(verde);
    gpio_init(azul);
  
    //Configurando como saída
    gpio_set_dir(vermelho, GPIO_OUT);
    gpio_set_dir(verde, GPIO_OUT);
    gpio_set_dir(azul, GPIO_OUT);
  }

// Função o estado dos LEDs RGB
void state(bool rr, bool gg, bool bb) {
  gpio_put(vermelho, rr);    
  gpio_put(verde, gg); 
  gpio_put(azul, bb);  
}

// Inicializando e configurando o display SSD1306 
void display() {
    // Inicializa a comunicação I2C com velocidade de 400 kHz
    i2c_init(I2C_PORT, 400000); 

    // Configurando os pino para função I2C
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);  
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    // Ativando o resistor pull-up
    gpio_pull_up(I2C_SDA);   
    gpio_pull_up(I2C_SCL); 

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT); // Inicializa o display SSD1306
    ssd1306_config(&ssd);           // Configura o display
    ssd1306_send_data(&ssd);        // Atualiza o display
    ssd1306_fill(&ssd, false);      // Limpa
    ssd1306_send_data(&ssd);        // Atualiza o display
}

void botaoA(bool *led_green, bool *led_blue){
    *led_green = !(*led_green);
    bool cor = true;
    ssd1306_fill(&ssd, !cor); // Limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
    ssd1306_draw_string(&ssd, "Led Verde", 20, 20);
    ssd1306_draw_string(&ssd, *led_green ? "ligado" : "desligado", 20, 40); // Exibe estado do LED
    ssd1306_send_data(&ssd); // Atualiza o display
    printf("Led verde %s\n", *led_green ? "ligado" : "desligado");
    state(0, *led_green, *led_blue); // Alterna o estado do LED 
}

void botaoB(bool *led_green, bool *led_blue){
*led_blue = !(*led_blue);
bool cor = true;
ssd1306_fill(&ssd, !cor); // Limpa o display
ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Desenha um retângulo
ssd1306_draw_string(&ssd, "Led Azul", 20, 20);  
ssd1306_draw_string(&ssd, *led_blue ? "ligado" : "desligado", 20, 40); // Exibe estado do LED
ssd1306_send_data(&ssd); // Atualiza o display
printf("Led Azul %s\n", *led_blue ? "ligado" : "desligado");
state(0, *led_green, *led_blue); // Alterna o estado do LED
}

// Callback chamado quando ocorre interrupção em algum botão
void botao_callback(uint gpio, uint32_t eventos) {
    // Obtém o tempo atual em us
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    // Verifica se passou tempo suficiente desde o último evento
    if (current_time - last_time > 250000) // 250 ms
    {
        last_time = current_time; // Atualiza o tempo do último evento
        if (gpio == BOTAO_A) { //  Botão A foi pressionado
            botaoA(&led_green, &led_blue);
        }
        else if(gpio == BOTAO_B){
            botaoB(&led_green, &led_blue);
        }
}}

// Função para inicializar os botões
void iniciar_botoes(void) {
    // Inicializa os pinos
    gpio_init(BOTAO_A); 
    gpio_init(BOTAO_B);
    // Configurando como entrada
    gpio_set_dir(BOTAO_A, GPIO_IN); 
    gpio_set_dir(BOTAO_B, GPIO_IN);
    // Resistor de pull-up
    gpio_pull_up(BOTAO_A); 
    gpio_pull_up(BOTAO_B);
    // Interrupções para os botões
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, botao_callback);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, botao_callback); 
}

/* Função para indexar as posições dos led da matriz */
int getIndex(int x, int y) {
    if (x % 2 == 0) {
        return  24 - (x * 5 + y);
    } else {
        return 24 - (x * 5 + (4 - y));
    }
}

// Definição da estrutura de cor para cada LED
struct pixel_t {
    uint8_t R, G, B; // Componentes de cor: vermelho, verde e azul
};
typedef struct pixel_t npLED_t; // Cria um tipo npLED_t baseado em pixel_t
npLED_t leds[25]; // Vetor que armazena as cores de todos os LEDs

// PIO e state machine para controle dos LEDs
PIO np_pio; // PIO utilizado para comunicação com os LEDs
uint sm;   // State machine associada ao PIO

// Função para atualizar os LEDs da matriz e salvar o estado "buffer"
void buffer() {
    for (uint i = 0; i < 25; i++) {
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100); // Aguarda 100 microsegundos para estabilizar
}

// Função de inicializar a matriz de LEDs com a ws2818b.
void iniciar_matriz() {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, true);
    ws2818b_program_init(np_pio, sm, offset, 7, 800000.f);

    for (uint i = 0; i < 25; i++) {
        leds[i].R = leds[i].G = leds[i].B = 0;
    }
    buffer();
}
// Função para configurar a cor de um LED específico
void led_cor(const uint indice, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[indice].R = r; // Define o componente vermelho
    leds[indice].G = g; // Define o componente verde
    leds[indice].B = b; // Define o componente azul
}
// Função para desligar todos os LEDs
void desliga() {
    for (uint i = 0; i < 25; ++i) { // Percorre todos os LEDs
        led_cor(i, 0, 0, 0); // Define a cor preta (desligado) para cada LED
    }
    buffer(); // Atualiza o estado dos LEDs
}

// Conjunto de funções para ligar os leds e formar o numero na matriz
void num0() {
    int mat1[5][5][3] = {
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0},{0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 255, 0},  {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 255, 0},  { 0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 255, 0},  {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0},{0, 0, 0}}
    };
    //Exibir na matriz
    for (int linha = 0; linha < 5; linha++) {
        for (int cols = 0; cols < 5; cols++) {
            int posicao = getIndex(linha, cols);
            led_cor(posicao, mat1[linha][cols][0], mat1[linha][cols][1], mat1[linha][cols][2]);
        }
    }
    buffer();
}

void num1() {
    int mat1[5][5][3] = {
        {{0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}}
    };

    // Exibir na matriz
    for (int linha = 0; linha < 5; linha++) {
        for (int cols = 0; cols < 5; cols++) {
            int posicao = getIndex(linha, cols);
            led_cor(posicao, mat1[linha][cols][0], mat1[linha][cols][1], mat1[linha][cols][2]);
        }
    }
    buffer();
}

void num2() {
    int mat1[5][5][3] = {
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}}
    };

    // Exibir na matriz
    for (int linha = 0; linha < 5; linha++) {
        for (int cols = 0; cols < 5; cols++) {
            int posicao = getIndex(linha, cols);
            led_cor(posicao, mat1[linha][cols][0], mat1[linha][cols][1], mat1[linha][cols][2]);
        }
    }
    buffer();
}

void num3() {
    int mat1[5][5][3] = {
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0  }},
        {{0, 0, 0}, {0, 0, 0},     {0, 0,     0}, {0, 255, 0}, {0, 0,   0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0,   0}},
        {{0, 0, 0}, {0, 0, 0},     {0, 0,     0}, {0, 255, 0}, {0, 0,   0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0,   0}}
    };

    // Exibir na matriz
    for (int linha = 0; linha < 5; linha++) {
        for (int cols = 0; cols < 5; cols++) {
            int posicao = getIndex(linha, cols);
            led_cor(posicao, mat1[linha][cols][0], mat1[linha][cols][1], mat1[linha][cols][2]);
        }
    }
    buffer();
}

void num4() {
    int mat1[5][5][3] = {
        {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}}
    };

    // Exibir na matriz
    for (int linha = 0; linha < 5; linha++) {
        for (int cols = 0; cols < 5; cols++) {
            int posicao = getIndex(linha, cols);
            led_cor(posicao, mat1[linha][cols][0], mat1[linha][cols][1], mat1[linha][cols][2]);
        }
    }
    buffer();
}

void num5() {
    int mat1[5][5][3] = {
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}}
    
    };

    // Exibir na matriz
    for (int linha = 0; linha < 5; linha++) {
        for (int cols = 0; cols < 5; cols++) {
            int posicao = getIndex(linha, cols);
            led_cor(posicao, mat1[linha][cols][0], mat1[linha][cols][1], mat1[linha][cols][2]);
        }
    }
    buffer();
}

void num6() {
    int mat1[5][5][3] = {
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}}
    };

    // Exibir na matriz
    for (int linha = 0; linha < 5; linha++) {
        for (int cols = 0; cols < 5; cols++) {
            int posicao = getIndex(linha, cols);
            led_cor(posicao, mat1[linha][cols][0], mat1[linha][cols][1], mat1[linha][cols][2]);
        }
    }
    buffer();
}

void num7() {
    int mat1[5][5][3] = {
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}}
    };

    // Exibir na matriz
    for (int linha = 0; linha < 5; linha++) {
        for (int cols = 0; cols < 5; cols++) {
            int posicao = getIndex(linha, cols);
            led_cor(posicao, mat1[linha][cols][0], mat1[linha][cols][1], mat1[linha][cols][2]);
        }
    }
    buffer();
}

void num8() {
    int mat1[5][5][3] = {
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}}
    };

    // Exibir na matriz
    for (int linha = 0; linha < 5; linha++) {
        for (int cols = 0; cols < 5; cols++) {
            int posicao = getIndex(linha, cols);
            led_cor(posicao, mat1[linha][cols][0], mat1[linha][cols][1], mat1[linha][cols][2]);
        }
    }
    buffer();
}

void num9() {
    int mat1[5][5][3] = {
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 255, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 255, 0}, {0, 255, 0}, {0, 255, 0}, {0, 0, 0}}

    };

    // Exibir na matriz
    for (int linha = 0; linha < 5; linha++) {
        for (int cols = 0; cols < 5; cols++) {
            int posicao = getIndex(linha, cols);
            led_cor(posicao, mat1[linha][cols][0], mat1[linha][cols][1], mat1[linha][cols][2]);
        }
    }
    buffer();
}

// Função base com switch para atualizar do número na matriz de acordo com a variavel valor_atual
void atualizar_valor(char valor_atual) {
    switch (valor_atual) {
        case '0':
            num0();
            
            break;
        case '1':
            num1();
            
            break;
        case '2':
            num2();
            
            break;
        case '3':
            num3();
            
            break;
        case '4':
            num4();
            
            break;
        case '5':
            num5();
            
            break;
        case '6':
            num6();
    
            break;
        case '7':
            num7();
        
            break;
        case '8':
            num8();
    
            break;
        case '9':
            num9();
        
            break;
        default: 
            desliga();
            
            break;
    }
}

void boas_vindas(){
        // Exibição inicial no display OLED
        ssd1306_fill(&ssd, !cor);                             // Limpa o display preenchendo com a cor oposta ao valor atual de "cor"
        ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);        // Desenha um retângulo com bordas dentro das coordenadas especificadas
        ssd1306_draw_string(&ssd, "Ola", 25, 10);       // Escreve "Utilize" na posição (20, 10) do display
        ssd1306_draw_string(&ssd, "Aperte", 18, 30);// Escreve "os botoes" na posição (20, 30) do display
        ssd1306_draw_string(&ssd, "uma tecla", 18, 50);// Escreve "ou o teclado" na posição (20, 50) do display
        ssd1306_send_data(&ssd);                         // Envia os dados para atualizar o display
}

int main() {

    stdio_init_all();
    iniciar_matriz();
    iniciar_botoes();   
    iniciar_rgb();   
    display();
    boas_vindas();       

// Loop infinito 
while (true) {
    cor = !cor;  
    ssd1306_send_data(&ssd);  
    sleep_ms(500);  

    printf("Digite um caractere: ");                         
    fflush(stdout);                                         // Limpar o buffer
    if (scanf(" %c", &valor_atual) == 1) {              
        atualizar_valor(valor_atual);                   
        printf("%c\n", valor_atual);                    
        ssd1306_fill(&ssd, !cor);                       
        ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);  
        ssd1306_draw_string(&ssd, &valor_atual, 63, 29);
        ssd1306_send_data(&ssd);                     
    }}
    return 0;
}
