#include <18F4620.h>

#fuses HS, NOFCMEN, NOIESO, PUT, NOBROWNOUT, NOWDT
#fuses NOPBADEN, STVREN, NOLVP, NODEBUG,

#device ADC  = 10

#use delay(clock=16000000)
#use RS232(stream=uart, baud=9600, xmit=pin_c6, rcv=pin_c7, bits=8, parity=N, stop=1)

#define LEDTEMPERATURE PIN_B0
#define LEDHUMIDITY PIN_B1
#define LEDLPG PIN_B2
#define LEDHYDROGEN PIN_B3

#include "lcd.c"
#include "DHT11.h"

unsigned char humidityIntegral_Byte1, humidityDecimal_Byte2;
unsigned char temperatureIntegral_Byte3, temperatureDecimal_Byte4;
unsigned char checksum_Byte5;
int timeSerial = 10, counterSerial, flagSerial=0, counterDelayDht11, flagDht11, timeLcd=10, counterLcd, flagLcd;
int btnEnter, btnEsc, btnUp, btnDown, flagChannel0, flagChannel1;
int16 alertTemperature=40, alertHumidity=40, alertLpg=500, alertHydrogen;
int16 humidityIntegral=0, humidityDecimal=0, temperaturaIntegral=0, temperatureDecimal=0, dataAdc;
int16 humidityInLcd, humidityDLcd, temperaturaInLcd, temperatureDLcd, dataAdcLcd;

int16 dataLpg, dataHydrogen;

void readData(int16 *humidityInRead, int16 *humidityDRead, int16 *temperatureInRead, int16 *temperatureDRead, int16 *dataLpg, int16 *dataHydrogen, int16 dataAdc);
void sendToSerial(int16 humiditySerial, int16 humiditySerialD, int16 temperatureSerial, int16 temperatureSerialD, int16 dataAdc1);
void mainMenu();
void warning(int16 alertTemperature, int16 temperaturaIntegral, int16 alertHumidity, int16 humidityIntegral, int16 alertLpg, int16 dataAdc);
void timeUpdateSerial(int *timeSerial);
void timeUpdateLcd(int *timeLcd);
void configurationAlert();
void configurationAlertTemp(int16 *alertTemperature);
void configurationAlertHumidity(int16 *alertHumidity);
void configurationAlertLpg(int16 *alertLpg);
void updateLcd(int16 humidityIntegral,int16 humidityDecimal,int16 temperaturaIntegral,int16 temperatureDecimal,int16 dataAdc,int16 *humidityInLcd,int16 *humidityDLcd,int16 *temperaturaInLcd,int16 *temperatureDLcd,int16 *dataAdcLcd);

#INT_TIMER0
void timer0_isr(void){
   counterSerial++;
   counterLcd++;
   counterDelayDht11++;
   if(counterSerial==timeSerial){
      counterSerial=0;
      flagSerial=1;
   }
   if(counterDelayDht11==10){
      counterDelayDht11=0;
      flagDht11=1;
   }
   if(counterLcd==timeLcd){
      counterLcd=0;
      flagLcd=1;
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
   
   lcd_init();
 
   Setup_ADC(adc_clock_internal | ADC_TAD_MUL_4);
   Setup_ADC_PORTS(AN0_to_AN1);
   
   setup_timer_0(RTCC_INTERNAL | RTCC_DIV_8);
   set_timer0 (0x3CB0);
   
   enable_interrupts(INT_AD);
   enable_interrupts(INT_RB);
   enable_interrupts(INT_TIMER0);
   enable_interrupts(GLOBAL);
   
   int position=1;
   set_adc_channel(0);
   flagChannel0=1;
   
   while(true){
      readData(&humidityIntegral, &humidityDecimal, &temperaturaIntegral, &temperatureDecimal, &dataLpg, &dataHydrogen, dataAdc);
      //warning();
      sendToSerial(humidityIntegral, humidityDecimal, temperaturaIntegral, temperatureDecimal, dataLpg);
      updateLcd(humidityIntegral,humidityDecimal,temperaturaIntegral,temperatureDecimal,dataAdc, &humidityInLcd,&humidityDLcd,&temperaturaInLcd,&temperatureDLcd,&dataAdcLcd);
      if(position==1){
         lcd_gotoxy(1,1); printf(lcd_putc, ">Humedad %Ld.%Ld%c  ",humidityInLcd, humidityDLcd, '%');
         lcd_gotoxy(1,2); printf(lcd_putc, " Temp. %Ld.%LdC  ",temperaturaInLcd, temperatureDLcd);
      }
      if(position==2){
         lcd_gotoxy(1,1); printf(lcd_putc, " Humedad %Ld.%Ld%c   ",humidityInLcd, humidityDLcd, '%');
         lcd_gotoxy(1,2); printf(lcd_putc, ">Temp. %Ld.%LdC  ",temperaturaInLcd, temperatureDLcd);
      }
      if(position==3){
         lcd_gotoxy(1,1); printf(lcd_putc, ">Gas GLP %Ldppm  ", dataLpg);
         lcd_gotoxy(1,2); printf(lcd_putc, " Gas hidro %Ld ", dataHydrogen);
      }
      if(position==4){
         lcd_gotoxy(1,1); printf(lcd_putc, " Gas GLP %Ldppm   ", dataLpg);
         lcd_gotoxy(1,2); printf(lcd_putc, ">Gas hidro %Ldppm  ", dataHydrogen);
      }
      
      if(btnUp){
         btnUp=0;
         position--;
         if(position==0){
            position=4;
         }
      }
      if(btnDown){
         btnDown=0;
         position++;
         if(position==5){
            position=1;
         }
      }
      if(btnEnter==1){
         btnEnter=0;
         mainMenu();
      }
   }
}

void updateLcd(int16 humidityIntegral,int16 humidityDecimal,int16 temperaturaIntegral,int16 temperatureDecimal,int16 dataAdc,int16 *humidityInLcd,int16 *humidityDLcd,int16 *temperaturaInLcd,int16 *temperatureDLcd,int16 *dataAdcLcd){

   if(flagLcd==1){
      flagLcd=0;
      *humidityInLcd = humidityIntegral;
      *humidityDLcd = humidityDecimal;
      *temperaturaInLcd = temperaturaIntegral;
      *temperatureDLcd = temperatureDecimal;
      *dataAdcLcd = dataAdc;
   }

}

void readData(int16 *humidityInRead, int16 *humidityDRead, int16 *temperatureInRead, int16 *temperatureDRead, int16 *dataLpg, int16 *dataHydrogen, int16 dataAdc){
   if(flagChannel0==1 && adc_done()==1){
      flagChannel0=0;
      set_adc_channel(1);
      *dataLpg = dataAdc;
      read_adc(adc_start_only);
      flagChannel1=1;
   }
   if(flagChannel1==1  && adc_done()==1){
      flagChannel1=0;
      set_adc_channel(0);
      read_adc(adc_start_only);
      *dataHydrogen = dataAdc;
      flagChannel0=1;
   }
        
   if(flagDht11==1){
      flagDht11=0;
      startSignal();
      if(check_Response()){
         humidityIntegral_Byte1 = readData();
         humidityDecimal_Byte2 = readData();
         temperatureIntegral_Byte3 = readData();
         temperatureDecimal_Byte4 = readData();
         checksum_Byte5 = readData();
         if(checksum_Byte5 == ((humidityIntegral_Byte1+humidityDecimal_Byte2+temperatureIntegral_Byte3+temperatureDecimal_Byte4) & 0xFF)){
            *humidityInRead = humidityIntegral_Byte1;
            *humidityDRead = humidityDecimal_Byte2;
            *temperatureInRead = temperatureIntegral_Byte3;
            *temperatureDRead = temperatureDecimal_Byte4;
         }
      }
   }
}

void sendToSerial(int16 humiditySerial, int16 humiditySerialD, int16 temperatureSerial, int16 temperatureSerialD, int16 dataAdc1){
   
   if(flagSerial==1){
      flagSerial=0;
      printf("\f");
      printf("\rHumedad: %Ld.%Ld %c",humiditySerial, humiditySerialD, '%');
      printf("\rTemperatura: %Ld.%Ld %cC",temperatureSerial, temperatureSerialD, 248);
      printf("\rGas GLP: %Ld ppm", dataAdc1);
   }
   
}

void mainMenu(){

   int position=1;
   while (!btnEsc){
      readData(&humidityIntegral, &humidityDecimal, &temperaturaIntegral, &temperatureDecimal, &dataLpg, &dataHydrogen, dataAdc);
      //warning();
      sendToSerial(humidityIntegral, humidityDecimal, temperaturaIntegral, temperatureDecimal, dataAdc);
      if(position==1){
         lcd_gotoxy(1,1); printf(lcd_putc, ">Config. Serial   ");
         lcd_gotoxy(1,2); printf(lcd_putc, " Config. Lcd     ");
      }
      if(position==2){
         lcd_gotoxy(1,1); printf(lcd_putc, " Config. Serial    ");
         lcd_gotoxy(1,2); printf(lcd_putc, ">Config. Lcd     ");
      }
      if(position==3){
         lcd_gotoxy(1,1); printf(lcd_putc, " Config. Lcd     ");
         lcd_gotoxy(1,2); printf(lcd_putc, ">Config. Alarma ");
      }
      if(btnEnter){
         btnEnter=0;
         switch(position){
            case 1: // configuracion serial
               timeUpdateSerial(&timeSerial);
            break;
            case 2: // configuracion LCD
               timeUpdateLcd(&timeLcd);
            break;
            case 3: // configuracion de alarma
               configurationAlert();
            break;
         }
      }
      if(btnDown){
         btnDown=0;
         position++;
         if(position==4){
            position=1;
         }
      }
      if(btnUp){
         btnUp=0;
         position--;
         if(position==0){
            position=3;
         }
      }
   }
   btnEsc=0;
}

void timeUpdateSerial(int *timeSerial){
   
   int variableTemporarySerial = *timeSerial; //inicializada al mismo valor que timeSerial
   while (!btnEsc){
      readData(&humidityIntegral, &humidityDecimal, &temperaturaIntegral, &temperatureDecimal, &dataLpg, &dataHydrogen, dataAdc);
      //warning();
      sendToSerial(humidityIntegral, humidityDecimal, temperaturaIntegral, temperatureDecimal, dataAdc);
      lcd_gotoxy(1,1); printf(lcd_putc,"Tiempo S: %d00ms  ", variableTemporarySerial);
      lcd_gotoxy(1,2); printf(lcd_putc,"                      ");
      if(btnUp){
         btnUp=0;
         variableTemporarySerial++; // maximo 5 segundos
         if(variableTemporarySerial>50){
            variableTemporarySerial=50;
            lcd_gotoxy(1,2); printf(lcd_putc,"Maximo valor ");
            delay_ms(100);
         }
      }
      if(btnDown){
         btnDown=0;
         variableTemporarySerial--;
         if(variableTemporarySerial<1){
            variableTemporarySerial=1;
            lcd_gotoxy(1,2); printf(lcd_putc,"Manimo valor ");
            delay_ms(100);
         }
      }
      if(btnEnter){
         btnEnter=0;
         btnEsc=1;
         *timeSerial = variableTemporarySerial;
      }
   }
   variableTemporarySerial = *timeSerial;
   btnEsc=0;
}

void timeUpdateLcd(int *timeLcd){
   
   int variableTemporaryLcd = *timeLcd; //inicializada al mismo valor que timeLcd
   while(!btnEsc){
      readData(&humidityIntegral, &humidityDecimal, &temperaturaIntegral, &temperatureDecimal, &dataLpg, &dataHydrogen, dataAdc);
      //warning();
      sendToSerial(humidityIntegral, humidityDecimal, temperaturaIntegral, temperatureDecimal, dataAdc);
      lcd_gotoxy(1,1); printf(lcd_putc,"Tpo lcd: %d00ms  ", variableTemporaryLcd);
      lcd_gotoxy(1,2); printf(lcd_putc,"                      ");
      if(btnUp){
         btnUp=0;
         variableTemporaryLcd++;
         if(variableTemporaryLcd>20){
            variableTemporaryLcd=20;
            lcd_gotoxy(1,2); printf(lcd_putc,"Maximo valor");
            delay_ms(100);
         }
      }
      if(btnDown){
         btnDown=0;
         variableTemporaryLcd--;
         if(variableTemporaryLcd<1){
            variableTemporaryLcd=1;
            lcd_gotoxy(1,2); printf(lcd_putc,"Manimo valor");
            delay_ms(100);
         }
      }
      if(btnEnter){
         btnEnter=0;
         btnEsc=1;
         *timeLcd = variableTemporaryLcd;
      }
   }
   variableTemporaryLcd = *timeLcd;
   btnEsc=0;
}

void configurationAlert(){

   int position=1;
   while (!btnEsc){
      readData(&humidityIntegral, &humidityDecimal, &temperaturaIntegral, &temperatureDecimal, &dataLpg, &dataHydrogen, dataAdc);
      warning(alertTemperature, temperaturaIntegral, alertHumidity, humidityIntegral, alertLpg,dataAdc);
      sendToSerial(humidityIntegral, humidityDecimal, temperaturaIntegral, temperatureDecimal, dataAdc);
      if(position==1){
         lcd_gotoxy(1,1); printf(lcd_putc, ">Alarm. Temp   ");
         lcd_gotoxy(1,2); printf(lcd_putc, " Alarm. humedad  ");
      }
      if(position==2){
         lcd_gotoxy(1,1); printf(lcd_putc, " Alarm. Temp   ");
         lcd_gotoxy(1,2); printf(lcd_putc, ">Alarm. humedad  ");
      }
      if(position==3){
         lcd_gotoxy(1,1); printf(lcd_putc, ">Alarm. Glp      ");
         lcd_gotoxy(1,2); printf(lcd_putc, " Alarm. Hidrogeno");
      }
      if(position==4){
         lcd_gotoxy(1,1); printf(lcd_putc, " Alarm. Glp      ");
         lcd_gotoxy(1,2); printf(lcd_putc, ">Alarm. Hidrogeno");
      }
      if(btnEnter){
         btnEnter=0;
         switch(position){
            case 1: // configuracion alarma temperatura
               configurationAlertTemp(&alertTemperature);
            break;
            case 2: // configuracion alarma humedad
               configurationAlertHumidity(&alertHumidity);
            break;
            case 3: // configuracion alarma glp
               configurationAlertLpg(&alertLpg);
            break;
            case 4: // configuracion alarma hidrogeno
               //configurationAlertHydrogen();
               lcd_gotoxy(1,1);
               printf(lcd_putc, "Alarm hidrogeno   ");
               lcd_gotoxy(1,2);
               printf(lcd_putc, "Alarm hidrogeno      ");
               delay_ms(500);
            break;
         }
      }
      if(btnDown){
         btnDown=0;
         position++;
         if(position==5){
            position=1;
         }
      }
      if(btnUp){
         btnUp=0;
         position--;
         if(position==0){
            position=4;
         }
      }
   }
   btnEsc=0;
}

void configurationAlertTemp(int16 *alertTemperature){

   int variableTempoAlertTemp=*alertTemperature;
   while(!btnEsc){
      readData(&humidityIntegral, &humidityDecimal, &temperaturaIntegral, &temperatureDecimal, &dataLpg, &dataHydrogen, dataAdc);
      //warning();
      sendToSerial(humidityIntegral, humidityDecimal, temperaturaIntegral, temperatureDecimal, dataAdc);
      lcd_gotoxy(1,1); printf(lcd_putc,"Alarm Temp: %dC     ", variableTempoAlertTemp);
      lcd_gotoxy(1,2); printf(lcd_putc,"                     ");
      if(btnUp){
         btnUp=0;
         variableTempoAlertTemp+=5;
         if(variableTempoAlertTemp>50){
            variableTempoAlertTemp=50;
            lcd_gotoxy(1,2); printf(lcd_putc,"Maximo valor    ");
            delay_ms(100);
         }
      }
      if(btnDown){
         btnDown=0;
         variableTempoAlertTemp-=5;
         if(variableTempoAlertTemp<5){
            variableTempoAlertTemp=5;
            lcd_gotoxy(1,2); printf(lcd_putc,"Manimo valor     ");
            delay_ms(100);
         }
      }
      if(btnEnter){
         btnEnter=0;
         btnEsc=1;
         *alertTemperature=variableTempoAlertTemp;
      }
   }
   variableTempoAlertTemp=*alertTemperature;
   btnEsc=0;
   
}

void configurationAlertHumidity(int16 *alertHumidity){

   int variableTempoAlertHumidity = *alertHumidity;
   while(!btnEsc){
      readData(&humidityIntegral, &humidityDecimal, &temperaturaIntegral, &temperatureDecimal, &dataLpg, &dataHydrogen, dataAdc);
      //warning();
      sendToSerial(humidityIntegral, humidityDecimal, temperaturaIntegral, temperatureDecimal, dataAdc);
      lcd_gotoxy(1,1); printf(lcd_putc,"Alarm Hum: %d%c   ", variableTempoAlertHumidity, '%');
      lcd_gotoxy(1,2); printf(lcd_putc,"                    ");
      if(btnUp){
         btnUp=0;
         variableTempoAlertHumidity+=5; // max 90
         if(variableTempoAlertHumidity>90){
            variableTempoAlertHumidity=90;
            lcd_gotoxy(1,2); printf(lcd_putc,"Maximo valor");
            delay_ms(100);
         }
      }
      if(btnDown){
         btnDown=0;
         variableTempoAlertHumidity-=5;
         if(variableTempoAlertHumidity<5){
            variableTempoAlertHumidity=5;
            lcd_gotoxy(1,2); printf(lcd_putc,"Manimo valor");
            delay_ms(100);
         }
      }
      if(btnEnter){
         btnEnter=0;
         btnEsc=1;
         *alertHumidity = variableTempoAlertHumidity;
      }
   }
   variableTempoAlertHumidity = *alertHumidity;
   btnEsc=0;
   
}

void configurationAlertLpg(int16 *alertLpg){

   int16 variableTempAlertLgp = *alertLpg;
   while(!btnEsc){
      readData(&humidityIntegral, &humidityDecimal, &temperaturaIntegral, &temperatureDecimal, &dataLpg, &dataHydrogen, dataAdc);
      //warning();
      sendToSerial(humidityIntegral, humidityDecimal, temperaturaIntegral, temperatureDecimal, dataAdc);
      lcd_gotoxy(1,1); printf(lcd_putc,"Alar Glp:%ldppm     ", variableTempAlertLgp);
      lcd_gotoxy(1,2); printf(lcd_putc,"                        ");
      if(btnUp){
         btnUp=0;
         variableTempAlertLgp+=500;
         if(variableTempAlertLgp>10000){ //max 10 000 ppm
            variableTempAlertLgp=10000;
            lcd_gotoxy(1,2); printf(lcd_putc,"Maximo valor");
            delay_ms(100);
         }
      }
      if(btnDown){
         btnDown=0;
         variableTempAlertLgp-=500;
         if(variableTempAlertLgp<500){ //minimo 500 ppm
            variableTempAlertLgp=500;
            lcd_gotoxy(1,2); printf(lcd_putc,"Manimo valor");
            delay_ms(100);
         }
      }
      if(btnEnter){
         btnEnter=0;
         btnEsc=1;
         *alertLpg = variableTempAlertLgp;
      }
   }
   variableTempAlertLgp = *alertLpg;
   btnEsc=0;
   
}

/*void configuracionAlertHydrogen(){

   int variableTempAlertHydro = alertHydrogen;
   while(!btnEsc){
      readData(&humedadEntera, &humedadDecimal, &temperaturaEntera, &temperaturaDecimal, &dataLpg, &dataHydrogen);
      warning();
      sendToSerial(humedadEntera, humedadDecimal, temperaturaEntera, temperaturaDecimal, dataAdc);
      lcd_gotoxy(1,1);
      printf(lcd_putc,"Alarm Hydro: %dppm", variableTempAlertHydro);
      lcd_gotoxy(1,2); printf(lcd_putc,"                     ");
      if(btnUp){
         btnUp=0;
         variableTempAlertHydro+=500;
         //if(variableTempAlertHydro<10000){ //max desconocido
           // variableTempAlertHydro=10000;
           // lcd_gotoxy(1,2);
           // printf(lcd_putc,"Maximo valor");
           delay_ms(100);
         //}
      }
      if(btnDown){
         btnDown=0;
         variableTempAlertHydro-=500;
         //if(variableTempAlertHydro>500){ //minimo desconocido
            //variableTempAlertHydro=500;
            //lcd_gotoxy(1,2);
            //printf(lcd_putc,"Manimo valor");
            delay_ms(100);
         //}
      }
      if(btnEnter){
         btnEnter=0;
         btnEsc=1;
         alertHydrogen=variableTempAlertHydro;
      }
   }
   variableTempAlertHydro=alarmaHydro;
   btnEsc=0;

}*/

void warning(int16 alertTemperature, int16 temperaturaIntegral, int16 alertHumidity, int16 humidityIntegral, int16 alertLpg, int16 dataAdc){
   
   if(alertTemperature <= temperaturaIntegral){
      output_high(LEDTEMPERATURE);
   }
   if(alertTemperature > temperaturaIntegral){
      output_low(LEDTEMPERATURE);
   }
   if(alertHumidity <= humidityIntegral){
      output_high(LEDHUMIDITY);
   }
   if(alertHumidity > humidityIntegral){
      output_low(LEDHUMIDITY);
   }
   if(alertLpg <= dataAdc){
      output_high(LEDLPG);
   }
   if(alertLpg > dataAdc){
      output_low(LEDLPG);
   }
   if(alertHydrogen <= dataAdc){
      output_high(LEDHYDROGEN);
   }
   if(alertHydrogen > dataAdc){
      output_low(LEDHYDROGEN);
   }
   
}
