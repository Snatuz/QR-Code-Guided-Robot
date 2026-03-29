#include <cstdlib>
#include <fstream>
#include <iostream>
#include <pigpio.h>
#include <sstream>
#include <unistd.h>
#include <vector>

//       ORGANIZADO E DOCUMENTADO POR IA, FEITO POR MIGUEL (SNATUZ).
//--------------------------------------------------------------------------------//
// MAPEAMENTO DE PINOS GPIO

#define pinencoder_esq 7
#define pinencoder_dir 8
#define IN1 2   // Motor esquerdo
#define IN2 3   // Motor esquerdo
#define IN3 17  // Motor direito
#define IN4 27  // Motor direito
#define EN1 4   // PWM motor esquerdo
#define EN2 22  // PWM motor direito

//--------------------------------------------------------------------------------//
// PROTOTIPOS DE FUNÇÕES

int spin(int angle);
void callback_esq(int gpio, int level, uint32_t tick);
void callback_dir(int gpio, int level, uint32_t tick);
void spin_start();
void stop();
void forward(int distance);
int spin_by_ticks(int ticks);
std::vector<std::string> splitString(const std::string &str, char delimiter = ',');
void processarComandos(const std::vector<std::string> &comandos);
int QR_search();
std::string get_qrcode();

//--------------------------------------------------------------------------------//
// VARIÁVEIS GLOBAIS

int distance_counter = 0;
std::string qr_code_data;

//--------------------------------------------------------------------------------//
// STRUCT DO MOTOR

struct motor {
    unsigned int PWM;
    unsigned int previous_PWM = 0;
    unsigned int velocity;
    unsigned int encoder_counter = 0;
    unsigned int previous_encoder_counter = 0;
    float error;
    float previous_error;
    float correction;
    float derivative;
    float integral;
} typedef motor;

motor l_motor;
motor r_motor;
motor *left_motor  = &l_motor;
motor *right_motor = &r_motor;

// Constantes PID (reservadas para implementação futura)
float KP = 0;
float KI = 0;
float KD = 0;
float setpoint = 0;

//--------------------------------------------------------------------------------//
// FUNÇÕES

std::string get_qrcode() {
    system("./qrcode_reader");

    std::ifstream arquivo("qrcode.txt");
    std::string buffer;
    std::getline(arquivo, buffer);

    return (buffer == "empty") ? "empty" : buffer;
}

void callback_esq(int gpio, int level, uint32_t tick) {
    left_motor->encoder_counter++;
}

void callback_dir(int gpio, int level, uint32_t tick) {
    right_motor->encoder_counter++;
    distance_counter++;
}

void stop() {
    gpioWrite(EN1, 0);
    gpioWrite(EN2, 0);
    gpioPWM(IN1, 0);
    gpioPWM(IN2, 0);
    gpioPWM(IN3, 0);
    gpioPWM(IN4, 0);
}

void forward(int distance) {
    stop();
    gpioWrite(IN1, 1);
    gpioWrite(IN3, 1);
    gpioPWM(EN1, 900);
    gpioPWM(EN2, 920); // Valores calibrados empiricamente para andar reto

    distance_counter = 0;
    while (distance_counter < distance) {
        usleep(50000);
    }
    stop();
}

void spin_start() {
    gpioPWM(EN2, 900);
}

int spin(int angle) {
    stop();

    if (angle > 0) {
        gpioWrite(IN3, 0);
        gpioWrite(IN4, 1);
    } else {
        gpioWrite(IN3, 1);
        gpioWrite(IN4, 0);
    }

    if ((angle > 360 && angle != 999) || (angle < -360 && angle != -999)) {
        std::cout << "\nAngulo invalido. Comando ignorado. Prosseguindo...\n";
        return 0;
    }

    if (angle == 999 || angle == -999) {
        spin_start();
        return 0;
    }

    angle = (angle < 0) ? -angle : angle;

    spin_start();

    distance_counter = 0;
    while (distance_counter < (angle * 0.3)) {
        usleep(50000);
    }

    stop();
    return 0;
}

int spin_by_ticks(int ticks) {
    stop();

    if (ticks > 0) {
        gpioWrite(IN1, 1);
        gpioWrite(IN2, 0);
        gpioWrite(IN3, 0);
        gpioWrite(IN4, 1);
    } else {
        gpioWrite(IN1, 0);
        gpioWrite(IN2, 1);
        gpioWrite(IN3, 1);
        gpioWrite(IN4, 0);
    }

    ticks = (ticks < 0) ? -ticks : ticks;

    spin_start();

    distance_counter = 0;
    while (distance_counter < ticks) {
        usleep(100000);
    }

    stop();
    return 0;
}

std::vector<std::string> splitString(const std::string &str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

void processarComandos(const std::vector<std::string> &comandos) {
    std::cout << "Iniciando processamento de comandos..." << std::endl;

    bool chamarFuncaoF = true;

    for (const std::string &cmd : comandos) {

        if (cmd == "E") {
            std::cout << "Comando 'E' encontrado. Encerrando." << std::endl;
            exit(0);
        }

        int valorNumerico;
        try {
            valorNumerico = std::stoi(cmd);
        } catch (const std::invalid_argument &e) {
            std::cerr << "ERRO: '" << cmd << "' nao e um numero valido. Pulando." << std::endl;
            continue;
        }

        if (chamarFuncaoF) {
            forward(valorNumerico);
        } else {
            spin_by_ticks(valorNumerico);
        }

        usleep(500000);
        chamarFuncaoF = !chamarFuncaoF;
    }

    std::cout << "Processamento de comandos concluido." << std::endl;
}

int QR_search() {
    qr_code_data = "empty";
    distance_counter = 0;

    while (qr_code_data == "empty") {
        qr_code_data = get_qrcode();
    }

    std::cout << "\nQR Code lido: " << qr_code_data << std::endl;

    stop();
    usleep(1000000);

    std::vector<std::string> comandos = splitString(qr_code_data);
    processarComandos(comandos);

    return 0;
}

//--------------------------------------------------------------------------------//
// MAIN

int main() {
    if (gpioInitialise() < 0) {
        std::cerr << "Falha ao inicializar pigpio." << std::endl;
        return 1;
    }

    gpioSetPWMrange(EN1, 1000);
    gpioSetPWMrange(EN2, 1000);

    gpioSetMode(pinencoder_esq, PI_INPUT);
    gpioSetMode(pinencoder_dir, PI_INPUT);
    gpioSetMode(IN1, PI_OUTPUT);
    gpioSetMode(IN2, PI_OUTPUT);
    gpioSetMode(IN3, PI_OUTPUT);
    gpioSetMode(IN4, PI_OUTPUT);
    gpioSetMode(EN1, PI_OUTPUT);
    gpioSetMode(EN2, PI_OUTPUT);

    gpioSetPullUpDown(pinencoder_esq, PI_PUD_UP);
    gpioSetPullUpDown(pinencoder_dir, PI_PUD_UP);

    gpioSetAlertFunc(pinencoder_esq, callback_esq);
    gpioSetAlertFunc(pinencoder_dir, callback_dir);

    usleep(500000);

    while (1) {
        stop();
        QR_search();
        get_qrcode(); // Leitura do QR de confirmação de chegada
    }

    gpioTerminate();
    return 0;
}
