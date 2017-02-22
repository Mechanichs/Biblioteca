#include <EEPROM.h>
#include <LiquidCrystal.h>
#define LIMITE         4          // Define o limite de erro do sinal do potenciometro
#define DEBUG          1          // Ativar(1) ou desativar(0) a comunicação com o serial.
#define ZERAR          1          // (1) zero o EEPROM (0) mantem o EEPROM com leituras anteriores
#define DELAY          500        // Define o tempo para o delay de debug em milissegundos
#define NUM_SENSORES   4          // Numero de sensores usados
#define NUM_INTERACOES 700        // Numero de interções no filtro
#define OVERFLOW       4000000000 // Over flow para o unsigned long
#define OCIO           5          // Tempo minimo entre uma ativação e outra
#define LUZ            6          // Sinalizador luminoso ligado à porta digital do arduino 
#define SOM            7          // Sirene ligada à porta digital do arduino
#define COOLER         10         // Define a porta do cooler
#define TEMPO_COOLER   3          // Tempo que o cooler permanecerá ligado 
#define NIVEL_AVISO    180        // Determina nível de ruído/pulsos para ativar o sinalizador luminoso.
#define NIVEL_LIMITE   180        // Determina nível de ruído/pulsos para ativar a sirene.
#define TEMPO_LUZ      3          // Define o tempo de duração em que o sinalizador permanecerá ativo.
#define TEMPO_SOM      3          // Define o tempo de duração em que a sirene permanecerá ativo.
#define REP_SOM        2          // Quantidade de vezes que a sirene irá disparar 
#define REP_LUZ        2          // Quantidade de vezes que o sinalizador luminoso irá disparar
#define ON             1
#define OFF            0
#define TOLERANCIA     1          // EM SEGUNDOS PELO AMOR DE DEUS!

int           sensores[NUM_SENSORES] = {A2, A3, A5, A6}; // Sensores ligados às portas analógicas
int           verificadores[NUM_SENSORES] = {A7, A4, A1, A0};  // Resposansaveis por gravar saida do potenciometro
int           limite_POT[NUM_SENSORES] = {554, 554, 554, 554};  // Variável responsável por definir o limiar do potenciometro medido analogicamente em relação à sensibilidade do sensor
bool          flag_calibracao[NUM_SENSORES];
int           nivel                  = 0;    // Variável responsável pelo nível de ruído
int           endereco               = 0;    // Endereço de memória que vai armazenar quantidade de vezes que a sirene acionou
int           Q_Acionamento          = 0;    // Variável responsável por armazenar quantidade de vezes que a sirene acionou  
unsigned long t0                     = 0;    // Variável responsável pelo tempo inicial
unsigned long tc                     = 0;    // Variável responsável pelo tempo calculado  
int           deveAlertar            = 1;    // Variável atua como um binário (true/false)
unsigned int  expoente               = 1;
bool          ligarcooler            = false;
int botoes[4];
bool estadoBotao;
void(* reset) (void)= 0;
bool verificador;
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);


//Função que define os componentes sinalizador,sirene e sensores como entrada ou saída

void setup() 
{
  Serial.begin(9600);
  pinMode(LUZ,  OUTPUT); //@param[OUT]
  pinMode(SOM, OUTPUT); //@param[OUT]

  for(int i = 0; i < NUM_SENSORES; i++) 
  {
    pinMode(sensores[i], INPUT); //@param[IN]
  }
  if(ZERAR)
    EEPROM.write(endereco, 0);    
  else
    EEPROM.write(endereco, EEPROM.read(endereco));
  
  for(int i = 0; i < NUM_SENSORES; i++)
    flag_calibracao[i] = false;
  lcd.begin(20,4);
  for(int i = 0; i < 4; i++)
    pinMode(botoes[i], INPUT);
}

void loop() 
{
  for(int i=0; i < NUM_SENSORES; i++)
    flag_calibracao[i] = false;
  ajusteSensibilidade();  
  
  nivel = ouvirNivel();
 // lerTempoCooler();
  if(nivel < NIVEL_AVISO) 
  {
    if(t0 < OVERFLOW)
    {
      t0 = (millis()/1000); // Transforma para segundo para armazenar mais valores na variável
    }
    else 
    reset();
  }

  deveAlertar = ((millis()/1000) - t0) > TOLERANCIA; // Se a condicao for aceita deveAlertar = 1(true) se a condicao for falsa deveAlertar = 0(false)

  if(deveAlertar && (nivel > NIVEL_AVISO && nivel < NIVEL_LIMITE)) //se deveAlertar for verdadeiro e nivel < NIVEL_LIMITE aviso luminoso
  {
    luz();
    if(DEBUG)
      Serial.println("Em Ocio");
    else
      delay(OCIO*1000);  
  }
  else if(deveAlertar && nivel >= NIVEL_LIMITE) //se deveAlertar for verdadeiro e nivel >= NIVEL_LIMITE aviso luminoso e sonoro
  {
    luz();
    som();
    if(DEBUG)
      Serial.println("Em Ocio");
    else
      delay(OCIO*1000);
    Q_Acionamento++;
    EEPROM.write(endereco, Q_Acionamento);  
  }
  if(DEBUG)
  {
    Serial.print("Numero de vezes que acionou: ");
    Serial.println(EEPROM.read(endereco));
    delay(DELAY);
  }
}

void menu()
{
  unsigned long time;
  
  lcd.clear();
  lcd.cursor();
  lcd.blink();
  lcd.setCursor(0, 0);
  lcd.print("S.H.I.U.");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Deseja alterar limia");
  lcd.setCursor(0, 1);
  lcd.print("r ou sensibilidade");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Parar confirmar");
  lcd.setCursor(0, 1);
  lcd.print("Aperte 1");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Limiar");
  time=millis();
  do{
  estadoBotao == digitalRead(botoes[0]);
  if (estadoBotao == HIGH){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Todos os limiares?");
      lcd.setCursor(0,1);
      lcd.print("Aperte 1");
      time = millis();
  do {
     estadoBotao == digitalRead(botoes[0]);
     if (estadoBotao == HIGH){
      estadoBotao = LOW;
      verificador = true;
      alterarlimiares(verificador);
     }
    } while (millis() - time < 3001);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Algum limiares?");
  lcd.setCursor(0,1);
  lcd.print("Aperte o botao");
  time = millis();
  do {
    estadoBotao == digitalRead(botoes[0]);
    if (estadoBotao == HIGH) {
      estadoBotao = LOW;
      verificador = false;
      alterarlimiares(verificador);
    }
  } while (millis() - time < 3001);
  }
  }while (millis() - time < 3001);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Sensibilidade");
  time=millis();
  do {
    estadoBotao == digitalRead(botoes[0]);
    if (estadoBotao == HIGH) {
      ajusteSensibilidade();
    }
  } while (millis() - time < 3001);
  
}
void alterarlimiares(bool quantidade)
{
  int botaoParar=LOW;
  int escolherBotao;
  unsigned int alteradorDePotenciometro=0;
  unsigned long time;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Alterar limiar");
  lcd.setCursor(0,1);
  lcd.print("Com potenciometro");
  delay (2500);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Para parar aperte");
  lcd.setCursor(0,1);
  lcd.print("O botao 2");
  delay(2500);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Valor atual");
  lcd.setCursor(0,1);
  
  while(botaoParar != HIGH){
  if (quantidade)
  {
    for(int i = 0; i < NUM_SENSORES;i++)
    limite_POT[i]+= alteradorDePotenciometro;
    lcd.print(limite_POT[0]);
  }
  else {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Selecione o limiar");
    lcd.setCursor(0,1);
    lcd.print("Que deseja mudar");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Aperte o botao 3");
    lcd.setCursor(0,1);
    lcd.print("para confirmar");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Limiar 1");
    time = millis();
    do {
      escolherBotao = digitalRead(botoes[2]);
     if (escolherBotao == HIGH)
      limite_POT[0]+= alteradorDePotenciometro;

    } while (millis() - time < 3001);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Limiar 2");
    time = millis();
    do {
      escolherBotao = digitalRead(botoes[2]);
     if (escolherBotao == HIGH)
      limite_POT[1]+= alteradorDePotenciometro;
     
    } while (millis() - time < 3001);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Limiar 3");
    time = millis();
    do {
      escolherBotao = digitalRead(botoes[2]);
     if (escolherBotao == HIGH)
      limite_POT[2]+= alteradorDePotenciometro;
     
    } while (millis() - time < 3001);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Limiar 4");
    time = millis();
    do {
      escolherBotao = digitalRead(botoes[2]);
     if (escolherBotao == HIGH)
      limite_POT[3]+= alteradorDePotenciometro;
    } while (millis() - time < 3001);    
  }
  botaoParar = digitalRead(botoes[1]);
  }
  menu();
}

int ouvirNivel() 
{
  int value = 0;
  int leitura_sensor = 0;
  int leitura_verificador = 0;
  for(int i = 0; i < NUM_SENSORES; i++) 
  {
    if(flag_calibracao[i]){
      leitura_sensor = read_sensor(sensores[i]);
      //leitura_verificador = read_sensor(verificadores[i]);
      if(DEBUG)
      {
        imprime_valor(i,leitura_sensor);
        //imprime_verificador(i, leitura_verificador);
      }
      if(leitura_sensor > value) 
        value = leitura_sensor;
    }
  }
  return value;
}

int read_sensor(int port)
{
  unsigned long value = 0;
  for(int x = 0; x < NUM_INTERACOES; x++)//Antes tinha times, uma funcao que passava como argumento. Desnecessario muito mais viavel usar um define N_INTERACOES
  {
    value += map(analogRead(port),0,1023,1023,0); //Inverte os valores começando em 1023 e terminando em 0
                            //Especificamente esse sensor utiliza 1023 como silencio e 0 como barulho máximo.
  }
  value /= NUM_INTERACOES;
  return value;
}
void lerTempoCooler()
{
  if(tc<OVERFLOW)
  {
    if(millis()/1000 - tc > TEMPO_COOLER)
    {
      ligarcooler = expoente%2;    //Fazendo o modulo da divisão por 2, o valor de ligarcooler vai variar de 0 - 1 a cada TEMPO_COOLER
      if(DEBUG)
      {
        Serial.print("ligarcooler = "); 
        Serial.println(ligarcooler);
        Serial.print("Expoente    = ");      
        Serial.println(expoente);
      }
      expoente++;
      tc = millis()/1000;
    }
  }
  else
  {
    reset();
  }
  cooler();
}
void imprime_valor(int x, int leitura) 
{
  Serial.print("Sensor ");
  Serial.print(x+1);
  Serial.print(": ");
  Serial.println(leitura);
}
void luz() 
{
    for(int n=0 ; n< REP_LUZ; n++)
    {
      if(DEBUG) 
        Serial.println("Disparado Luz");
      else
      {
        digitalWrite(LUZ, HIGH);
        delay(TEMPO_LUZ*1000);
        digitalWrite(LUZ, LOW);
      }
      if(DEBUG) 
        Serial.println("Desligando Luz");
    }
}

void som() 
{
    for(int n=0; n < REP_SOM ; n++ )
    {
      if(DEBUG)
        Serial.println("Disparado Som");
       else
       { 
        digitalWrite(SOM, HIGH);
        delay(TEMPO_SOM*1000);
        digitalWrite(SOM, LOW);
       }
       if(DEBUG) 
        Serial.println("Desligando Som");
    }
}
void cooler()
{
  if(ligarcooler == 1 && deveAlertar == 0) //Afim de proteger o circuito, apenas ligar o coller quando os outros componentes desligados
  {
    digitalWrite(COOLER, HIGH);
    if(DEBUG) 
      Serial.println("Cooler Ligado");
  }
  else if(ligarcooler == 0)
  {
    digitalWrite(COOLER, LOW);
    if(DEBUG) 
      Serial.println("Cooler desligado");      
  }
}

void imprime_verificador(int y, int leitura)
{
  Serial.print("Verficador ");
  Serial.print(y+1);
  Serial.print(" : ");
  Serial.println(leitura);
  Serial.println();
}


//--------------------- FUNÇÃO CRIADA PARA AUXILIAR A AJUSTAR A SENSIBILIDADE DO POTENCIOMETRO --------------------
// OBS.: PRECISA TER VERIFICADO O LIMIAR DO POTENCIOMETRO PARA CADA SENSOR E INSERIDO NO CODIGO

// COM BUGS


void ajusteSensibilidade(){                      //A função recebe uma porta analogica (de um potenciometro) como parametro, para realizar a calibração do sensor
  bool ajuste = false;                           //Flag para a calibração. Regulada - True; Desregulada - False;
  int leitura;                                   //Variavel de leitura analogica da porta
  for(int i = 0; i < NUM_SENSORES; i++){         //Laço para calibrar todos os sensores listados;
    leitura = read_sensor(verificadores[i]);
    int valor = leitura - limite_POT[i];         //Variavel de analise da precisão da calibração
    if(!flag_calibracao[i]){
      if(valor > LIMITE){                        //Testes da precisão
        if(valor <= 15){
          Serial.print("Sensibilidade desregulada, girar potenciometro LEVEMENTE no sentido horario. O LIMITE EH: ");
          Serial.println(limite_POT[i]);
          imprime_verificador(i, leitura);
          delay(1000);
        }else{
          Serial.print("Sensibilidade desregulada, girar potenciometro no sentido horario. O LIMITE EH: ");
          Serial.println(limite_POT[i]);
          imprime_verificador(i, leitura);
          delay(1000);
        }
      }
      else if(valor < -LIMITE){
        if(valor >= -15){
          Serial.print("Sensibilidade desregulada, girar potenciometro LEVEMENTE no sentido anti-horario. O LIMITE EH: ");
          Serial.println(limite_POT[i]);
          imprime_verificador(i, leitura);
          delay(1000);
        }else{
          Serial.print("Sensibilidade desregulada, girar potenciometro no sentido anti-horario. O LIMITE EH: ");
          Serial.println(limite_POT[i]);
          imprime_verificador(i, leitura);
          delay(1000);
        }
      }else
        flag_calibracao[i] = true;
    }
  }
}
