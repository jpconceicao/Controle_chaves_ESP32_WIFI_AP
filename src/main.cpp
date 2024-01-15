#include <Arduino.h>

#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>

#include <EEPROM.h>

#include <iostream>
#include <string.h>

#include <ESP32Servo.h>

#define TEMPO_ESPERA 50

/* Define as informações do AP (Access Point) */
const char *apSSID = "ESP32-AP";
const char *apPassword = "123456789";
const int serverPort = 80;

AsyncWebServer server(serverPort);
DNSServer dns;
/* ------------------------------------------ */

/* Variáveis e funções relacionadas à EEPROM */
const int eeprom_Address_master = 0;
const int eeprom_Address_chave1 = 10;
const int eeprom_Address_chave2 = 20;
const int eeprom_Address_chave3 = 30;
const int eeprom_Address_chave4 = 40;
const int eeprom_Address_check  = 100; // Checagem se ESP32 está configurada

void set_senha_master();
void set_senha_padrao_carro_1();
void set_senha_padrao_carro_2();
void set_senha_padrao_carro_3();
void set_senha_padrao_carro_4();

void resetar_EEPROM();
void ler_eeprom();

void salvar_senha_EEPROM(int numero_chave, int n_carro, String senha);
String get_string(int endereco_inicial);
char get_char(int endereco_inicial);
int definir_endereco_inicial(int numero_carro);
/* ---------------------------------------- */


/* Variáveis e funções referentes ao servo motor */
Servo myservo; // create servo object to control a servo

int servoPin1 = 26; // GPIO pin used to connect the servo control (digital out)
int servoPin2 = 25; // GPIO pin used to connect the servo control (digital out)
int servoPin3 = 33; // GPIO pin used to connect the servo control (digital out)
int servoPin4 = 32; // GPIO pin used to connect the servo control (digital out)

int ADC_Max = 4096; // This is the default ADC max value on the ESP32 (12 bit ADC width);
                    // this width can be set (in low-level oode) from 9-12 bits, for a
                    // a range of max values of 512-4096

int val; // variable to read the value from the analog pin

void liberar_chave(int carro);
/* ----------------------------------------------*/


void setup() {
  Serial.begin(115200);
  // Teste de saída de servo motor
  pinMode(servoPin1, OUTPUT);
  pinMode(servoPin2, OUTPUT);
  pinMode(servoPin3, OUTPUT);
  pinMode(servoPin4, OUTPUT);
  //------------------------------

  // Inicializa o Access Point
  WiFi.softAP(apSSID, apPassword);

  // Configuração do servidor DNS
  dns.start(53, "*", WiFi.softAPIP());

  // Configuração do servidor web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<html><body><form action='/check' method='POST'><label for='carros'>Selecione um carro:</label><select id='carros' name='carros'><option value='1'>1</option><option value='2'>2</option><option value='3'>3</option><option value='4'>4</option></select><label for='password'>Senha</label><input type='password' name='password'><input type='submit' value='Liberar'></form></body></html>");
  });
  
  // Envio de formulário para checagem de senha do usuário
  server.on("/check", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("password", true))
    { 
      int carro = std::stoi(request->getParam("carros", true)->value().c_str());
      
      String password = request->getParam("password", true)->value();

      int endereco_inicial = definir_endereco_inicial(carro);
      String senha_salva = get_string(endereco_inicial);

      if(password.equals(senha_salva))
      {
        // Entra a função que irá girar o motor específico do carro selecionado
        liberar_chave(carro);
        request->send(200, "text/plain", "Senha correta! Acesso permitido.");
      } 
      else 
      {
        // Informar que a senha está incorreta e permitir retornar pra página inicial
        // Talvez o formato precise ser text/html
        request->send(401, "text/plain", "Senha incorreta! Acesso negado.");
      }
    } 
    else 
    {
      request->send(400, "text/plain", "Parâmetro 'password' não encontrado.");
    }
  });

  // Página de alteração de senhas do admin
  server.on("/admin/Key_admin", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<html><body><form action='/admin/salvar_senha' method='POST'><label for='carros'>Selecione um carro:</label><select id='carros' name='carros'><option value='1'>1</option><option value='2'>2</option><option value='3'>3</option><option value='4'>4</option></select><label for='password'>Nova Senha</label><input type='password' name='password' maxlength='9' required><input type='submit' value='Salvar'></form></body></html>");
  });

  server.on("/admin/Key_admin_reset", HTTP_GET, [](AsyncWebServerRequest *request){
    resetar_EEPROM();
    Serial.println("Resetadas as senhas da ESP32 via comando web");
    request->send(200, "text/html", "<html><body><p>Realizado o RESET nas senhas da ESP32</p></body></html>");
  });

  // Envio de formulário para alteração de senhas
  server.on("/admin/salvar_senha", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("password", true))
    {
      int carro = std::stoi(request->getParam("carros", true)->value().c_str());
      
      String password = request->getParam("password", true)->value();

      int endereco_inicial = definir_endereco_inicial(carro);
      
      salvar_senha_EEPROM(endereco_inicial, carro, password);

      request->send(200, "text/plain", "Senha alterada com sucesso!");
    }
  });


  // Inicia o servidor web
  server.begin();

  Serial.println("Iniciando EEPROM!");

  // Iniciando a EEPROM
  EEPROM.begin(512);

  // Checa se a ESP já foi configurada
  char c = get_char(eeprom_Address_check);
  Serial.println(c);
  if(c != 's')
  {
    Serial.println("ESP32 sendo configurada pela primeira vez");
    resetar_EEPROM();
    EEPROM.write(eeprom_Address_check, static_cast<uint8_t>('s'));
    EEPROM.commit();
  }
  else
  {
    Serial.println("ESP32 já configurada");
  }

  Serial.println("Setup finalizado!");
}

void loop() {
  // Permite que o WiFiManager faça a configuração do Wi-Fi
  dns.processNextRequest();
}

void set_senha_master()
{
  // Converte a string em bytes
  String data = "#Diabinho";

  byte bytes[data.length() + 1];
  data.getBytes(bytes, data.length() + 1);

  EEPROM.write(0, data.length());

  // Escreve os bytes da string na EEPROM
  for (int i = 0; i < data.length(); i++)
  {
    EEPROM.write(i + 1, bytes[i]);
  }

  EEPROM.commit();
  delay(TEMPO_ESPERA);
  Serial.println("Resetada a senha padrão do Master");
}

void set_senha_padrao_carro_1()
{
  // Converte a string em bytes
  String data = "#Carro1";

  byte bytes[data.length() + 1];
  data.getBytes(bytes, data.length() + 1);

  EEPROM.write(eeprom_Address_chave1, data.length());

  // Escreve os bytes da string na EEPROM
  for (int i = 0; i < data.length(); i++)
  {
    EEPROM.write(eeprom_Address_chave1 + i + 1, bytes[i]);
  }

  EEPROM.commit();
  delay(TEMPO_ESPERA);
  Serial.println("Resetada a senha padrão do carro 1");
}

void set_senha_padrao_carro_2()
{
  // Converte a string em bytes
  String data = "#Carro2";

  byte bytes[data.length() + 1];
  data.getBytes(bytes, data.length() + 1);

  EEPROM.write(eeprom_Address_chave2, data.length());

  // Escreve os bytes da string na EEPROM
  for (int i = 0; i < data.length(); i++)
  {
    EEPROM.write(eeprom_Address_chave2 + i + 1, bytes[i]);
  }

  EEPROM.commit();
  delay(TEMPO_ESPERA);
  Serial.println("Resetada a senha padrão do carro 2");
}

void set_senha_padrao_carro_3()
{
  // Converte a string em bytes
  String data = "#Carro3";

  byte bytes[data.length() + 1];
  data.getBytes(bytes, data.length() + 1);

  EEPROM.write(eeprom_Address_chave3, data.length());

  // Escreve os bytes da string na EEPROM
  for (int i = 0; i < data.length(); i++)
  {
    EEPROM.write(eeprom_Address_chave3 + i + 1, bytes[i]);
  }

  EEPROM.commit();
  delay(TEMPO_ESPERA);
  Serial.println("Resetada a senha padrão do carro 3");
}

void set_senha_padrao_carro_4()
{
  // Converte a string em bytes
  String data = "#Carro4";

  byte bytes[data.length() + 1];
  data.getBytes(bytes, data.length() + 1);

  EEPROM.write(eeprom_Address_chave4, data.length());

  // Escreve os bytes da string na EEPROM
  for (int i = 0; i < data.length(); i++)
  {
    EEPROM.write(eeprom_Address_chave4 + i + 1, bytes[i]);
  }

  EEPROM.commit();

  delay(TEMPO_ESPERA);
  Serial.println("Resetada a senha padrão do carro 4");
}

void salvar_senha_EEPROM(int endereco_inicial, int n_carro, String senha)
{
  Serial.println("Entrou em salvar senha");

  byte bytes[senha.length() + 1];
  senha.getBytes(bytes, senha.length() + 1);

  EEPROM.write(endereco_inicial, senha.length());

  // Escreve os bytes da string na EEPROM
  for (int i = 0; i < senha.length(); i++)
  {
    EEPROM.write(endereco_inicial + i + 1, bytes[i]);
  }

  EEPROM.commit();

  delay(TEMPO_ESPERA);
  
  Serial.print("Finalizada a inserção da nova senha p/ carro ");
  Serial.println(n_carro);  
}

void resetar_EEPROM()
{
  set_senha_master();
  set_senha_padrao_carro_1();
  set_senha_padrao_carro_2();
  set_senha_padrao_carro_3();
  set_senha_padrao_carro_4();
}

String get_string(int endereco_inicial)
{
  String string = "";

  int tamanho = EEPROM.read(endereco_inicial);
  for (int i = 0; i < tamanho; i++)
  {
    char c = (char)EEPROM.readByte(endereco_inicial + i + 1);
    string += c;
  }

  return string;
}

char get_char(int endereco_inicial)
{
  unsigned char c;

  c = (char)EEPROM.readByte(endereco_inicial);
  Serial.println(c);

  return c;
}

void ler_eeprom()
{
  Serial.println("Valores armazenados na EEPROM:");

  for (int address = 0; address < EEPROM.length(); address++) {
    // Lê um byte da EEPROM na posição 'address'
    uint8_t value = EEPROM.read(address);

    // Mostra o valor na Serial
    Serial.print("Endereço ");
    Serial.print(address);
    Serial.print(": ");
    Serial.println(value);
  }

  Serial.println("Leitura da EEPROM concluída.");
}

int definir_endereco_inicial(int numero_carro)
{
  int endereco_inicial;

  switch (numero_carro)
  {
    
  case 1/* constant-expression */:
    /* code */
    endereco_inicial = eeprom_Address_chave1;
    break;
  
  case 2/* constant-expression */:
    /* code */
    endereco_inicial = eeprom_Address_chave2;
    break;

  case 3/* constant-expression */:
    /* code */
    endereco_inicial = eeprom_Address_chave3;
    break;

  case 4/* constant-expression */:
    /* code */
    endereco_inicial = eeprom_Address_chave4;
    break;

  default:
    break;
  }

  return endereco_inicial;
}

void liberar_chave(int carro)
{
  Serial.print("Carro: ");
  Serial.println(carro);

  switch (carro)
  {
  case 1:
    Serial.println("Entro no case de liberar carro 1");
    digitalWrite(servoPin1, HIGH);
    delay(2000);
    digitalWrite(servoPin1, LOW);
    break;
  
  case 2:
    Serial.println("Entro no case de liberar carro 2");
    digitalWrite(servoPin2, HIGH);
    delay(2000);
    digitalWrite(servoPin2, LOW);
    break;
  
  case 3:
    Serial.println("Entro no case de liberar carro 3");
    digitalWrite(servoPin3, HIGH);
    delay(2000);
    digitalWrite(servoPin3, LOW);
    break;
  
  case 4:
    Serial.println("Entro no case de liberar carro 4");
    digitalWrite(servoPin4, HIGH);
    delay(2000);
    digitalWrite(servoPin4, LOW);
    break;
  
  default:
    Serial.println("Valor de carro inválido");
    break;
  }
}
