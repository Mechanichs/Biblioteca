#include <EEPROM.h>
#include <LiquidCrystal.h>

#define LIMITE            6          // Define o limite de erro do sinal do potenciometro.
#define DEBUG             1          // Ativar(1) ou desativar(0) a comunicação com o serial.    *FALTA
#define ZERAR             1          // (1) zera o EEPROM (0) mantem o EEPROM com leituras anteriores.
#define DELAY_HISTERESE   15          // Valor dado em segundos para depois do acionamento da sirene
#define DELAY_MEDICAO     200        // Define o tempo para o delay entre as medicoes que serao adicionadas no vetor (em milisegundos)
#define DELAY_SIRENE      500        // Define o tempo para o delay de debug em milissegundos.
#define DELAY_BOTAO       200        // Define o tempo de espera para o delay do erro humano em relação aos botões. ~ (FUNÇÃO BOTÃO)    *FALTA
#define DELAY_AVISO       1000       // Define o tempo de espera para o usuario ler uma menssagem de aviso no display.    *FALTA
#define DELAY_DISPLAY     80         // Define o tempo de espera para o delay do display evitando que a tela fique piscando.
#define DELAY_INICIAL     2000       // Define o tempo para o delay quando o sistema é ligado na energia.
#define TAMANHO_VETOR     80         // Aproximadamente 10 interações por segundo.
#define NUM_SENSOR        1          // Numero de sensores usados. ~ (SENSOR SONORO)
#define NUM_INTERACAO     700        // Numero de interções no filtro linear.
#define NUM_REPETICAO     2          // Quantidade de vezes que a sirene irá disparar.
#define OVERFLOW          4000000000 // Over flow para o unsigned long.
#define SIRENE            13          // Sinalizador luminoso ligado à porta digital do arduino. ~ PORTA DA SIRENE
#define NIVEL_LIMITE      180        // Determina nível de ruído/pulsos para ativar a sirene. ~ NIVEL_LIMITE DO AMBIENTE
#define TEMPO_SIRENE      3          // Define o tempo de duração em que o sinalizador permanecerá ativo. 
#define PORCENT           0.2        // Define a porcentagem de medicoes despresadas na media_vetor(). 

/*
####   EPROM   ####
## Enderecos X Conteudo ##
0 - Struct com os dados as variaveis utilizadas no eeprom
*/

typedef struct st_eeprom{
  int contador;
  int tolerancia;
}t_eeprom;

short  sensor_porta[NUM_SENSOR]         = {A0};         // Sensores ligados às portas analógicas
short  sensor_sinal[NUM_SENSOR]         = {};           // Responsáveis por gravar saida do sensor
short  potenciometro_porta[NUM_SENSOR]  = {A1};         // Responsáveis por gravar saida do potenciometro
short  potenciometro_sinal[NUM_SENSOR]  = {};           // Potenciometros ligados às portas analógicas

int   vetor[TAMANHO_VETOR]              = {};    // Vetor responsável por guardar os ultimos TAMANHO_VETOR's níveis de ruído
int   media_total                       = 0;     // Valor medio do vetor de valores    EVITANDO LIXO
int   potenciometro_ideal[NUM_SENSOR]   = {};    // Valor ideal do potenciometro
int   tempo                             = 0;
int   key                               = 1;
int   contador                          = 0;    //Permite trocar apenas o valor mais antigo do vetor. Usado em adicionar_vetor().
long unsigned time1, time2; //variaveis de apoio para calcular o delta tempo
int resp1, resp2;   //variaveis responsaveis por conter o delta tempo

void(*reset)(void) = 0; //Função responsável por reiniciar a programação pelo código.

LiquidCrystal lcd(5, 4, 3, 2, 1, 0);

void setup()
{
  delay(DELAY_INICIAL); // sistema é ligado na energia

  if(ZERAR)
    clear_eeprom();

  if(DEBUG) 
    Serial.begin(9600);

  /* pinMode's */
  pinMode(SIRENE, OUTPUT);
  //sensor_porta[0] = A0;
  //potenciometro_porta = A1;
  pinMode(sensor_porta[0], INPUT);
  pinMode(potenciometro_porta[0], INPUT);

  lcd.begin(16, 2);

  zerar_vetor(); //zera o vetor para nao haver complicacoes nas primeiras medias realizadas
}

void loop()
{
  if(DEBUG)     //Debug usado para descobrir o tempo de utilizacao de processamento do programa
    time1 = millis();

  /* Primeiro passo */
  ler_sensor();
  /* Segundo passo */
  adicionar_vetor(); // filtro -> distribuir valores
  //vetor[contador] = media_sala();
  /* Terceiro passo */
  if(analisar_barulho())
    sirene(); // alarme -> zerar vetor -> delay
  //else // caso não o sirene...
  menu_iniciar(); // volta para o incicio (o display mostrando os valores atuais - recebido pelos sensores)
  delay(DELAY_MEDICAO);//tempo entre cada medicao
  
  if(DEBUG)
  {
    resp1 = millis() - time1;
    Serial.print("> Temp Loop: ");
    Serial.println(resp1);
    //lcd.print(resp1);
  }
}
/* ----- Pós void setup & loop ----- */
void menu_iniciar() // função que lança no display o que o sensor esta captando no momento (sensor de som e o potenciomentro) ~ sinal
{
  lcd.clear(); // importante
  lcd.setCursor(0, 0);
  lcd.print("Sin: ");
  lcd.setCursor(5, 0); // posicionamento primeira linha
  for(int i = 0; i < NUM_SENSOR; i++)
  {
    Serial.print(sensor_sinal[i]); // sinal = porta
    lcd.print(sensor_sinal[i]); // sinal = porta
    Serial.print("     "); // posicionamento primeira linha
    //lcd.setCursor(0, i+4+6); // posicionamento primeira linha
  }
  Serial.println("");
  lcd.setCursor(0, 1);
  lcd.print("Pot: ");
  lcd.setCursor(5, 1); // posicionamento primeira linha
  for(int i = 0; i < NUM_SENSOR; i++)
  {
    Serial.print(potenciometro_sinal[i]); // sinal = porta
    lcd.print(potenciometro_sinal[i]); // sinal = porta
    Serial.print("     "); // posicionamento segunda linha 
    //lcd.setCursor(1, i+4+6); // posicionamento primeira linha
  }
  lcd.setCursor(9, 0);
  lcd.print("|");
  lcd.setCursor(9, 1);
  lcd.print("|");
  lcd.setCursor(10, 0);
  lcd.print("Med V");
  lcd.setCursor(11, 1);
  lcd.print(media_total);
  Serial.println("");
  delay(DELAY_DISPLAY); // evita que a tela fique piscando
}
/* ----- Começando aqui após o INÍCIO ----- */
void ler_sensor() // sinal irá receber porta, para o sensor e o potenciometro, SINAL = PORTA
{
  if(DEBUG)     //Debugs usados para descobrir o tempo de utilizacao de processamento da funcao tanto por cada sensor(time2,resp2) como por todos o conjunto(time1, resp1)
    time1 = millis();

  for(int i = 0; i < NUM_SENSOR; i++)
  {
    if(DEBUG)
      time2 = millis();
    sensor_sinal[i] = filtro_linear(sensor_porta[i]); // filtro!
    potenciometro_sinal[i] = analogRead(potenciometro_porta[i]);    //segundo verifiquei, o potenciometro so esta no codigo para ser impresso
    Serial.print("sensor: ");
    Serial.println(sensor_sinal[i]);
    if(DEBUG)                                                       //os dados do potenciometro nao tem nenhuma ligacao com o processamento
    {
      resp2 = millis() - time2;
      Serial.print("- T sensor ");
      Serial.print(i);
      Serial.print(": ");
      Serial.print(resp2);
      //lcd.print(resp2);
    }
  }
  if(DEBUG)
  {
    resp1 = millis() - time1;
    Serial.print("> T total: ");
    Serial.println(resp1);
    //lcd.print(resp1);
  }
}

/* Funcao ler_sensor sem debugs

   void ler_sensor() // sinal irá receber porta, para o sensor e o potenciometro, SINAL = PORTA
   {
   for(int i = 0; i < NUM_SENSOR; i++)
   {
   sensor_sinal[i] = filtro_linear(sensor_porta[i]); // filtro!
   potenciometro_sinal[i] = analogRead(potenciometro_porta[i]);    //segundo verifiquei, o potenciometro so esta no codigo para ser impresso
   }
   }

 */

int filtro_linear(int porta) // lê uma porta e retorna o seu sinal filtrado
{
  unsigned long soma = 0;                   //filtro significa uma media aritimetica de NUM_INTERACAO(no momento, 700) medidas
  for(int i = 0; i< NUM_INTERACAO ; i++)    //permitindo assim uma maior percisao do dado recebido
    soma += analogRead(porta);              //ou seja, ele e a "propria leitura do sensor"

  return soma/NUM_INTERACAO;
}

int media_sala() // media sala(no momento). Ele permite retornar uma media aritimetica dos valores dos sensores espalhados pela sala no momento da medicao
{
  unsigned long soma = 0;
  for(int i = 0; i < NUM_SENSOR; i++)
    soma += sensor_sinal[i];

  return soma/NUM_SENSOR;
}

int media_vetor() // media sala(no momento). Retorna uma media aritimetica das medidas recolhidas durante cerca de 9 segundos(ainda precisa averiguar esse tempo)
{
  unsigned long soma = 0;   //tambem pode despresar uma porcentagem do vetor. O despreso vem de algumas medicoes mais baixas
  int nun[TAMANHO_VETOR];   //ou seja, ele ignora uma certa quantidade de valores mais baixos do sistema
  //>>A utilidade dele ainda deve ser analisada como uma arrastar de uma cadeira
  for(int i=0; i<TAMANHO_VETOR; i++)
    nun[i]=vetor[i];

  ordenamento_bolha(nun);

  for(int i = 0; i < TAMANHO_VETOR - (int)TAMANHO_VETOR * PORCENT; i++)
    soma += nun[i];

  return soma/(TAMANHO_VETOR - (int)TAMANHO_VETOR * PORCENT);
}

/* Reviveu kkkk */
void adicionar_vetor() // preencher o vetor com cada endereço a media_sala daquele respectivo momento, e sempre atualizando a cada nova interação
{
  vetor[contador] = media_sala(); //com o contador ele sempre modificara a ultima analise feita, preservando assim os dados mais recentes

  contador++;
  if(contador==TAMANHO_VETOR)
    contador=0; //zera o contador quando ele passa do limite do tamanho do vetor
}

void ordenamento_bolha(int num[])   //metodo de ordenamento decrescente de bolha para melhor utilizar a porcentagem na funcao media_vetor
{                                   //com o vetor ordenado de forma decrescente fica possivel simplesmente ignorar as ultimas informacoes do vetor ao calcular a media
  int x, y, aux;
  for( x = 0; x < TAMANHO_VETOR; x++ )
  {  
    for( y = x + 1; y < TAMANHO_VETOR; y++ ) // sempre 1 elemento à frente
    {
      if ( num[y] > num[x] )
      {
        aux = num[y];
        num[y] = num[x];
        num[x] = aux;
      }
    }
  }  
}

bool analisar_barulho() // decide se vai acionar ou nao...      
{
  t_eeprom ep;

  EEPROM.get(0, ep);
  media_total = media_vetor();  //simplificado com a criacao da funcao media vetor
  Serial.print("media vetor: ");
  Serial.println(media_vetor());

  if(media_total >= ep.tolerancia)
    return true;

  return false;

}

void sirene()
{
  Serial.println("SIRENE ATIVA!!!");
  lcd.clear(); // importante
  lcd.setCursor(0, 0);
  lcd.print("Sirene!!!");


  digitalWrite(SIRENE, HIGH);
  delay(DELAY_SIRENE*6);
  digitalWrite(SIRENE, LOW);

  lcd.clear(); // importante
  lcd.setCursor(0, 0);
  lcd.print("Histerese Sirene");

  zerar_vetor();
  adicionar_contador();

  delay(DELAY_HISTERESE*1000);
}

void zerar_vetor() // uma vez passado o limite, zerar o vetor com as medidas da sala e começar a preencher novamente
{
  for(int i = 0; i< TAMANHO_VETOR; i++)
    vetor[i] = 0;
}

void adicionar_contador(void)
{
  t_eeprom ep;

  EEPROM.get(0, ep);
  ep.contador = ep.contador + 1;
  EEPROM.put(0, ep);
}

void clear_contador(void)
{
  t_eeprom ep;

  EEPROM.get(0, ep);
  ep.contador=0;
  EEPROM.put(0, ep);
}

void clear_eeprom(void)
{
  for(int i=0; i<EEPROM.length(); i++)
    EEPROM.write(i,0);
 
  conf_padrao();
}

void conf_padrao(void)
{
  t_eeprom ep;

  ep.contador=0;
  ep.tolerancia = NIVEL_LIMITE;
  EEPROM.put(0, ep);
}

void mod_tolerancia(char c) //modificar_tolerancia
{
  t_eeprom ep;

  EEPROM.get(0, ep);

  if(c=='+')
    ep.tolerancia = ep.tolerancia + 1;
  else if(c=='-')
    ep.tolerancia = ep.tolerancia - 1;

  EEPROM.put(0, ep);
}

