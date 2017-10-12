#include <EEPROM.h>
#include <LiquidCrystal.h>

#define TOLERANCIA_POTENCIOMETRO 50  // Define o limite de erro do sinal do potenciometro.
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
#define NUM_INTERACAO     100        // Numero de interções no filtro linear.
#define NUM_REPETICAO     2          // Quantidade de vezes que a sirene irá disparar.
#define OVERFLOW          4000000000 // Over flow para o unsigned long.
#define SIRENE            13          // Sinalizador luminoso ligado à porta digital do arduino. ~ PORTA DA SIRENE
#define NIVEL_LIMITE      180        // Determina nível de ruído/pulsos para ativar a sirene. ~ NIVEL_LIMITE DO AMBIENTE
#define TEMPO_SIRENE      3          // Define o tempo de duração em que o sinalizador permanecerá ativo. 
#define PORCENT           0.2        // Define a porcentagem de medicoes despresadas na media_vetor().
#define TEMPO_PROCESSAMENTO 200

/*
####   EPROM   ####
## Enderecos X Conteudo ##
0 - Struct com os dados as variaveis utilizadas no eeprom
*/

typedef struct st_eeprom{
  int contador;
  int tolerancia;
  bool sensor_chave[NUM_SENSOR];        
  short potenciometro_ideal[NUM_SENSOR];
}t_eeprom;

bool  sensor_status[NUM_SENSOR];        
short  sensor_porta[NUM_SENSOR]         = {A0};         // Sensores ligados às portas analógicas
short  sensor_sinal[NUM_SENSOR]         = {};           // Responsáveis por gravar saida do sensor
short  potenciometro_porta[NUM_SENSOR]  = {A1};         // Responsáveis por gravar saida do potenciometro
short  potenciometro_sinal[NUM_SENSOR]  = {};           // Potenciometros ligados às portas analógicas

int   vetor[TAMANHO_VETOR]              = {};    // Vetor responsável por guardar os ultimos TAMANHO_VETOR's níveis de ruído
int   media_total                       = 0;     // Valor medio do vetor de valores    EVITANDO LIXO
//int   potenciometro_ideal[NUM_SENSOR]   = {};    // Valor ideal do potenciometro
//int   tempo                             = 0;
//int   key                               = 1;
//int   contador                          = 0;    //Permite trocar apenas o valor mais antigo do vetor. Usado em adicionar_vetor().
long unsigned time1, time2; //variaveis de apoio para calcular o delta tempo
int resp1, resp2;   //variaveis responsaveis por conter o delta tempo

void(*reset)(void) = 0; //Função responsável por reiniciar a programação pelo código.

LiquidCrystal lcd(5, 4, 3, 2, 1, 0);

void setup()
{
  t_eeprom ep;

  EEPROM.get(0, ep);

  delay(DELAY_INICIAL); // sistema é ligado na energia

  if(ZERAR)
    clear_eeprom();

  if(DEBUG) 
    Serial.begin(9600);

  /* pinMode's */
  pinMode(SIRENE, OUTPUT);

  for(int i=0; i<NUM_SENSOR; i++)
  {
    ep.sensor_chave[i]=true;
    sensor_status[i]=true;
    ep.potenciometro_ideal[i] = 540;
    pinMode(sensor_porta[i], INPUT);
    pinMode(potenciometro_porta[i], INPUT);
  }
  EEPROM.put(0, ep);

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
  while(millis()-time1 < TEMPO_PROCESSAMENTO)
    delay(1);
}
/* ----- Pós void setup & loop ----- */
void menu_iniciar(void) // função que lança no display o que o sensor esta captando no momento (sensor de som e o potenciomentro) ~ sinal
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
void ler_sensor(void) // sinal irá receber porta, para o sensor e o potenciometro, SINAL = PORTA
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

  for(int i = 0; i < TAMANHO_VETOR - (int)TAMANHO_VETOR * PORCENT; i++)
    soma += nun[i];

  return soma/(TAMANHO_VETOR - (int)TAMANHO_VETOR * PORCENT);
}

/* Reviveu kkkk */
void adicionar_vetor(void) // preencher o vetor com cada endereço a media_sala daquele respectivo momento, e sempre atualizando a cada nova interação
{
  verificar_intervalo();

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

bool analisar_barulho(void) // decide se vai acionar ou nao...      
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

void sirene(void)
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
  int i;                      //caso algum desses nao esteja correto, ele nao permite que a leitura desse sensor
  t_eeprom ep;                //entre na media da sala

  EEPROM.get(0, ep);

  for(i=0; i<NUM_SENSOR; i++)
    if(ep.sensor_chave[i])
      if(sensor_sinal[i]<20 || sensor_sinal[i]>950)
        sensor_status[i] = false;
      else
        if( potenciometro_sinal[i] > (ep.potenciometro_ideal[i] + TOLERANCIA_POTENCIOMETRO)  || potenciometro_sinal[i] < (ep.potenciometro_ideal[i] - TOLERANCIA_POTENCIOMETRO))
          sensor_status[i]=false;
        else
          sensor_status[i] = true;

  return;
}


