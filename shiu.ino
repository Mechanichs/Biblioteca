#include <stdarg.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <SD.h>

#define TOLERANCIA_POTENCIOMETRO 250 // Define o limite de erro do sinal do potenciometro.
#define DEBUG_SERIAL      false      // Ativar(1) ou desativar(0) a comunicação com o serial.    *FALTA
#define DEBUG_TEMPO       false     
#define MICROSD           false     
#define ZERAR             1          // (1) zera o EEPROM (0) mantem o EEPROM com leituras anteriores.
#define DELAY_HISTERESE   4          // Valor dado em segundos para depois do acionamento da sirene
#define DELAY_BOTAO       200        // Define o tempo de espera para o delay do erro humano em relação aos botões. ~ (FUNÇÃO BOTÃO)    *FALTA
#define DELAY_AVISO       2000       // Define o tempo de espera para o usuario ler uma menssagem de aviso no display.    *FALTA
#define DELAY_DISPLAY     80         // Define o tempo de espera para o delay do display evitando que a tela fique piscando.
#define DELAY_INICIAL     2000       // Define o tempo para o delay quando o sistema é ligado na energia.
#define TAMANHO_VETOR     80         // Aproximadamente 10 interações por segundo.
#define TAMANHO_VETOR_MAX 130        // Aproximadamente 10 interações por segundo.
#define NUM_SENSOR        4          // Numero de sensores usados. ~ (SENSOR SONORO)
#define NUM_INTERACAO     100        // Numero de interções no filtro linear.
#define NUM_REPETICAO     2          // Quantidade de vezes que a sirene irá disparar.
#define OVERFLOW          4000000000 // Over flow para o unsigned long.
#define LED               10         
#define SIRENE            11         // Sinalizador luminoso ligado à porta digital do arduino. ~ PORTA DA SIRENE
#define LAMPADA           12       
#define NIVEL_LIMITE      180        // Determina nível de ruído/pulsos para ativar a sirene. ~ NIVEL_LIMITE DO AMBIENTE
#define TEMPO_SIRENE      3          // Define o tempo de duração em que o sinalizador permanecerá ativo. 
#define PORCENT           0.2        // Define a porcentagem de medicoes despresadas na media_vetor().
#define TEMPO_PROCESSAMENTO 320

/*
####   EPROM   ####
## Enderecos X Conteudo ##
0 - Struct com os dados as variaveis utilizadas no eeprom
512 - Struct com os dados as variaveis utilizadas no define
*/

/*
#### Portas ####
Sensores: A1, A2, A4, A6
Potenciometros: A0, A3, A5, A7
Sirene: D11
Lampada: D12
Led: D10
LCD: (5, 4, 3, 2, 0, 1)
MicroSD: 13(SCK), 12(MISO), 11(MOSI), 10(CS)
Botoes: D6(menu), D7(down), D8(up), D9(change)
RJ11: GND(preto), Vcc(vermelho), OUT(verde), Pot(amarelo)
*/

typedef struct st_eeprom{
  int contador;
  int tolerancia;
  int tolerancia_potenciometro;      // 250        // Define o limite de erro do sinal do potenciometro.
  bool sensor_chave[NUM_SENSOR];        
  short potenciometro_ideal[NUM_SENSOR];
  float delay_histerese;             // 4          // Valor dado em segundos para depois do acionamento da sirene. [Em Segundos]
  unsigned short tempo_sirene;       // 3          // Define o tempo de duração em que o sinalizador permanecerá ativo. [Em segundos] 
}t_eeprom;

typedef struct st_define{
  bool debug_serial;                 // false      // Ativar(1) ou desativar(0) a comunicação com o serial.    *FALTA
  bool debug_tempo;                  // false     
  bool microsd;                      // false     
  float porcent;                     // 0.2        // Define a porcentagem de medicoes despresadas na media_vetor().
  int delay_botao;                   // 200        // Define o tempo de espera para o delay do erro humano em relação aos botões. ~ (FUNÇÃO BOTÃO)    *FALTA
  int tamanho_vetor;                 // 80         // Aproximadamente 3 interações por segundo.
  unsigned short num_interacao;      // 100        // Numero de interções no filtro linear.
  unsigned short tempo_processamento;// 320
}t_define;

//#### Para as variaveis do define - inicio ####
bool debug_serial;                 // false      // Ativar(1) ou desativar(0) a comunicação com o serial.    *FALTA
bool debug_tempo;                  // false     
bool microsd;                      // false     
float porcent;                     // 0.2        // Define a porcentagem de medicoes despresadas na media_vetor().
int delay_botao;                   // 200        // Define o tempo de espera para o delay do erro humano em relação aos botões. ~ (FUNÇÃO BOTÃO)    *FALTA
int tamanho_vetor;                 // 80         // Aproximadamente 3 interações por segundo.
unsigned short num_interacao;      // 100        // Numero de interções no filtro linear.
unsigned short tempo_processamento;// 320
//#### Para as variaveis do define - fim ####

bool  sensor_status[NUM_SENSOR];        
short  sensor_porta[NUM_SENSOR]         = {A1, A2, A4, A6};         // Sensores ligados às portas analógicas
short  sensor_sinal[NUM_SENSOR]         = {};           // Responsáveis por gravar saida do sensor
short  potenciometro_porta[NUM_SENSOR]  = {A0, A3, A5, A7};         // Responsáveis por gravar saida do potenciometro
short  potenciometro_sinal[NUM_SENSOR]  = {};           // Potenciometros ligados às portas analógicas

int   vetor[TAMANHO_VETOR_MAX]          = {};    // Vetor responsável por guardar os ultimos TAMANHO_VETOR's níveis de ruído
int   media_total                       = 0;     // Valor medio do vetor de valores    EVITANDO LIXO
int   posicao                           = 0;    //Permite trocar apenas o valor mais antigo do vetor. Usado em adicionar_vetor().
long unsigned time1, time2; //variaveis de apoio para calcular o delta tempo
int resp1, resp2;   //variaveis responsaveis por conter o delta tempo

File arq;

void(*reset)(void) = 0; //Função responsável por reiniciar a programação pelo código.

LiquidCrystal lcd(5, 4, 3, 2, 0, 1); //NANO é 0, 1
//LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

void setup()
{
  delay(DELAY_INICIAL); // sistema é ligado na energia
  
  t_eeprom ep; 

  lcd.begin(16, 2);
 
  if(debug_serial) 
    Serial.begin(9600);

  conf_padrao_def();
  init_def();
    
  if(ZERAR)
    clear_eeprom();

  EEPROM.get(0, ep);

  if(microsd)
  {
    SD.begin(10);

    arq = SD.open("texto.txt", FILE_WRITE);
  
    lcd.setCursor(0, 0); // posicionamento primeira linha

    if(arq) {
      arq.println("Teste de arquivos TXT em SD no Arduino");
      Serial.println("OK.");
      lcd.println("Arquivo aberto");
    }
    else {
      Serial.println("Erro ao abrir ou criar o arquivo texto.txt.");
      lcd.println("Erro ao abrir");
    }
    delay(DELAY_AVISO); // para o print da tela
  }

  /* pinMode's */
  pinMode(LED, OUTPUT);
  pinMode(SIRENE, OUTPUT);
  pinMode(LAMPADA, OUTPUT);

  for(int i=0; i<NUM_SENSOR; i++)
  {
    ep.sensor_chave[i]=true;
    sensor_status[i]=true;
    ep.potenciometro_ideal[i] = 540;
  }
  EEPROM.put(0, ep);

  zerar_vetor(); //zera o vetor para nao haver complicacoes nas primeiras medias realizadas
}

void loop()
{
  if(debug_tempo)     //Debug usado para descobrir o tempo de utilizacao de processamento do programa
    time1 = millis();

  /* Primeiro passo */
  ler_sensor();
  /* Segundo passo */
  adicionar_vetor(); // filtro -> distribuir valores
  /* Terceiro passo */
  if(analisar_barulho())
    sirene(); // alarme -> zerar vetor -> delay
  //else // caso não o sirene...
  menu_iniciar(); // volta para o incicio (o display mostrando os valores atuais - recebido pelos sensores)

  if(debug_tempo)
  {
    resp1 = millis() - time1;
    if(debug_serial)
    {
      Serial.print("> Temp Loop: ");
      Serial.println(resp1);
    }
    if(microsd)
    {
      arq.print("> Temp Loop: ");
      arq.println(resp1);
    }
    //lcd.print(resp1);
  }
  while(millis()-time1 < tempo_processamento)
    delay(1);
  if(debug_tempo)
  {
    resp1 = millis() - time1;
    if(debug_serial)
    {
      Serial.print("> Temp Loop Final: ");
      Serial.println(resp1);
    } 
    if(microsd)
    {
      arq.print("> Temp Loop Final: ");
      arq.println(resp1);
    }
    //lcd.print(resp1);
  }
}
/* ----- Pós void setup & loop ----- */
void menu_iniciar(void) // função que lança no display o que o sensor esta captando no momento (sensor de som e o potenciomentro) ~ sinal
{
  lcd.clear(); // importante
  for(int i = 0; i < NUM_SENSOR; i++)
  {
    lcd.setCursor(i*4, 0); // posicionamento primeira linha
    lcd.print(sensor_sinal[i]); // sinal = porta
    if(debug_serial) 
    {
      Serial.print(sensor_sinal[i]); // sinal = porta
      Serial.print("     "); // posicionamento primeira linha
    }
    if(microsd)
    {
      arq.print(sensor_sinal[i]);
      arq.print("     "); // posicionamento primeira linha
    }
  }
    if(debug_serial) 
      Serial.println("");
    if(microsd)
      arq.println("");
  for(int i = 0; i < NUM_SENSOR; i++)
  {
    lcd.setCursor(i*4, 1); // posicionamento primeira linha
    lcd.print(potenciometro_sinal[i]); // sinal = porta
    if(debug_serial)
    {
      Serial.print(potenciometro_sinal[i]); // sinal = porta
      Serial.print("     "); // posicionamento segunda linha 
    }
    if(microsd)
    {
      arq.print(potenciometro_sinal[i]); // sinal = porta
      arq.print("     "); // posicionamento segunda linha 
    }
  }
  if(debug_serial) 
    Serial.println("");
  if(microsd)
    arq.println("");
}
/* ----- Começando aqui após o INÍCIO ----- */
void ler_sensor(void) // sinal irá receber porta, para o sensor e o potenciometro, SINAL = PORTA
{
  unsigned long soma[NUM_SENSOR];
  int i, j;
  t_eeprom ep;

  EEPROM.get(0, ep);


  for(i = 0; i< NUM_SENSOR ; i++)
    soma[i] = 0;

  for(i = 0; i< num_interacao ; i++)    //permitindo assim uma maior percisao do dado recebido
    for(j = 0; j< NUM_SENSOR ; j++)
      if(ep.sensor_chave[j])
        soma[j] += analogRead(sensor_porta[j]);              //ou seja, ele e a "propria leitura do sensor"

  for(i = 0; i< NUM_SENSOR ; i++)
  {
    potenciometro_sinal[i] = analogRead(potenciometro_porta[i]);    //segundo verifiquei, o potenciometro so esta no codigo para ser impresso
    sensor_sinal[i] = soma[i]/num_interacao;
  }

  return;
}

int media_sala(void) // media sala(no momento). Ele permite retornar uma media aritimetica dos valores dos sensores espalhados pela sala no momento da medicao
{
  int j=0;
  unsigned long soma = 0;
  t_eeprom ep;

  EEPROM.get(0, ep);

  for(int i = 0; i < NUM_SENSOR; i++)
    if(ep.sensor_chave[i])
      if(sensor_status[i])
      {
        soma += sensor_sinal[i];
        j++;
      }

  return soma/j;
}

int media_vetor(void) // media sala(no momento). Retorna uma media aritimetica das medidas recolhidas durante cerca de 9 segundos(ainda precisa averiguar esse tempo)
{
  unsigned long soma = 0;   //tambem pode despresar uma porcentagem do vetor. O despreso vem de algumas medicoes mais baixas
  int nun[tamanho_vetor];   //ou seja, ele ignora uma certa quantidade de valores mais baixos do sistema
  //>>A utilidade dele ainda deve ser analisada como uma arrastar de uma cadeira
  for(int i=0; i<tamanho_vetor; i++)
    nun[i]=vetor[i];

  ordenamento_bolha(nun);

  for(int i = 0; i < tamanho_vetor - (int)tamanho_vetor * porcent; i++)
    soma += nun[i];

  return soma/(tamanho_vetor - (int)tamanho_vetor * porcent);
}

/* Reviveu kkkk */
void adicionar_vetor(void) // preencher o vetor com cada endereço a media_sala daquele respectivo momento, e sempre atualizando a cada nova interação
{
  verificar_intervalo();

  vetor[posicao] = media_sala(); //com a posicao ele sempre modificara a ultima analise feita, preservando assim os dados mais recentes

  posicao++;
  if(posicao==tamanho_vetor)
    posicao=0; //zera a posicao quando ele passa do limite do tamanho do vetor
}

void ordenamento_bolha(int num[])   //metodo de ordenamento decrescente de bolha para melhor utilizar a porcentagem na funcao media_vetor
{                                   //com o vetor ordenado de forma decrescente fica possivel simplesmente ignorar as ultimas informacoes do vetor ao calcular a media
  int x, y, aux;
  for( x = 0; x < tamanho_vetor; x++ )
    for( y = x + 1; y < tamanho_vetor; y++ ) // sempre 1 elemento à frente
      if ( num[y] > num[x] )
      {
        aux = num[y];
        num[y] = num[x];
        num[x] = aux;
      }
}

bool analisar_barulho(void) // decide se vai acionar ou nao...      
{
  t_eeprom ep;

  EEPROM.get(0, ep);
  media_total = media_vetor();  //simplificado com a criacao da funcao media vetor
  if(debug_serial)
  {
    Serial.print("media vetor: ");
    Serial.println(media_vetor());
  }
  if(microsd)
  {
    arq.print("media vetor: ");
    arq.println(media_vetor());
  }

  if(media_total >= 160)
    digitalWrite(LAMPADA, HIGH);
  else
    digitalWrite(LAMPADA, LOW);

  if(media_total >= ep.tolerancia)
    return true;

  return false;
}

void sirene(void)
{
  t_eeprom ep;

  EEPROM.get(0, ep);

  if(debug_serial)
    Serial.println("SIRENE ATIVA!!!");
  if(microsd)
    arq.println("SIRENE ATIVA!!!");
  lcd.clear(); // importante
  lcd.setCursor(0, 0);
  lcd.print("Sirene!!!");

  digitalWrite(SIRENE, HIGH);
  delay(ep.tempo_sirene*1000);
  digitalWrite(SIRENE, LOW);

  lcd.clear(); // importante
  lcd.setCursor(0, 0);
  lcd.print("Histerese Sirene");

  zerar_vetor();
  adicionar_contador();

  delay(ep.delay_histerese*1000);
}

void zerar_vetor(void) // uma vez passado o limite, zerar o vetor com as medidas da sala e começar a preencher novamente
{
  for(int i = 0; i< tamanho_vetor; i++)
    vetor[i] = 0;
}

void adicionar_contador(void)//adiciona +1 no contador de ativacoes e grava novamente no eeprom [funcao auxiliar eeprom]
{
  t_eeprom ep;

  EEPROM.get(0, ep);
  ep.contador = ep.contador + 1;
  EEPROM.put(0, ep);
}

void clear_contador(void)//zera o contador de ativacoes da sirene [funcao auxiliar eeprom]
{
  t_eeprom ep;

  EEPROM.get(0, ep);
  ep.contador=0;
  EEPROM.put(0, ep);
}

void clear_eeprom(void)//limpa todo o eeprom e depois carrega a configuracao padrao [funcao auxiliar eeprom]
{
  for(int i=0; i<EEPROM.length(); i++)
    EEPROM.write(i,0);

  conf_padrao();
}

void conf_padrao(void)//carrega a parte inicial do eeprom(endereco 0) com a struct das informacoes do eeprom
{                     //modifica todas as configuracoes para "configuracoes de fabrica"
  t_eeprom ep;

  ep.contador=0;
  ep.tolerancia = NIVEL_LIMITE;
  ep.tempo_sirene = TEMPO_SIRENE;
  ep.delay_histerese = DELAY_HISTERESE;
  ep.tolerancia_potenciometro = TOLERANCIA_POTENCIOMETRO;

  for(int i=0; i<NUM_SENSOR; i++)
  {
    ep.sensor_chave[i]=true;
    ep.potenciometro_ideal[i] = 540;
  }

  EEPROM.put(0, ep);
}

void conf_padrao_def(void)//carrega a parte do eeprom(endereco 512) com a struct das informacoes dos define's
{                     //modifica todas as configuracoes para "configuracoes de fabrica"
  t_define def;

  def.debug_serial  = DEBUG_SERIAL;
  def.debug_tempo = DEBUG_TEMPO;
  def.microsd = MICROSD;
  def.porcent = PORCENT;
  def.delay_botao = DELAY_BOTAO;
  def.tamanho_vetor = TAMANHO_VETOR;
  def.num_interacao = NUM_INTERACAO;
  def.tempo_processamento = TEMPO_PROCESSAMENTO;

  EEPROM.put(512, def);
}

void init_def(void);
{
  t_define def;

  EEPROM.get(512, def);

  debug_serial  = def.debug_serial;
  debug_tempo = def.debug_tempo;
  microsd = def.microsd;
  porcent = def.porcent;
  delay_botao = def.delay_botao;
  tamanho_vetor = def.tamanho_vetor;
  num_interacao = def.num_interacao;
  tempo_processamento = def.tempo_processamento;

}

void mod_tolerancia(char c) //modificar_tolerancia [funcao auxiliar eeprom]
{
  t_eeprom ep;

  EEPROM.get(0, ep);

  if(c=='+')
    ep.tolerancia = ep.tolerancia + 1;
  else if(c=='-')
    ep.tolerancia = ep.tolerancia - 1;

  EEPROM.put(0, ep);
}

void mod_chave(int sensor, bool status) //modificar_chave - do sensor [funcao auxiliar eeprom]
{
  t_eeprom ep;

  EEPROM.get(0, ep);

  ep.sensor_chave[sensor] = status;

  EEPROM.put(0, ep);
}

void mod_pot_ideal(int pot, char botao) //modificar_potenciometro_ideal [funcao auxiliar eeprom]
{
  t_eeprom ep;

  EEPROM.get(0, ep);

  if(botao=='+')
    ep.potenciometro_ideal[pot] = ep.potenciometro_ideal[pot] + 1;
  else if(botao=='-')
    ep.potenciometro_ideal[pot] = ep.potenciometro_ideal[pot] - 1;

  EEPROM.put(0, ep);
}

void verificar_intervalo(void)//verifica se o sensor esta conectado atravez de uma analise de intervalo
{                             //e posteriormente verifica se o potenciometro esta dentro do intevalo desejado
  t_eeprom ep;                //caso algum desses nao esteja correto, ele nao permite que a leitura desse sensor  
                              //entre na media da sala
  EEPROM.get(0, ep);
  
  intervalo_pot(ep);

  pulldown(ep);

  desligar_menor(ep);

  return;
}

float porcento_aux(int qt, int l, ...)
{
  float media;
  int soma=0, i;
  va_list va;

  va_start(va, qt);

  for(i=0; i<qt; i++)
    soma += sensor_sinal[va_arg(va, int)];

  va_end(va);

  media = soma/3;

  return (media - sensor_sinal[l])/media; //se o valor do sensor for menor, entao ele dara uma subtracao positiva e dividindo pelo valor do maior vai dar a porcentagem desejada
}

void pulldown(t_eeprom ep)
{
  int sinal_maior=0, i_maior, i;

  for(i=0; i<NUM_SENSOR; i++)
    if(ep.sensor_chave[i])
      if(sinal_maior <= sensor_sinal[i])
        i_maior = i;
  
  for(i=0; i<NUM_SENSOR; i++)
    if(ep.sensor_chave[i])
      if(sensor_sinal[i]<=20 || sensor_sinal[i]>950)
        sensor_status[i] = false;
      else if(porcento_aux(1,i,i_maior) >= 0.65 )
        sensor_status[i] = false;
}

void intervalo_pot(t_eeprom ep)
{
  int i;

  for(i=0; i<NUM_SENSOR; i++)
    if(ep.sensor_chave[i])
      if( potenciometro_sinal[i] > (ep.potenciometro_ideal[i] + ep.tolerancia_potenciometro)  || potenciometro_sinal[i] < (ep.potenciometro_ideal[i] - ep.tolerancia_potenciometro))
        sensor_status[i]=false;
      else
        sensor_status[i] = true;
}

void desligar_menor(t_eeprom ep)
{
  int i, j, k, aux, num, escolha = -1;
  float valor, maior = 0.0;
  bool key=true;

  num = aux = 3;

  for(i=0; i<NUM_SENSOR; i++)
    if(ep.sensor_chave[i]==false || sensor_status[i]==false)
    {
      key = false;
      break;
    }

  if(key && NUM_SENSOR >= 4)
  {
    for(i=0; i<=num; i++)
      for(j=i+1; j<=num; j++)
        for(k=j+1; k<=num; k++)
        {
          valor = porcento_aux(3, aux, i, j, k);
          if(valor>=0.2)
            if(valor>maior)
              escolha = aux;
          aux--;
        }

    if(escolha!= -1)
      sensor_status[escolha]=false;
  }
}


