#include <stdarg.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <SD.h>

#define TOLERANCIA_POTENCIOMETRO 250 // Define o limite de erro do sinal do potenciometro.
#define DEBUG_SERIAL      true     // Ativar(1) ou desativar(0) a comunicação com o serial.    *FALTA
#define DEBUG_TEMPO       false     
#define MICROSD           false     
#define ZERAR             1          // (1) zera o EEPROM (0) mantem o EEPROM com leituras anteriores.
#define DELAY_HISTERESE   4          // Valor dado em segundos para depois do acionamento da sirene
#define DELAY_BOTAO       200        // Define o tempo de espera para o delay do erro humano em relação aos botões. ~ (FUNÇÃO BOTÃO)    *FALTA
#define DELAY_AVISO       2000       // Define o tempo de espera para o usuario ler uma menssagem de aviso no display.    *FALTA
#define DELAY_DISPLAY     80         // Define o tempo de espera para o delay do display evitando que a tela fique piscando.
#define DELAY_INICIAL     2000       // Define o tempo para o delay quando o sistema é ligado na energia.
#define TAMANHO_VETOR     80         // Aproximadamente 10 interações por segundo.
#define NUM_SENSOR        4          // Numero de sensores usados. ~ (SENSOR SONORO)
#define NUM_INTERACAO     500        // Numero de interções no filtro linear.
#define NUM_REPETICAO     2          // Quantidade de vezes que a sirene irá disparar.
#define OVERFLOW          4000000000 // Over flow para o unsigned long.
#define LED               10         
#define SIRENE            11         // Sinalizador luminoso ligado à porta digital do arduino. ~ PORTA DA SIRENE
#define LAMPADA           12       
#define NIVEL_LIMITE      75        // Determina nível de ruído/pulsos para ativar a sirene. ~ NIVEL_LIMITE DO AMBIENTE
#define TEMPO_SIRENE      3          // Define o tempo de duração em que o sinalizador permanecerá ativo. 
#define PORCENT           0.2        // Define a porcentagem de medicoes mais baixas despresadas na media_vetor().
#define PORCENT2           0.00        // Define a porcentagem de medicoes mais altas despresadas na media_vetor().
#define TEMPO_PROCESSAMENTO 320
#define TEMPO_CALIBRAGEM  22         //tempo dado em segundos

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
  bool sensor_chave[NUM_SENSOR];        
  short potenciometro_ideal[NUM_SENSOR];
}t_eeprom;

bool  sensor_status[NUM_SENSOR];        
short  sensor_porta[NUM_SENSOR]         = {A1, A2, A4, A6};         // Sensores ligados às portas analógicas
short  sensor_sinal[NUM_SENSOR]         = {};           // Responsáveis por gravar saida do sensor
short  sensor_max[NUM_SENSOR]= {0,0,0,0};
short  sensor_min[NUM_SENSOR]= {1023,1023,1023,1023};
short  potenciometro_porta[NUM_SENSOR]  = {A0, A3, A5, A7};         // Responsáveis por gravar saida do potenciometro
short  potenciometro_sinal[NUM_SENSOR]  = {};           // Potenciometros ligados às portas analógicas

int   vetor[TAMANHO_VETOR]              = {};    // Vetor responsável por guardar os ultimos TAMANHO_VETOR's níveis de ruído
int   media_total                       = 0;     // Valor medio do vetor de valores    EVITANDO LIXO
int   contador                          = 0;    //Permite trocar apenas o valor mais antigo do vetor. Usado em adicionar_vetor().
long unsigned time1; //variaveis de apoio para calcular o delta tempo
int resp1;   //variaveis responsaveis por conter o delta tempo

File arq;

void(*reset)(void) = 0; //Função responsável por reiniciar a programação pelo código.

LiquidCrystal lcd(5, 4, 3, 2, 0, 1); //NANO é 0, 1
//LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

void setup()
{
  delay(DELAY_INICIAL); // sistema é ligado na energia

  t_eeprom ep; 

  lcd.begin(16, 2);

  if(DEBUG_SERIAL) 
    Serial.begin(9600);

  if(ZERAR)
    clear_eeprom();

  //  conf_padrao_def();

  EEPROM.get(0, ep);

  if(MICROSD)
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

  verificar_sensores();
  lcd.clear(); // importante
  lcd.setCursor(0, 1);
  lcd.print("CALIBRAGEM");
  calibra();
}

void loop()
{
  //if(DEBUG_TEMPO)     //Debug usado para descobrir o tempo de utilizacao de processamento do programa
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

  if(DEBUG_SERIAL)
  {
    for(int k; k<NUM_SENSOR; k++)
    {
      Serial.println("MAX:");
      Serial.println(sensor_max[k]);
      Serial.println("MIN:");
      Serial.println(sensor_min[k]);
    }
  }

  if(DEBUG_TEMPO)
  {
    resp1 = millis() - time1;
    if(DEBUG_SERIAL)
    {
      Serial.print("> Temp Loop: ");
      Serial.println(resp1);
    }
    if(MICROSD)
    {
      arq.print("> Temp Loop: ");
      arq.println(resp1);
    }
    //lcd.print(resp1);
  }

  lcd.setCursor(8, 1);
  lcd.print(media_total);
  while(millis()-time1 < TEMPO_PROCESSAMENTO)
    delay(1);
  if(DEBUG_TEMPO)
  {
    resp1 = millis() - time1;
    if(DEBUG_SERIAL)
    {
      Serial.print("> Temp Loop Final: ");
      Serial.println(resp1);
    } 
    if(MICROSD)
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
    if(DEBUG_SERIAL) 
    {
      Serial.print(sensor_sinal[i]); // sinal = porta
      Serial.print("     "); // posicionamento primeira linha
    }
    if(MICROSD)
    {
      arq.print(sensor_sinal[i]);
      arq.print("     "); // posicionamento primeira linha
    }
  }
  if(DEBUG_SERIAL) 
    Serial.println("");
  if(MICROSD)
    arq.println("");
  for(int i = 0; i < NUM_SENSOR; i++)
  {
    lcd.setCursor(i*4, 1); // posicionamento primeira linha
    if(i!=2)
      lcd.print(potenciometro_sinal[i]); // sinal = porta
    if(DEBUG_SERIAL)
    {
      Serial.print(potenciometro_sinal[i]); // sinal = porta
      Serial.print("     "); // posicionamento segunda linha 
    }
    if(MICROSD)
    {
      arq.print(potenciometro_sinal[i]); // sinal = porta
      arq.print("     "); // posicionamento segunda linha 
    }
  }
  if(DEBUG_SERIAL) 
    Serial.println("");
  if(MICROSD)
    arq.println("");
}
/* ----- Começando aqui após o INÍCIO ----- */
void ler_sensor(void) // sinal irá receber porta, para o sensor e o potenciometro, SINAL = PORTA
{
  unsigned long soma[NUM_SENSOR];
  int i, j, x;
  t_eeprom ep;

  EEPROM.get(0, ep);


  for(i = 0; i< NUM_SENSOR ; i++)
    soma[i] = 0;
  //Ler junto  os sensores e somar
  for(i = 0; i< NUM_INTERACAO ; i++)    //permitindo assim uma maior percisao do dado recebido
    for(j = 0; j< NUM_SENSOR ; j++)
      if(ep.sensor_chave[j])
        soma[j] += analogRead(sensor_porta[j]);              //ou seja, ele e a "propria leitura do sensor"

  //ler potenciometro por 5 vezes e faz a media do vetor 
  for(i = 0; i< NUM_SENSOR ; i++)
  {
    x=0;
    for(j = 0; j< 5 ; j++)
      x += analogRead(potenciometro_porta[i]);    //segundo verifiquei, o potenciometro so esta no codigo para ser impresso
    potenciometro_sinal[i] = x/j;
    j = soma[i]/NUM_INTERACAO;
    sensor_sinal[i] = ((((1000*j-1000*sensor_min[i])/(sensor_max[i] - sensor_min[i]))));
    Serial.print(j);
    Serial.print("   ");
    Serial.print(sensor_min[i]);
    Serial.print("   ");
    Serial.print(sensor_max[i]);
    Serial.print("   ");
    Serial.println((((1000*j-1000*sensor_min[i])/(sensor_max[i] - sensor_min[i]))));

  }

  return;
}
void verificar_sensores()
{
  unsigned time2;
  time1 = millis();

  while(millis()-time1 < 10*1000)
  {
    time2 = millis();
    ler_sinal();
    menu_iniciar();
    while(millis()-time2 < TEMPO_PROCESSAMENTO)
      delay(1);
  }

}
void ler_sinal(void) // sinal irá receber porta, para o sensor e o potenciometro, SINAL = PORTA
{
  unsigned long soma[NUM_SENSOR];
  int i, j;
  t_eeprom ep;

  EEPROM.get(0, ep);


  for(i = 0; i< NUM_SENSOR ; i++)
    soma[i] = 0;

  for(i = 0; i< NUM_INTERACAO ; i++)    //permitindo assim uma maior percisao do dado recebido
    for(j = 0; j< NUM_SENSOR ; j++)
      if(ep.sensor_chave[j])
        soma[j] += analogRead(sensor_porta[j]);              //ou seja, ele e a "propria leitura do sensor"

  for(i = 0; i< NUM_SENSOR ; i++)
  {
    potenciometro_sinal[i] = analogRead(potenciometro_porta[i]);    //segundo verifiquei, o potenciometro so esta no codigo para ser impresso
    sensor_sinal[i] = soma[i]/NUM_INTERACAO;
  }

  return;
}

int ler(int porta)
{
  int i, soma=0;

  for(i=0; i<10; i++)
    soma += analogRead(sensor_porta[porta]);

  return soma/10;
}

void calibra(void)
{
  int j, i, menor, maior, sens, med, sinal;
  unsigned int soma=0;
  unsigned long int tempo_inicial;

  tempo_inicial = millis();

  while((millis() - tempo_inicial)/1000 < TEMPO_CALIBRAGEM)
  {
    Serial.println(millis());
    Serial.println(tempo_inicial);
    Serial.println(millis() - tempo_inicial);
    ler_sinal();
    Serial.println("passo1");
    maior = 0;
    menor = 1023;
    for(i=0;i<4;i++)
    {
      if(sensor_sinal[i]> maior)
      {
        maior=sensor_sinal[i];
        sens=i;
      }
    }
    if(sensor_max[sens] < maior)
      sensor_max[sens] = maior;
    Serial.println("passo2");
    Serial.println("passo4");

    for(i=0;i<4;i++)
      if(i!=sens)
      {
        Serial.println("passo5");
        if(sensor_sinal[i] < sensor_min[i])
        {
          sensor_min[i] = sensor_sinal[i];
          Serial.println(sensor_min[i]);
          Serial.println(i);
        }
        Serial.println("passo6");
      }
    Serial.println("PASSO7");
  }
  Serial.println("SAAAAAAIIIIIIIIUUUUUUUUUU");
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
  int nun[TAMANHO_VETOR];   //ou seja, ele ignora uma certa quantidade de valores mais baixos do sistema
  //>>A utilidade dele ainda deve ser analisada como uma arrastar de uma cadeira
  for(int i=0; i<TAMANHO_VETOR; i++)
    nun[i]=vetor[i];

  ordenamento_bolha(nun);

  for(int i = (int)(TAMANHO_VETOR * PORCENT2); i < TAMANHO_VETOR - (int)TAMANHO_VETOR * PORCENT; i++)
    soma += nun[i];

  return soma/(TAMANHO_VETOR - (int)TAMANHO_VETOR * PORCENT);
}

/* Reviveu kkkk */
void adicionar_vetor(void) // preencher o vetor com cada endereço a media_sala daquele respectivo momento, e sempre atualizando a cada nova interação
{
  verificar_intervalo(); //

  vetor[contador] = media_sala(); //com o contador ele sempre modificara a ultima analise feita, preservando assim os dados mais recentes

  contador++;
  if(contador==TAMANHO_VETOR)
    contador=0; //zera o contador quando ele passa do limite do tamanho do vetor
}

void ordenamento_bolha(int num[])   //metodo de ordenamento decrescente de bolha para melhor utilizar a porcentagem na funcao media_vetor
{                                   //com o vetor ordenado de forma decrescente fica possivel simplesmente ignorar as ultimas informacoes do vetor ao calcular a media
  int x, y, aux;
  for( x = 0; x < TAMANHO_VETOR; x++ )
    for( y = x + 1; y < TAMANHO_VETOR; y++ ) // sempre 1 elemento à frente
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
  if(DEBUG_SERIAL)
  {
    Serial.print("media vetor: ");
    Serial.println(media_vetor());
  }
  if(MICROSD)
  {
    arq.print("media vetor: ");
    arq.println(media_vetor());
  }

  if(media_total >= 160)
    digitalWrite(LAMPADA, HIGH);
  else
    digitalWrite(LAMPADA, LOW);

  lcd.clear();
  lcd.setCursor(12, 1);
  lcd.print(media_total);

  if(media_total >= ep.tolerancia)
    return true;

  return false;
}

void sirene(void)
{
  if(DEBUG_SERIAL)
    Serial.println("SIRENE ATIVA!!!");
  if(MICROSD)
    arq.println("SIRENE ATIVA!!!");
  lcd.clear(); // importante
  lcd.setCursor(0, 0);
  lcd.print("Sirene!!!");

  digitalWrite(SIRENE, HIGH);
  delay(TEMPO_SIRENE*1000);
  digitalWrite(SIRENE, LOW);

  lcd.clear(); // importante
  lcd.setCursor(0, 0);
  lcd.print("Histerese Sirene");

  zerar_vetor();
  adicionar_contador();

  delay(DELAY_HISTERESE*1000);
}

void zerar_vetor(void) // uma vez passado o limite, zerar o vetor com as medidas da sala e começar a preencher novamente
{
  for(int i = 0; i< TAMANHO_VETOR; i++)
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

  for(int i=0; i<NUM_SENSOR; i++)
  {
    ep.sensor_chave[i]=true;
    ep.potenciometro_ideal[i] = 540;
  }

  EEPROM.put(0, ep);
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
      {
        sinal_maior = sensor_sinal[i];
        i_maior = i;
      }

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

  for(i=0; i<NUM_SENSOR; i++)//intervalo de segurança do potenciometro
    if(ep.sensor_chave[i])
      if( potenciometro_sinal[i] > (ep.potenciometro_ideal[i] + TOLERANCIA_POTENCIOMETRO)  || potenciometro_sinal[i] < (ep.potenciometro_ideal[i] - TOLERANCIA_POTENCIOMETRO))
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



