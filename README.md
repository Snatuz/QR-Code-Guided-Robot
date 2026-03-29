# Documentação do Código — Robô Móvel Guiado por QR Code (GMR)

**Projeto:** TCC — Robô Móvel Guiado por QR Code  
**Instituição:** Instituto Federal de Minas Gerais — Campus Itabirito  
**Autores:** Alissanderson Felipe Barros Nazareth, Gabriel Pereira Clóvis, Miguel Henrique de Aguiar Santos  
**Linguagem:** C++17  
**Dependências:** pigpio, pthreads, OpenCV 4

---

## Visão Geral da Arquitetura

O sistema é dividido em **dois executáveis independentes** que se comunicam por meio de um arquivo de texto intermediário (`qrcode.txt`):

```
┌─────────────────────────────────────────────┐
│                  main (GMR)                 │
│  - Controle de motores via GPIO (pigpio)    │
│  - Lógica de navegação e processamento      │
│  - Chama qrcode_reader como subprocesso     │
└────────────────┬────────────────────────────┘
                 │ system("./qrcode_reader")
                 │ lê: qrcode.txt
                 ▼
┌─────────────────────────────────────────────┐
│             qrcode_reader (leitor)          │
│  - Captura frames da câmera via OpenCV      │
│  - Detecta e decodifica QR Codes            │
│  - Escreve resultado em qrcode.txt          │
└─────────────────────────────────────────────┘
```

---

## Arquivo: `qrcode_reader.cpp`

### Descrição Geral

Executável auxiliar responsável pela **captura e decodificação de QR Codes** através da câmera conectada ao Raspberry Pi. Ao detectar um código válido, salva o conteúdo em um arquivo de texto que serve como canal de comunicação com o processo principal.

### Compilação

```bash
g++ -std=c++17 -Wall -O2 qrcode_reader.cpp -o qrcode_reader \
$(pkg-config --cflags --libs opencv4)
```

### Dependências

| Biblioteca | Uso |
|---|---|
| `opencv2/core.hpp` | Tipos base do OpenCV (`Mat`, `Point`) |
| `opencv2/imgproc.hpp` | Processamento de imagem |
| `opencv2/objdetect.hpp` | `QRCodeDetector` |
| `opencv2/videoio.hpp` | Captura de vídeo (`VideoCapture`) |
| `fstream` | Leitura/escrita de arquivos |

### Variáveis Globais

```cpp
VideoCapture cam(0);
```

| Variável | Tipo | Descrição |
|---|---|---|
| `cam` | `VideoCapture` | Instância da câmera. O índice `0` refere-se à câmera padrão do sistema (câmera USB). Inicializada no escopo global para persistir entre chamadas. |

---

### Funções

#### `std::string get_qrcode()`

**Descrição:** Captura um único frame da câmera e tenta detectar e decodificar um QR Code presente na imagem.

**Parâmetros:** Nenhum.

**Retorno:**
- A string com o conteúdo do QR Code, caso detectado com sucesso.
- `"empty"` se o frame estiver vazio ou nenhum QR Code for encontrado.

**Fluxo interno:**
1. Instancia o `QRCodeDetector` do OpenCV.
2. Captura um frame com `cam >> frame`.
3. Verifica se o frame está vazio; retorna `"empty"` em caso positivo.
4. Executa `qr.detectAndDecode(frame, bbox)`, que retorna uma string vazia se nada for detectado.
5. Retorna o conteúdo ou `"empty"`.

```cpp
std::string get_qrcode() {
    QRCodeDetector qr;
    Mat frame;
    cam >> frame;
    if (frame.empty()) return "empty";
    std::vector<Point> bbox;
    std::string data = qr.detectAndDecode(frame, bbox);
    return data.empty() ? "empty" : data;
}
```

> **Nota:** `bbox` armazena os pontos do polígono delimitador do QR Code na imagem, mas não é utilizado após a detecção.

---

#### `int save_code(std::string code)`

**Descrição:** Salva o conteúdo decodificado do QR Code no arquivo `qrcode.txt`, que serve como pipe de comunicação com o processo `main`.

**Parâmetros:**

| Parâmetro | Tipo | Descrição |
|---|---|---|
| `code` | `std::string` | String com o conteúdo do QR Code a ser salvo. |

**Retorno:**
- `0` — arquivo aberto e escrito com sucesso.
- `1` — erro ao abrir o arquivo.

**Fluxo interno:**
1. Abre (ou cria) o arquivo `qrcode.txt` no modo de escrita, sobrescrevendo conteúdo anterior.
2. Escreve a string `code` no arquivo.
3. Fecha o arquivo e retorna `0`.

```cpp
int save_code(std::string code) {
    std::ofstream arquivo("qrcode.txt");
    if (!arquivo.is_open()) return 1;
    arquivo << code;
    arquivo.close();
    return 0;
}
```

> **Atenção:** O arquivo é sempre sobrescrito. Não há mecanismo de append.

---

#### `int main()`

**Descrição:** Ponto de entrada do executável `qrcode_reader`. Inicializa a câmera, executa o loop de detecção e encerra após a primeira leitura bem-sucedida.

**Fluxo:**
1. Verifica se a câmera foi aberta com sucesso (`cam.isOpened()`); encerra com código `1` em caso de falha.
2. Entra em loop infinito chamando `get_qrcode()`.
3. Ao obter um resultado diferente de `"empty"`, chama `save_code()` para persistir o dado e encerra o loop com `break`.
4. Libera a câmera com `cam.release()` e retorna `0`.

```
Início
  │
  ▼
Câmera aberta? ──Não──► Encerra (erro 1)
  │Sim
  ▼
┌─────────────────────┐
│  get_qrcode()       │◄──────┐
│  QR encontrado?     │       │ Não
│  Sim ──► save_code()│───────┘
└────────┬────────────┘
         │
         ▼
    cam.release()
    Retorna 0
```

---

## Arquivo: `main.cpp`

### Descrição Geral

Executável principal do GMR. Responsável por inicializar os pinos GPIO do Raspberry Pi, registrar os callbacks dos encoders, executar o loop de busca de QR Codes e controlar a movimentação do robô (avanço e rotação) com base nos comandos recebidos.

### Compilação

```bash
g++ -std=c++17 -Wall -O2 \
    main.cpp \
    -o main \
    -lpigpio -lpthread -lrt \
    $(pkg-config --cflags --libs opencv4)
```

### Dependências

| Biblioteca | Uso |
|---|---|
| `pigpio.h` | Controle dos pinos GPIO do Raspberry Pi |
| `unistd.h` | `usleep()` para delays |
| `fstream` | Leitura do arquivo `qrcode.txt` |
| `sstream` | Parsing de strings (`splitString`) |
| `vector` | Armazenamento de comandos |
| `cstdlib` | `system()` para subprocesso |

---

### Mapeamento de Pinos GPIO

| Macro | GPIO (BCM) | Função |
|---|---|---|
| `pinencoder_esq` | 7 | Sinal do encoder da roda esquerda |
| `pinencoder_dir` | 8 | Sinal do encoder da roda direita |
| `IN1` | 2 | Direção motor esquerdo (polo +) |
| `IN2` | 3 | Direção motor esquerdo (polo −) |
| `IN3` | 17 | Direção motor direito (polo +) |
| `IN4` | 27 | Direção motor direito (polo −) |
| `EN1` | 4 | Enable/PWM motor esquerdo |
| `EN2` | 22 | Enable/PWM motor direito |

> Os pinos `IN` controlam o sentido de rotação pelo driver L298N (Ponte H). Os pinos `EN` recebem o sinal PWM que controla a velocidade. A faixa de PWM configurada é de 0 a 1000.

---

### Variáveis Globais

| Variável | Tipo | Descrição |
|---|---|---|
| `distance_counter` | `int` | Contador global de ticks do encoder direito. Usado como medida de deslocamento e rotação. Resetado antes de cada movimentação. |
| `qr_code_data` | `std::string` | Armazena o conteúdo do QR Code lido na iteração atual. |
| `KP`, `KI`, `KD` | `float` | Constantes de controle PID (declaradas mas não utilizadas na versão atual — reservadas para implementação futura de controle de velocidade). |
| `setpoint` | `float` | Valor de referência para controle PID (não utilizado atualmente). |

---

### Struct `motor`

Representa o estado de um motor DC individualmente.

```cpp
struct motor {
    unsigned int PWM;                    // Valor PWM atual
    unsigned int previous_PWM = 0;      // Valor PWM anterior
    unsigned int velocity;               // Velocidade (não utilizada atualmente)
    unsigned int encoder_counter = 0;    // Contagem total de ticks do encoder
    unsigned int previous_encoder_counter = 0; // Contagem anterior (para delta)
    float error;                         // Erro PID atual
    float previous_error;                // Erro PID anterior
    float correction;                    // Correção calculada
    float derivative;                    // Componente derivativo
    float integral;                      // Componente integral
} typedef motor;
```

**Instâncias:**
- `motor l_motor` — motor esquerdo (acessado via ponteiro `left_motor`)
- `motor r_motor` — motor direito (acessado via ponteiro `right_motor`)

> Os campos de PID (`error`, `correction`, `derivative`, `integral`) estão declarados para uma implementação futura de controle de malha fechada que equalize a velocidade dos dois motores.

---

### Funções

#### `std::string get_qrcode()`

**Descrição:** Dispara o executável `qrcode_reader` como subprocesso e lê o resultado do arquivo `qrcode.txt`.

**Parâmetros:** Nenhum.

**Retorno:**
- Conteúdo do QR Code lido (string).
- `"empty"` se o arquivo contiver esse valor (QR não encontrado) ou se a leitura falhar.

**Fluxo:**
1. Chama `system("./qrcode_reader")`, que bloqueia até o processo filho terminar.
2. Abre `qrcode.txt` e lê a primeira linha com `std::getline`.
3. Se a linha for `"empty"`, retorna `"empty"`.
4. Caso contrário, retorna a string lida.

```cpp
std::string get_qrcode() {
    system("./qrcode_reader");
    std::ifstream arquivo("qrcode.txt");
    std::string buffer;
    std::getline(arquivo, buffer);
    if (buffer == "empty") return "empty";
    return buffer;
}
```

> **Importante:** A função é **bloqueante**. O processo principal fica suspenso enquanto o `qrcode_reader` executa. Isso significa que, durante a busca por QR Code, a chamada é síncrona e depende do tempo que a câmera leva para detectar o marcador.

---

#### `void callback_esq(int gpio, int level, uint32_t tick)`

**Descrição:** Callback de interrupção registrado para o encoder da roda **esquerda**. Chamado automaticamente pela biblioteca pigpio a cada mudança de nível lógico no pino `pinencoder_esq`.

**Parâmetros (pigpio):**

| Parâmetro | Tipo | Descrição |
|---|---|---|
| `gpio` | `int` | Número do pino que gerou o evento |
| `level` | `int` | Novo nível lógico (0 ou 1) |
| `tick` | `uint32_t` | Timestamp em microssegundos |

**Comportamento:** Incrementa `left_motor->encoder_counter` a cada pulso.

> Na versão atual, o contador do motor esquerdo não é usado diretamente no controle de movimento. Serve como base para futura implementação de PID.

---

#### `void callback_dir(int gpio, int level, uint32_t tick)`

**Descrição:** Callback de interrupção para o encoder da roda **direita**. Chamado a cada mudança de nível lógico no pino `pinencoder_dir`.

**Comportamento:** Incrementa tanto `right_motor->encoder_counter` quanto a variável global `distance_counter`. Este segundo contador é o utilizado como referência de deslocamento e rotação em todas as funções de movimento.

> O encoder direito é o **encoder de referência** do sistema. Todo controle de distância e ângulo é baseado em `distance_counter`.

---

#### `void stop()`

**Descrição:** Para imediatamente os dois motores, zerando todos os sinais de controle do driver L298N.

**Parâmetros:** Nenhum.

**Comportamento:**
- Desativa os pinos `EN1` e `EN2` (sem tensão = sem corrente nos motores).
- Zera o PWM em todos os pinos `IN`, garantindo frenagem ativa.

```cpp
void stop() {
    gpioWrite(EN1, 0);
    gpioWrite(EN2, 0);
    gpioPWM(IN1, 0);
    gpioPWM(IN2, 0);
    gpioPWM(IN3, 0);
    gpioPWM(IN4, 0);
}
```

> Deve ser chamada antes de qualquer mudança de direção para evitar corrente de curto-circuito no driver.

---

#### `void forward(int distance)`

**Descrição:** Move o robô para **frente** por uma distância aproximada em centímetros.

**Parâmetros:**

| Parâmetro | Tipo | Unidade | Descrição |
|---|---|---|---|
| `distance` | `int` | cm (≈ ticks) | Distância a percorrer. 1 tick do encoder ≈ 1 cm. |

**Fluxo:**
1. Chama `stop()` para garantir estado inicial seguro.
2. Configura `IN1` e `IN3` em nível alto (ambos motores para frente).
3. Aplica PWM: `EN1 = 900`, `EN2 = 920` (valores calibrados empiricamente para andar reto).
4. Reseta `distance_counter = 0`.
5. Aguarda em loop até `distance_counter >= distance` com polling a cada 50 ms.
6. Chama `stop()`.

```
Convenção de sentido (Ponte H L298N):
  IN1=1, IN2=0 → motor esquerdo para frente
  IN3=1, IN4=0 → motor direito para frente
  IN1=0, IN2=1 → motor esquerdo para trás
  IN3=0, IN4=1 → motor direito para trás
```

> **Calibração:** Os valores `900` e `920` para `EN1`/`EN2` são definidos empiricamente para compensar a diferença mecânica entre os dois motores e fazer o robô andar em linha reta.

---

#### `void spin_start()`

**Descrição:** Inicia o movimento de rotação aplicando PWM ao motor direito (`EN2`). Usada internamente pelas funções `spin()` e `spin_by_ticks()`.

**Comportamento:** Aplica `gpioPWM(EN2, 900)`. O código original para o motor esquerdo está comentado — na versão atual, a rotação é feita apenas com o motor direito (rotação sobre a roda esquerda estacionária).

> **Atenção:** Esta função não define a direção (sentido de rotação). A direção é configurada pelos pinos `IN` antes de `spin_start()` ser chamada.

---

#### `int spin(int angle)`

**Descrição:** Gira o robô no próprio eixo por um ângulo aproximado em graus.

**Parâmetros:**

| Parâmetro | Tipo | Unidade | Descrição |
|---|---|---|---|
| `angle` | `int` | graus | Ângulo de rotação. Positivo = direita, Negativo = esquerda. |

**Valores especiais:**

| Valor | Comportamento |
|---|---|
| `999` | Inicia rotação para a direita sem parar (indeterminado) |
| `-999` | Inicia rotação para a esquerda sem parar (indeterminado) |
| `> 360` (exceto 999) | Comando inválido, ignorado com aviso |
| `< -360` (exceto -999) | Comando inválido, ignorado com aviso |

**Conversão ângulo → ticks:** O loop aguarda `distance_counter < (angle * 0.3)` ticks. Isso resulta em aproximadamente `1 grau ≈ 0.3 ticks` (calibrado empiricamente).

**Fluxo:**
1. `stop()`.
2. Define direção nos pinos `IN3`/`IN4` (apenas motor direito é ativado).
3. Valida o ângulo.
4. Chama `spin_start()`.
5. Aguarda `distance_counter < (angle * 0.3)` em polling de 50 ms.
6. `stop()`.

> **Nota:** Na função `processarComandos`, esta função **não** é chamada diretamente — a versão atual usa `spin_by_ticks` para maior precisão. O `spin` por graus pode ser usado para testes ou substituição futura.

---

#### `int spin_by_ticks(int ticks)`

**Descrição:** Gira o robô no próprio eixo por uma quantidade exata de ticks do encoder, oferecendo maior precisão que `spin()`.

**Parâmetros:**

| Parâmetro | Tipo | Unidade | Descrição |
|---|---|---|---|
| `ticks` | `int` | ticks | Número de pulsos do encoder. Positivo = direita, Negativo = esquerda. Referência: 29 ticks ≈ 90 graus. |

**Diferença em relação a `spin()`:** Enquanto `spin()` converte graus para ticks com fator 0.3, `spin_by_ticks()` recebe diretamente a contagem de pulsos, eliminando a imprecisão da conversão.

**Fluxo:**
1. `stop()`.
2. Define direção: ticks positivo → direita (IN1=1, IN2=0, IN3=0, IN4=1); ticks negativo → esquerda (IN1=0, IN2=1, IN3=1, IN4=0).
3. Converte `ticks` para positivo.
4. Chama `spin_start()`.
5. Aguarda `distance_counter < ticks` com polling de 100 ms.
6. `stop()`.

> **Calibração empírica:** 29 ticks ≈ 90°. Isso foi determinado por testes realizados pelos autores, variando conforme superfície e carga da bateria.

---

#### `std::vector<std::string> splitString(const std::string &str, char delimiter = ',')`

**Descrição:** Divide uma string em tokens com base em um delimitador, retornando um vetor de strings. Usada para parsear a string de comandos lida do QR Code.

**Parâmetros:**

| Parâmetro | Tipo | Padrão | Descrição |
|---|---|---|---|
| `str` | `const std::string&` | — | String de entrada a ser dividida. |
| `delimiter` | `char` | `','` | Caractere separador. |

**Retorno:** `std::vector<std::string>` com os tokens. Tokens vazios são ignorados.

**Exemplo:**
```
Entrada:  "30,29,40,E"
Saída:    ["30", "29", "40", "E"]
```

---

#### `void processarComandos(const std::vector<std::string> &comandos)`

**Descrição:** Interpreta e executa sequencialmente a lista de comandos extraída do QR Code, alternando entre movimento para frente e rotação.

**Parâmetros:**

| Parâmetro | Tipo | Descrição |
|---|---|---|
| `comandos` | `const std::vector<std::string>&` | Vetor de strings gerado por `splitString()`. |

**Protocolo de comandos:**

A string do QR Code segue o padrão `F,S,F,S,...` onde:
- Posições ímpares (1ª, 3ª, 5ª...): parâmetro para `forward()` — distância em **centímetros**.
- Posições pares (2ª, 4ª, 6ª...): parâmetro para `spin_by_ticks()` — rotação em **ticks**.
- Caractere `"E"` em qualquer posição: encerra o programa (`exit(0)`).

**Fluxo:**
1. Inicializa `chamarFuncaoF = true`.
2. Para cada comando no vetor:
   - Se `== "E"`: imprime mensagem e chama `exit(0)`.
   - Converte string para inteiro com `std::stoi()`. Se inválido, imprime erro e passa para o próximo.
   - Se `chamarFuncaoF == true`: chama `forward(valorNumerico)`.
   - Se `chamarFuncaoF == false`: chama `spin_by_ticks(valorNumerico)`.
   - Inverte `chamarFuncaoF`.
   - Aguarda 500 ms entre cada comando (`usleep(500000)`).

**Exemplo de execução:**
```
QR Code: "30,29,40,E"

Passo 1: forward(30)     → avança 30 cm
Pausa:   500 ms
Passo 2: spin_by_ticks(29) → gira ~90° à direita
Pausa:   500 ms
Passo 3: forward(40)     → avança 40 cm
Pausa:   500 ms
Passo 4: "E" encontrado  → exit(0)
```

---

#### `int QR_search()`

**Descrição:** Função de busca principal. Aguarda a detecção de um QR Code válido e, após a leitura, processa os comandos de movimentação contidos no código.

**Parâmetros:** Nenhum.

**Retorno:** `0` em caso de sucesso (mas na prática, o controle já foi transferido para `processarComandos` antes do retorno).

**Fluxo detalhado:**

```
1. Reseta qr_code_data = "empty"
2. Reseta distance_counter = 0
3. Loop: enquanto distance_counter < 171 E qr_code_data == "empty":
   └─ chama get_qrcode() e armazena resultado
4. Se distance_counter >= 171:
   └─ stop(), imprime aviso, exit(0)  ← QR não encontrado após ~171 ticks
5. Imprime o QR Code lido
6. stop()
7. Aguarda 1 segundo (usleep(1000000))
8. splitString(qr_code_data) → vetor de comandos
9. processarComandos(comandos)
10. return 0
```

**Limite de 171 ticks:** O valor `171` representa a quantidade máxima de ticks que o robô aguarda enquanto tenta ler um QR Code. Se o marcador não for encontrado dentro desse limite (equivalente a aproximadamente uma volta completa), o programa é encerrado.

> **Observação:** O trecho `spin(-999)` que giraria o robô durante a busca está **comentado** na versão atual. Isso significa que na versão do arquivo `main.cpp` enviado, o robô fica **parado** enquanto tenta ler o QR, diferentemente da versão do TCC onde ele girava durante a busca.

---

#### `int main()`

**Descrição:** Ponto de entrada do programa principal. Inicializa a GPIO, configura todos os pinos e callbacks, e executa o loop principal de operação.

**Fluxo de inicialização:**

```
1. gpioInitialise()            ← inicializa pigpio
2. gpioSetPWMrange(EN1, 1000)  ← resolução PWM: 0–1000
   gpioSetPWMrange(EN2, 1000)
3. Configura modos dos pinos:
   - encoders: PI_INPUT
   - IN1..IN4, EN1, EN2: PI_OUTPUT
4. Pull-up nos pinos de encoder (PI_PUD_UP)
5. Registra callbacks:
   - pinencoder_esq → callback_esq
   - pinencoder_dir → callback_dir
6. usleep(500000)  ← aguarda 500ms para estabilização
```

**Loop principal:**

```cpp
while (1) {
    stop();
    QR_search();
    get_qrcode();
}
```

O loop chama `stop()` por segurança, em seguida `QR_search()` (que internamente já processa os comandos). A chamada `get_qrcode()` após `QR_search()` serve como leitura do QR de confirmação de chegada ao destino (conforme descrito no TCC).

> **Nota:** `gpioTerminate()` está declarado mas **nunca é alcançado** devido ao `while(1)` infinito. Em uma implementação mais robusta, seria chamado em um handler de sinal (ex: `SIGINT`).

---

## Fluxo Completo do Sistema

```
main() inicia
│
├─ Inicializa GPIO
├─ Registra callbacks dos encoders
│
└─ Loop infinito:
    │
    ├─ stop()
    │
    └─ QR_search():
        │
        ├─ Aguarda QR Code:
        │   └─ get_qrcode():
        │       ├─ system("./qrcode_reader")
        │       │   └─ [subprocesso]:
        │       │       ├─ abre câmera
        │       │       ├─ captura frames
        │       │       ├─ detecta QR
        │       │       └─ salva em qrcode.txt
        │       └─ lê qrcode.txt → retorna string
        │
        ├─ [QR lido com sucesso]
        │
        └─ processarComandos():
            ├─ forward(N)       → avança N cm
            ├─ spin_by_ticks(T) → gira T ticks
            ├─ ... (alternado)
            └─ "E" → exit(0) ou fim da lista → volta ao loop
```

---

## Protocolo de Comandos (QR Code)

O conteúdo do QR Code é uma string de valores inteiros separados por vírgula, seguindo o padrão:

```
<dist_cm>,<ticks_rot>,<dist_cm>,<ticks_rot>,...[,E]
```

| Campo | Função chamada | Unidade | Observação |
|---|---|---|---|
| Posição ímpar | `forward()` | centímetros | Sempre positivo |
| Posição par | `spin_by_ticks()` | ticks | Positivo = direita, Negativo = esquerda |
| `E` | `exit(0)` | — | Encerra o programa |

**Exemplo:**
```
30,29,40,-29,20,E
```
- `forward(30)` → anda 30 cm
- `spin_by_ticks(29)` → gira ~90° à direita
- `forward(40)` → anda 40 cm
- `spin_by_ticks(-29)` → gira ~90° à esquerda
- `forward(20)` → anda 20 cm
- `E` → encerra

---

## Referências de Calibração

| Medida | Valor | Método |
|---|---|---|
| 1 tick do encoder ≈ | 1 cm de deslocamento | Calculado: diâmetro ~20 cm, 40 ticks/volta → π×20/40 ≈ 1,57 cm/tick (aproximado para 1 cm/tick no código) |
| 29 ticks ≈ | 90° de rotação | Determinado empiricamente |
| 1 tick em rotação ≈ | 3° | Calculado a partir dos testes (360°/29 ticks ≈ 12,4°, mas o código usa 3° como referência para `spin()`) |
| PWM para frente reto | EN1=900, EN2=920 | Calibrado empiricamente para compensar diferença entre motores |
| PWM para rotação | EN2=900 | Calibrado empiricamente |
| Limite de busca de QR | 171 ticks | Equivale a aproximadamente uma volta completa do robô |

---

## Limitações Conhecidas

- **Subprocesso bloqueante:** O robô fica parado enquanto o `qrcode_reader` busca o QR Code (na versão atual sem o `spin(-999)` ativo).
- **Sem malha fechada:** Os campos de PID na struct `motor` não são utilizados. O controle é de malha aberta, suscetível a desvios.
- **Encoder único de referência:** Apenas o encoder direito (`distance_counter`) controla o movimento. O encoder esquerdo conta mas não influencia o controle.
- **`gpioTerminate()` inalcançável:** O loop infinito em `main()` impede a finalização limpa da GPIO. Recomenda-se implementar tratamento de `SIGINT`.
- **Arquivo como IPC:** A comunicação entre `main` e `qrcode_reader` via arquivo em disco é simples mas não atômica — em condições adversas (disco cheio, permissão, etc.) pode falhar silenciosamente.
