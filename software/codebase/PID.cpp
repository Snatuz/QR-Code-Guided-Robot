#include "motors.hpp"
#include <iostream>
#include <pigpio.h>
#include <unistd.h>
#include <cstdint>
#define pinencoder_esq 7
#define pinencoder_dir 8
#define IN1 2
#define IN2 3
#define IN3 17
#define IN4 27
#define EN1 4
#define EN2 22

void stop() {
  
  gpioWrite(EN1, 0);
  gpioWrite(EN2, 0);
  gpioPWM(IN1, 0);
  gpioPWM(IN2, 0);
  gpioPWM(IN3, 0);
  gpioPWM(IN4, 0);
  gpioSetMode(IN1, PI_OUTPUT);
  gpioSetMode(IN2, PI_OUTPUT);
  gpioSetMode(IN3, PI_OUTPUT);
  gpioSetMode(IN4, PI_OUTPUT);  
  
  }


// g++ -pthread nome_arquivo.cpp -o executavel_nome -lpigpio -lrt

uint32_t tick_anterior = 0;
uint32_t intervalo = 0;


void callback_esq(int gpio, int level, uint32_t tick) {
  left_motor->encoder_counter++;
}

void callback_dir(int gpio, int level, uint32_t tick) {
  right_motor->encoder_counter++;
}

void PID() {


  float dt = intervalo / 1000000.0f;

  int delta_esq = left_motor->encoder_counter - left_motor->previous_encoder_counter;
  int delta_dir = right_motor->encoder_counter - right_motor->previous_encoder_counter;

  left_motor->velocity = delta_esq * (20.42/20) * 1000000/intervalo;  // em cm/s
  right_motor->velocity = delta_dir * (20.42/20) * 1000000/intervalo;  // em cm/s

  left_motor->error = setpoint - left_motor->velocity;
  right_motor->error = setpoint - right_motor->velocity;


  left_motor->derivative = (left_motor->error - left_motor->previous_error) / dt; //caucula o derivativo a ser corrigido.
  right_motor->derivative = (right_motor->error - right_motor->previous_error) / dt;

  left_motor->integral += left_motor->error * dt;
  right_motor->integral += right_motor->error * dt;

//calcula a correcao total
  left_motor->correction = (KP * left_motor->error) + (KI * left_motor->integral) + (KD * left_motor->derivative);
  right_motor->correction = (KP * right_motor->error) + (KI * right_motor->integral) + (KD * right_motor->derivative);
  

  left_motor->PWM = left_motor->previous_PWM + left_motor->correction;
  right_motor->PWM = right_motor->previous_PWM + right_motor->correction;
  
//verificar limites do PWM

	if(left_motor->PWM > 1000)
    left_motor->PWM = 1000;
  
  if(left_motor->PWM < 0)
    left_motor->PWM = 0;
  
  if(right_motor->PWM > 1000)
    right_motor->PWM = 1000;
    
  if(right_motor->PWM < 0)
    right_motor->PWM = 0;


  gpioPWM(IN1, left_motor->PWM);
  gpioPWM(IN3, right_motor->PWM); // isso considera que PID() foi chamada enquanto o robo anda para frente. 

  left_motor->previous_PWM = left_motor->PWM;
  right_motor->previous_PWM = right_motor->PWM;

  left_motor->previous_encoder_counter = left_motor->encoder_counter;
  right_motor->previous_encoder_counter = right_motor->encoder_counter;

  left_motor->previous_error = left_motor->error;
  right_motor->previous_error = right_motor->error;


  std::cout << "		PWM ESQUERDO:  " << left_motor->PWM
            << "		PWM DIREITO: " << right_motor->PWM 
            << "\n Velocidade DIREITO: " << right_motor->velocity
            << "		Velocidade ESQUERDO: " << left_motor->velocity << "\n";
}

int main() {

KP = 3;
KI = 0.1;
KD = 0.03;


   if (gpioInitialise() < 0) {
    std::cerr << "\nErro ao inicializar a pigpio!" << std::endl;
    return 1;
  }

  gpioSetPWMrange(IN1, 1000);
  gpioSetPWMrange(IN3, 1000);

  gpioSetMode(pinencoder_esq, PI_INPUT);
  gpioSetMode(pinencoder_dir, PI_INPUT);
  gpioSetMode(IN1, PI_OUTPUT);
  gpioSetMode(IN2, PI_OUTPUT);
  gpioSetMode(IN3, PI_OUTPUT);
  gpioSetMode(IN4, PI_OUTPUT);
  gpioSetMode(EN1, PI_OUTPUT);
  gpioSetMode(EN2, PI_OUTPUT);


  int ciclos = 0;
  int time = 0;

  std::cout << "\nVelocidade inicial (em cm/s)";  	
  std::cin >> setpoint;
  std::cout << "\nInsira KP: ";  
  std::cin >> KP;
  std::cout << "\nInsira o intervalo em micro-segundos: ";
  std::cin >> time;
  std::cout << "\n quantidade de tempo em segundos: ";
  std::cin >> ciclos;
  ciclos = (ciclos * 1000000) / time;


  usleep(2*10e6);


  if (gpioInitialise() < 0) {
    std::cerr << "\nErro ao inicializar a pigpio!" << std::endl;
    return 1;
  }

  gpioWrite(EN1, 1);
  gpioWrite(EN2, 1);

  gpioPWM(IN1, 300);
  gpioPWM(IN3, 300);
  gpioWrite(IN2, 0);
  gpioWrite(IN4, 0);

  gpioSetPullUpDown(pinencoder_esq, PI_PUD_UP);
  gpioSetPullUpDown(pinencoder_dir, PI_PUD_UP);

  gpioSetAlertFunc(pinencoder_esq, callback_esq);
  gpioSetAlertFunc(pinencoder_dir, callback_dir);

  usleep(100000); //tempo para estabilizar a leitura dos encoders


  for(int i = 0; i < ciclos; i++){ //loop principal

  
    std::cout << "\nVelocidade Esquerda: " << left_motor->velocity << "cm/s " 
              << "		Velocidade Direita: " << right_motor->velocity << "cm/s \n";
              
    intervalo = gpioTick() - tick_anterior;; 
    PID();
    tick_anterior = gpioTick();
    
    usleep(time);
  }

  std::cout << "\n\nPWMS FINAIS:    ESQUERDO: " << left_motor->PWM 
            << "    DIREITO: " << right_motor->PWM;
  
  stop();

  gpioTerminate();
  usleep(100000);
  return 0;
}
