#include <LiquidCrystal.h> 
#define NUM_SOM 4
#define NUM_SENSOR 3
#define ESPACO 5

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);  // Declaração do objeto tipo lcd

// Definição dos pinos dos botões
#define menu    9 
#define change  7  
#define up      6  
#define down    8

#define menu0   90  // Valor de referência que a
#define change0 91  // função check() passa
#define up0     92  // indicando que um botão foi
#define down0   93  // solto

#define NIVEL_LIMITE 180

int limite_acionar = NIVEL_LIMITE;  // variavel a ser alterada pelo menu
char state = 1;  // variável que guarda posição atual do menu
int setor = 1; //vc

short potenciometro_porta[NUM_SENSOR] = {A5, A4, A3};
short potenciometro_sinal[NUM_SENSOR] = {0}; // Potenciometros ligados às portas analógicas
int   potenciometro_ideal[NUM_SENSOR] = {60, 60, 60};    // Valor ideal do potenciometro
short aux[NUM_SENSOR] = {0};

bool aMenu, aChange, aUp, aDown;  // Grava o ultimo valor lidos nos botões. Utilizado pela função check p/ identificar quando há uma alteração no estado do pino dos botões

//============================================== SETUP
void setup()
{
  lcd.begin(16, 2);  // Iniciando a biblioteca do LCD

  pinMode(menu,  INPUT);   // Botões
  pinMode(change,INPUT);
  pinMode(up,    INPUT);
  pinMode(down,  INPUT);

  for (int i = 0; i < NUM_SENSOR; i++)
    pinMode(potenciometro_porta[i], INPUT);
}

//============================================== LOOP
void loop()
{
  lcd.clear();
  while(1)
  {
    lcd.setCursor(0,0);
    lcd.print("                ");
    for(int i = 0; i < NUM_SENSOR; i++) // 0,4,8,12
    {
      if(!i)
      {
        lcd.setCursor(i,0);
        lcd.print(potenciometro_sinal[i]);
      }
      else
      {
        lcd.setCursor(i + ESPACO*i,0);
        lcd.print(potenciometro_sinal[i]);
      }
    }

    lcd.setCursor(0,1);
    lcd.print("2 - CONFIGURACAO");
    if(digitalRead(menu))
    {
      lcd.clear();
      while(1)
        poop();
    }
  }
}

void ler_sensor()
{
  for(int i = 0; i < NUM_SENSOR; i++)
    potenciometro_sinal[i] = analogRead(potenciometro_porta[i]);
}

//============================================== checar
void checar()
{
  switch(check())
  {
    case change:
      delay(500);
      loop();
      break;
    case up:
      lcd.clear()// Antes de mudar de tela, é necessário limpar o display com a função lcd.clear().;
      if(state <= 1 )// se ele der  up na primeira ele vai pra ultima
        state = 5;
      else
        state--;
      break;
    case down:
      lcd.clear()// Antes de mudar de tela, é necessário limpar o display com a função lcd.clear().;
      if(state >=5 )//se ele chegar na ultima volta pra primeira
        state = 1;
      else
        state++
      break;
    case menu:// os enters devem ser adicionados aqui 
      lcd.clear();
      /*set_state(6);
        lcd.print("         ");*/
      set_state(state);
      if(state == 5)
        state=6;
      break;

  }

  set_state(state);


}

//============================================== POOP
void poop()
{

  if(state > 0 && state < 6)
    checar();
  else if(state == 6)
    switch(check())
    {
      if(setor == 1)
        setor = 0;
      case menu:

      potenciometro_ideal[setor]++;
      set_state(6);
      break;
      case change:
      potenciometro_ideal[setor]--;
      set_state(6);
      break;
      case down:
      if(setor > 3)
        setor = 0;
      else
        setor++;
      set_state(6); // Antes de mudar de tela, é necessário limpar o display com a função lcd.clear().
      break;
      case up:
      if(setor < 0)
        setor = 3;
      else
        setor--;
      set_state(6);
      break;

    }        

  set_state(state);


}

//============================================== set_state
void set_state(char index) 
{
  int setor2;

  if(setor > 3)
    setor2 = 0;/*´pra setar a posição de cada 60. como começa em 5  muda pra 0*/
  else
    setor2 = setor + setor*ESPACO;

  state = index; // Atualiza a variável state para a nova tela
  switch(state) // verifica qual a tela atual e exibe o conteúdo correspondente
  {
    case 1: //==================== state 1
      lcd.setCursor(0,0);
      lcd.print("Potenciometro 1");
      lcd.setCursor(0,1);
      lcd.print("     ");
      lcd.setCursor(0,1);
      aux[0] = analogRead(potenciometro_porta[0]);
      lcd.print(aux[0]);
      break;
    case 2: //==================== state 2
      lcd.setCursor(0,0);
      lcd.print("Potenciometro 2");
      lcd.setCursor(0,1);
      lcd.print("     ");
      lcd.setCursor(0,1);
      aux[1] = analogRead(potenciometro_porta[1]);
      lcd.print(aux[1]);
      break;
    case 3: //==================== state 3
      lcd.setCursor(0,0);
      lcd.print("Potenciometro 3");
      lcd.setCursor(0,1);
      lcd.print("     ");
      lcd.setCursor(0,1);
      aux[2] = analogRead(potenciometro_porta[2]);
      lcd.print(aux[2]);
      break;
    case 4: //==================== state 4
      lcd.setCursor(0,0);
      lcd.print("Limite Acionar");
      lcd.setCursor(0,1);
      lcd.print(limite_acionar);  // mostra o valor de "variavel"
      break;
    case 5: //==================== state 5
      lcd.setCursor(0,0);
      lcd.print("Poten ideal");
      for(int i = 0; i < NUM_SENSOR; i++) // 0,4,8,12
      {
        if(!i)
        {
          lcd.setCursor(i + 1,1);
          lcd.print(potenciometro_ideal[i]);
        }
        else
        {
          lcd.setCursor(i + ESPACO*i + 1,1);
          lcd.print(potenciometro_ideal[i]);
        }
      }  // mostra o valor de "variavel"
      break;
    case 6: //==================== state 5.1
      lcd.setCursor(0,0);
      lcd.print("Ajustando...");
      for(int i = 0; i < NUM_SENSOR; i++) // 0,4,8,12
      {
        if(!i)
        {
          lcd.setCursor(i + 1,1);
          lcd.print(potenciometro_ideal[i]);
        }
        else
        {
          lcd.setCursor(i + ESPACO*i + 1,1);
          lcd.print(potenciometro_ideal[i]);
        }
      }  // mostra o valor de "variavel"
      lcd.setCursor(setor2,1);
      lcd.print(potenciometro_ideal[setor]);  // mostra o valor de "variavel"
      break;
    default: ;
  }
}

//=============================================== mudar
int mudar(int atual)
{
  int nova = atual;
  nova++;
  return nova;
}

//============================================== check
char check() 
{
  if(aMenu != digitalRead(menu)) 
  {
    aMenu = !aMenu;
    if(aMenu) 
      return menu0; 
    else 
      return menu;
  } 
  else if(aChange != digitalRead(change))
  {
    aChange = !aChange;
    if(aChange) 
      return change0; 
    else 
      return change;
  } 
  else if(aUp != digitalRead(up))
  {
    aUp = !aUp;
    if(aUp) 
      return up0; 
    else 
      return up;
  } 
  else if(aDown != digitalRead(down)) 
  {
    aDown = !aDown;
    if(aDown) 
      return down0; 
    else 
      return down;
  } 
  else
    return 0;
}
