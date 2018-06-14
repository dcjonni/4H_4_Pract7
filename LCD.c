#include <18F4620.h>

#fuses HS, NOFCMEN, NOIESO, PUT, NOBROWNOUT, NOWDT
#fuses NOPBADEN, STVREN, NOLVP, NODEBUG,

#device ADC  = 10

#use delay(clock=16000000)
#use RS232(stream=uart, baud=9600, xmit=pin_c6, rcv=pin_c7, bits=8, parity=N, stop=1)

#define LCD_ENABLE_PIN    PIN_D2
#define LCD_RS_PIN        PIN_D3
#define LCD_RW_PIN        PIN_D1
#define LCD_DATA4         PIN_D4
#define LCD_DATA5         PIN_D5
#define LCD_DATA6         PIN_D6
#define LCD_DATA7         PIN_D7

#define LCD_EN_Direction  TRISD2_bit;
#define LCD_RS_Direction  TRISD3_bit;
#define LCD_D4_Direction  TRISD4_bit;
#define LCD_D5_Direction  TRISD5_bit;
#define LCD_D6_Direction  TRISD6_bit;
#define LCD_D7_Direction  TRISD7_bit;
#include <lcd.c>
#include "DHT11.h"

unsigned char humedadEntera_Byte1, humedadDecimal_Byte2;
unsigned char temperaturaEntera_Byte3, temperaturaDecimal_Byte4;
unsigned char checksum_Byte5;
int timeSerial = 10, contador, flagSerial=0, contadorDelayDht11, flagDht11;
int16 humedadEntera=0, humedadDecimal=0;
int16 temperaturaEntera=0, temperaturaDecimal=0;

void readData(int16 *humedadERead, int16 *humedadDRead, int16 *temperaturaERead, int16 *temperaturaDRead);
void sendToSerial(int16 humedadSerial, int16 humedadSerialD, int16 temperaturaSerial, int16 temperaturaSerialD, int16 dataAdc1);
void mainMenu();
void warning();
void TiempoActualizacionSerial();
void TiempoActualizacionLcd();
void configuracionAlarma();

int btnEnter, btnEsc, btnUp, btnDown, adc, flagChannel0, flagChannel1;
int rows=1;
int16 dataAdc;

#INT_TIMER0
void timer0_isr(void){
   contador++;
   contadorDelayDht11++;
   if(contador==timeSerial){
      contador=0;
      flagSerial=1;
   }
   if(contadorDelayDht11==10){
      contadorDelayDht11=0;
      flagDht11=1;
   }
   set_timer0 (0x3CB0);
}

#INT_AD
void interruptAdc_isr(void){
   dataAdc=read_adc(adc_read_only);
}

#INT_RB
void interruptRb_isr(void){
   if(input(PIN_B4)==1)// Enter
      btnEnter=1;
   if(input(PIN_B5)==1)// Esc
      btnEsc=1;
   if(input(PIN_B6)==1)// Arriba
      btnUp=1;
   if(input(PIN_B7)==1)// Abajo
      btnDown=1;
}

void main(){
   set_tris_a(0xFF);
   set_tris_b(0xF0);
   set_tris_c(0x10);
   set_tris_D(0x00);
   setup_oscillator(OSC_16MHZ);
   
   Setup_ADC(adc_clock_internal | ADC_TAD_MUL_4);
   Setup_ADC_PORTS(AN0_to_AN1);
   
   setup_timer_0(RTCC_INTERNAL | RTCC_DIV_8/* | RTCC_8_BIT*/);
   set_timer0 (0x3CB0);
   
   enable_interrupts(INT_AD);
   enable_interrupts(INT_RB);
   enable_interrupts(INT_TIMER0);
   enable_interrupts(GLOBAL);
   
   int position=1;
   lcd_init();
   
   while(true){
      readData(&humedadEntera, &humedadDecimal, &temperaturaEntera, &temperaturaDecimal);
      //warning();
      sendToSerial(humedadEntera, humedadDecimal, temperaturaEntera, temperaturaDecimal, dataAdc);
      //lcd_gotoxy(1,rows); printf(lcd_putc,"->");
      //delay_ms(500);
      //printf(lcd_putc,"\f");
      //printf(lcd_putc, "Temp. %Ld.%Ld%cC ",temperaturaEntera, temperaturaDecimal, 42);
      //lcd_gotoxy(2,2);
      //printf(lcd_putc, "Humedad %Ld.%Ld%c ",humedadEntera, humedadDecimal, '%');
      //delay_ms(100);
      //printf(lcd_putc,"\f");
      /*lcd_gotoxy(2,1);
      printf(lcd_putc, "Gas GLP %Ldppm", dataAdc);
      lcd_gotoxy(2,2);
      printf(lcd_putc, "Gas hidrogeno ");*/
      //printf(lcd_putc, "%d", vec[p]);
      if(position==1){
         printf(lcd_putc,"\f");
         lcd_gotoxy(1,rows); printf(lcd_putc,">");
         lcd_gotoxy(2,rows); printf(lcd_putc, "humedad     ");
         //lcd_gotoxy(2,rows+1); printf(lcd_putc, "temp     ");
      }
      if(position==2){
         printf(lcd_putc,"\f");
         lcd_gotoxy(1,rows); printf(lcd_putc,">");
         lcd_gotoxy(2,rows); printf(lcd_putc, "temp     ");
         //lcd_gotoxy(2,rows-1); printf(lcd_putc, "humedad       ");
      }
      if(position==3){
         printf(lcd_putc,"\f");
         lcd_gotoxy(1,rows); printf(lcd_putc,">");
         lcd_gotoxy(2,rows); printf(lcd_putc, "gas x   ");
      }
      if(position==4){
         printf(lcd_putc,"\f");
         lcd_gotoxy(1,rows); printf(lcd_putc,">");
         lcd_gotoxy(2,rows); printf(lcd_putc, "gas y  ");
      }
      
      if(btnUp){
         btnUp=0;
         rows--;
         position--;
         if(position==0){
            position=4;
            rows=2;
         }
         if(rows==0){
            rows=1;
         }
      }
      if(btnDown){
         btnDown=0;
         rows++;
         position++;
         if(position==5){
            position=1;
            rows=1;
         }
         if(rows==3){
            rows=2;
         }
      }
      if(btnEnter==1){
         btnEnter=0;
         mainMenu();
      }
   }
}

void readData(int16 *humedadERead, int16 *humedadDRead, int16 *temperaturaERead, int16 *temperaturaDRead){
   set_adc_channel(0);
   read_adc(adc_start_only);
     
   if(flagDht11==1){
      flagDht11=0;
      startSignal();
      if(check_Response()){
         humedadEntera_Byte1 = readData();
         humedadDecimal_Byte2 = readData();
         temperaturaEntera_Byte3 = readData();
         temperaturaDecimal_Byte4 = readData();
         checksum_Byte5 = readData();
         if(checksum_Byte5 == ((humedadEntera_Byte1+humedadDecimal_Byte2+temperaturaEntera_Byte3+temperaturaDecimal_Byte4) & 0xFF)){
            *humedadERead = humedadEntera_Byte1;
            *humedadDRead = humedadDecimal_Byte2;
            *temperaturaERead = temperaturaEntera_Byte3;
            *temperaturaDRead = temperaturaDecimal_Byte4;
         }
      }
   }
}

void sendToSerial(int16 humedadSerial, int16 humedadSerialD, int16 temperaturaSerial, int16 temperaturaSerialD, int16 dataAdc1){
   
   if(flagSerial==1){
      flagSerial=0;
      printf("\f");
      printf("\rHumedad: %Ld.%Ld %c",humedadSerial, humedadSerialD, '%');
      printf("\rTemperatura: %Ld.%Ld %cC",temperaturaSerial, temperaturaSerialD, 248);
      printf("\rGas GLP: %Ld ppm", dataAdc1);
   }
   
}

void mainMenu(){

   int variable=1;
   while (!btnEsc){
      readData(&humedadEntera, &humedadDecimal, &temperaturaEntera, &temperaturaDecimal);
      //warning();
      sendToSerial(humedadEntera, humedadDecimal, temperaturaEntera, temperaturaDecimal, dataAdc);
      if(variable==1){
         lcd_gotoxy(1,1);
         printf(lcd_putc, ">Config. Serial   ");
         lcd_gotoxy(1,2);
         printf(lcd_putc, " Config. Lcd     ");
      }
      if(variable==2){
         lcd_gotoxy(1,1);
         printf(lcd_putc, " Config. Serial    ");
         lcd_gotoxy(1,2);
         printf(lcd_putc, ">Config. Lcd     ");
      }
      if(variable==3){
         lcd_gotoxy(1,1);
         printf(lcd_putc, " Config. Lcd     ");
         lcd_gotoxy(1,2);
         printf(lcd_putc, ">Config. Alarma ");
      }
      if(btnEnter){
         btnEnter=0;
         switch(variable){
            case 1: // configuracion serial
               TiempoActualizacionSerial();
            break;
            case 2: // configuracion LCD
               TiempoActualizacionLcd();
            break;
            case 3: // configuracion de alarma
               configuracionAlarma();
            break;
         }
      }
      if(btnDown){
         btnDown=0;
         variable++;
         if(variable==4){
            variable=1;
         }
      }
      if(btnUp){
         btnUp=0;
         variable--;
         if(variable==0){
            variable=3;
         }
      }
   }
   btnEsc=0;
}

void TiempoActualizacionSerial(){
   
   while (!btnEsc){
      lcd_gotoxy(1,1);
      printf(lcd_putc, "      serial         ");
      lcd_gotoxy(1,2);
      printf(lcd_putc, "      serial         ");
      delay_ms(500);
      readData(&humedadEntera, &humedadDecimal, &temperaturaEntera, &temperaturaDecimal);
      //warning();
      sendToSerial(humedadEntera, humedadDecimal, temperaturaEntera, temperaturaDecimal, dataAdc);
      /*int variableTemporalSerial; //inicializada al mismo valor que timeSerial
      lcd_gotoxy(1,1);
      printf(lcd_putc,"Tiempo serial: %d", variableTemporalSerial);
      if(btnUp){
         btnUp=0;
         variableTemporalSerial+=100;
         if(variableTemporaSerial<5000){
            variableTemporaSerial=5000;
            lcd_gotoxy(1,2);
            printf(lcd_putc,"Maximo valor");
         }
      }
      if(btnUp){
         btnUp=0;
         variableTemporalSerial+=100;
         if(variableTemporaSerial<5000){
            variableTemporaSerial=5000;
            lcd_gotoxy(1,2);
            printf(lcd_putc,"Maximo valor");
         }*/
   }
   //variableTemporalSerial = timeSerial;
   btnEsc=0;
}

void TiempoActualizacionLcd(){
   
   while(!btnEsc){
      lcd_gotoxy(1,1);
      printf(lcd_putc, "      lcd         ");
      lcd_gotoxy(1,2);
      printf(lcd_putc, "      lcd         ");
      delay_ms(500);
      readData(&humedadEntera, &humedadDecimal, &temperaturaEntera, &temperaturaDecimal);
      //warning();
      sendToSerial(humedadEntera, humedadDecimal, temperaturaEntera, temperaturaDecimal, dataAdc);
      /*int variableTemporalLcd; //inicializada al mismo valor que timeLcd
      lcd_gotoxy(1,1);
      printf(lcd_putc,"Tiempo lcd: %d", variableTemporalLcd);
      
      if(btnUp){
         btnUp=0;
         variableTemporalLcd+=100;
         if(variableTemporalLcd<2000){
            variableTemporalLcd=2000;
            lcd_gotoxy(1,2);
            printf(lcd_putc,"Maximo valor");
         }
      }
      if(btnDown){
         btnDown=0;
         variableTemporalLcd-=100;
         if(variableTemporalLcd>100){
            variableTemporalLcd=100;
            lcd_gotoxy(1,2);
            printf(lcd_putc,"Manimo valor");
         }
      }
      if(btnEnter){
         btnEnter=0;
         btnEsc=1;
         timeLcd = variableTemporalLcd;
      }*/
   }
   //variableTemporalLcd = timeLcd;
   btnEsc=0;
}

void configuracionAlarma(){

   while (!btnEsc){
      lcd_gotoxy(1,1);
      printf(lcd_putc, "      alarma      ");
      lcd_gotoxy(1,2);
      printf(lcd_putc, "      alarma      ");
      delay_ms(500);
      readData(&humedadEntera, &humedadDecimal, &temperaturaEntera, &temperaturaDecimal);
      //warning();
      sendToSerial(humedadEntera, humedadDecimal, temperaturaEntera, temperaturaDecimal, dataAdc);
      /*switch(variableAlarma){
         case 1:
            lcd_gotoxy(1,1);
            printf(lcd_putc,"Temperatura");
            configuracionAlertTemp();
         break;
         case 2:
            lcd_gotoxy(1,1);
            printf(lcd_putc,"Humedad");
            configuracionAlertHumedad();
         break;
         case 3:
            lcd_gotoxy(1,1);
            printf(lcd_putc,"GasX");
            configuracionAlertGlp();
         break;
         case 4:
            lcd_gotoxy(1,1);
            printf(lcd_putc,"GasY");
            configuracionAlertHydro();
         break;
      }*/
   }
   btnEsc=0;
}

/*void configuracionAlertTemp(){

   int variableTempAlertTemp;
   while(!btnEsc){
      readData(&humedadEntera, &humedadDecimal, &temperaturaEntera, &temperaturaDecimal);
      warning();
      sendToSerial(humedadEntera, humedadDecimal, temperaturaEntera, temperaturaDecimal, dataAdc);
      lcd_gotoxy(1,1);
      printf(lcd_putc,"Alarm Temp: %dC", variableTempAlertTemp);
      if(btnUp){
         btnUp=0;
         variableTempAlertTemp+=10; // max 50
         if(variableTempAlertTemp<50){
            variableTempAlertTemp=50;
            lcd_gotoxy(1,2);
            printf(lcd_putc,"Maximo valor");
         }
      }
      if(btnDown){
         btnDown=0;
         variableTempAlertTemp-=10;
         if(variableTempAlertTemp>10){
            variableTempAlertTemp=10;
            lcd_gotoxy(1,2);
            printf(lcd_putc,"Manimo valor");
         }
      }
      if(btnEnter){
         btnEnter=0;
         btnEsc=1;
         alarmaTemperatura=variableTempAlertTemp;
      }
   }
   variableTempAlertTemp=alarmaTemperatura;
   btnEsc=0;
   configuracionAlarma();
}*/
/*void configuracionAlertHumedad(){

   int variableTempAlertHumedad;
   while(!btnEsc){
      readData(&humedadEntera, &humedadDecimal, &temperaturaEntera, &temperaturaDecimal);
      warning();
      sendToSerial(humedadEntera, humedadDecimal, temperaturaEntera, temperaturaDecimal, dataAdc);
      lcd_gotoxy(1,1);
      printf(lcd_putc,"Alarm Hum: %d%c", variableTempAlertHumedad, '%');
      if(btnUp){
         btnUp=0;
         variableTempAlertHumedad+=10; // max 90
         if(variableTempAlertHumedad<90){
            variableTempAlertHumedad=90;
            lcd_gotoxy(1,2);
            printf(lcd_putc,"Maximo valor");
         }
      }
      if(btnDown){
         btnDown=0;
         variableTempAlertTemp-=10;
         if(variableTempAlertTemp>10){
            variableTempAlertTemp=10;
            lcd_gotoxy(1,2);
            printf(lcd_putc,"Manimo valor");
         }
      }
      if(btnEnter){
         btnEnter=0;
         btnEsc=1;
         alarmaHumedad=variableTempAlertHumedad;
      }
   }
   variableTempAlertHumedad=alarmaHumedad;
   btnEsc=0;
   configuracionAlarma();
   
}*/
/*void configuracionAlertGlp(){

   int variableTempAlertGlp;
   while(!btnEsc){
      readData(&humedadEntera, &humedadDecimal, &temperaturaEntera, &temperaturaDecimal);
      warning();
      sendToSerial(humedadEntera, humedadDecimal, temperaturaEntera, temperaturaDecimal, dataAdc);
      lcd_gotoxy(1,1);
      printf(lcd_putc,"Alarm Glp: %dppm", variableTempAlertGlp);
      if(btnUp){
         btnUp=0;
         variableTempAlertGlp+=100;
         //if(variableTempAlertGlp<90){ //max desconocido
           // variableTempAlertGlp=90;
           // lcd_gotoxy(1,2);
           // printf(lcd_putc,"Maximo valor");
         //}
      }
      if(btnDown){
         btnDown=0;
         variableTempAlertGlp-=100;
         //if(variableTempAlertGlp>10){ //minimo desconocido
            //variableTempAlertGlp=10;
            //lcd_gotoxy(1,2);
            //printf(lcd_putc,"Manimo valor");
         //}
      }
      if(btnEnter){
         btnEnter=0;
         btnEsc=1;
         alarmaHumedad=variableTempAlertGlp;
      }
   }
   variableTempAlertGlp=alarmaGlp;
   btnEsc=0;
   configuracionAlarma();
   
}*/
/*void configuracionAlertHydro(){

   int variableTempAlertHydro;
   while(!btnEsc){
      readData(&humedadEntera, &humedadDecimal, &temperaturaEntera, &temperaturaDecimal);
      warning();
      sendToSerial(humedadEntera, humedadDecimal, temperaturaEntera, temperaturaDecimal, dataAdc);
      lcd_gotoxy(1,1);
      printf(lcd_putc,"Alarm Hydro: %dppm", variableTempAlertHydro);
      if(btnUp){
         btnUp=0;
         variableTempAlertHydro+=100;
         //if(variableTempAlertHydro<90){ //max desconocido
           // variableTempAlertHydro=90;
           // lcd_gotoxy(1,2);
           // printf(lcd_putc,"Maximo valor");
         //}
      }
      if(btnDown){
         btnDown=0;
         variableTempAlertHydro-=100;
         //if(variableTempAlertHydro>10){ //minimo desconocido
            //variableTempAlertHydro=10;
            //lcd_gotoxy(1,2);
            //printf(lcd_putc,"Manimo valor");
         //}
      }
      if(btnEnter){
         btnEnter=0;
         btnEsc=1;
         alarmaHydrod=variableTempAlertHydro;
      }
   }
   variableTempAlertHydro=alarmaHydro;
   btnEsc=0;
   configuracionAlarma();

}*/

void warning(){
   /*if(alarmaTemperatura == temperaturaEntera){
      output_b(0x01);
   }
   if(alarmaHumedad == humedadEntera){
      output_b(0x02);
   }
   if(alarmaGlp == gasGlp){
      output_b(0x04);
   }
   if(alarmaHydro == gasHydro){
      output_b(0x08);
   }*/
}
