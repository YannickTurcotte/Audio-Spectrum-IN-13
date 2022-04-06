/*
 * Spectrum analyseur MSGEQ7.c
 *
 * Created: 2020-04-30 12:58:46
 * Author : https://www.instructables.com/member/Yannick99/instructables/
 */ 

#define F_CPU 8000000
#include <avr/io.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include "avr/interrupt.h"

#define STROBEPORT PORTD
#define RESETPORT PORTD
#define STROBEDDR DDRD
#define RESETDDR DDRD
#define SoundSenseDDR DDRD
#define SoundSensePORT PORTD	
#define StrobePin	PD0
#define ResetPin	PD1
#define SoundSensePin PD2
#define PushButtonPin PIND	
#define PushButton PD3
#define ConfigOutputPin PINB	
#define ConfigOutput PB1
#define ConfigOutputPort PORTB
#define ConfigOutputDDR DDRB
#define MSGEQ7_Osc_Port PORTD
#define MSGEQ7_Osc_DDR DDRD
#define MSGEQ7_OscPin PD6
#define Autooff 650000 // plus c'est gros plus c'est long (650000 = 50 secondes environ)
#define FreqOutPort PORTB
#define FreqOutDDR DDRB
#define FreqOutPin PB1
#define Invert PB3
#define InvertPort PORTB
#define InvertDDR DDRB
#define InvertPin PINB

//Notes:
// set a bit |=()
// clr a bit &=~()
// 	DDRC &=~1<<PINC5
//	loop_until_bit_is_set(PIND, SoundSensePin);

//Variable
volatile uint8_t i=0;
volatile uint8_t temp=0;
volatile uint8_t temp2=0;
volatile uint8_t FrequenceFlag = 1;
volatile long OffFlag = Autooff;
volatile uint8_t RepartAZeroFlag = 0;
volatile uint8_t ConfigModeFlag = 0;
volatile uint16_t RecenterFlag = 1984; //1984 pour une seconde envison. 1/(72usx7) Bonne frequence aux bonne place. Quelque fois quand ils sont tous allumer (trop fort) Trop de bruit et ils perdent le fil.


//sub routine declaration
void Send_Reset(void);
void Power_Supply_On(void);
void Power_Supply_Off(void);
void loop(void);
void ConfigMode(void);
void ConfigOutputHigh(void);
void ConfigOutputLow(void);

int main(void)
{	
	cli();
	STROBEDDR |=(1<<StrobePin); //pd0 and 1 output
	RESETDDR |=(1<<ResetPin);
	STROBEPORT &=~(1<<StrobePin); //0
	RESETPORT &=~(1<<ResetPin); //0
	MSGEQ7_Osc_DDR  |=(1<<MSGEQ7_OscPin); //pd6 sortie pour oscillateur
	DDRB = 0b11101111;	//portb sortie except pb4 for Always mode on
	PORTB = 0b00010000;
	ConfigOutputDDR |=(1<<ConfigOutputPin); //pb1 output square wave calibration
	ConfigOutputPort &=~(1<<ConfigOutput); //pb1 a 0
	
	DDRD |=(1<<PIND7); //Pd7 output for demux
	
//InvertPin init	
	InvertDDR &=~(1<<Invert); //port en entrée
	InvertPort |=(1<<Invert); // pull up
	
	FreqOutDDR |=(1<<FreqOutPin); // pb1 en sortie

//8-bit Timer/Counter0 160khz output on pd6. This is needed for the msgeq7 to operate
	TCCR0A |=(1<<WGM01)|(1<<COM0A0); //ctc mode 2. Toggle OC0A on Compare Match
	OCR0A = 0x18;
	TCCR0B |=(1<<CS00); //start counter

//Preparation AutoOff AutoOn et mise a Off at startup
//SoundSensePin to 0 pour vider le condensateur au debut et en entree tout de suite apres.

	Power_Supply_On(); //put power supply on

//PushButton en entree avec pull up
	DDRD &=~(1<<PushButton);	//pd3 en entree
	PORTD |=(1<<PushButton);	//pullup sur pd3
	_delay_ms(10); //laisse le temps a la pull up de charger le condensateur
	
// regarde si pushbutton is pressed if yes = config mode
if (bit_is_clear(PushButtonPin,PushButton))
{
	ConfigModeFlag = 2;
}

//int0 et 1 a configurer
	EICRA  |=((1<<ISC11)|(1<<ISC01)|(1<<ISC00)); // falling edge int1 --rising int0
	EIMSK |=(1<<INT1); // active Pushbutton
	sei();
	Send_Reset(); // send ResetPin to msgeq7	 

		
//******************************************************************************************************************************
//******************************************************************************************************************************
//******************************************************************************************************************************
    while (1) 
    {
		if (bit_is_set(InvertPin,Invert)) //sense le jumper Invert. 
		{for (i = 6; i < 255; i--)
			{
				if (RepartAZeroFlag == 1)
				{	Send_Reset();
					i=6;
					RepartAZeroFlag = 0;
				}
	
				if (ConfigModeFlag == 2)
				{ConfigMode();
				}
	
				//switcher ici le multiplexeur
				//ici on a doit envoyer le lsb et le msb sur pb0 et pb3 mais le bit du millieu sur pd7
					
				temp = 0;
				temp = PORTB & 0b11111010;  //on masque pour conserver ce qu'on veut conserver (1)
				PORTB = temp | i;			
				temp = 0;
				temp2=0;
				temp = i<<6; 				//On switch pb1 de 6 pour mettre sur pd7
				temp2 = PORTD & 0b01111111; //on masque pour conserver ce qu'on veut conserver (1)
				PORTD = temp2|temp;
									
				STROBEPORT &=~(1<<StrobePin); //StrobePin 0
				_delay_us(54); //output is in multiplexer le condensateur (base transistor) se charge.
				
				STROBEPORT |=(1<<StrobePin); //StrobePin 1
				_delay_us(18); //attends 18us et on recommence. avec 2 delais 54 et 18 us on a le minimun de 72us demande.
				
				if (bit_is_clear(PIND,SoundSensePin))  //auto on off
				{OffFlag--;
				}
				else
				{OffFlag = Autooff;
				}
				if (OffFlag == 0)
				{Power_Supply_Off();
				}
			}
			RecenterFlag --;	// apres le for 1 a 7 ou 7 a 0 on decremente de recenter flag de 1. Quand = 00 on recentre les tube en envoyant reset
			if (RecenterFlag == 0)
			{RepartAZeroFlag =1;
			RecenterFlag = 1984;
		}
	}
	else
	{for (i = 0;i < 7; i++)//for (i = 6;i < 255; i--)
		{
				if (RepartAZeroFlag == 1)
				{Send_Reset();
				i=0;
				RepartAZeroFlag = 0;
				}
			
				if (ConfigModeFlag == 2)
				{ConfigMode();
				}			
			
				//switcher ici le multiplexeur
				//ici on a doit envoyer le lsb et le msb sur pb0 et pb3 mais le bit du millieu sur pd7
				temp = 0;
				temp = PORTB & 0b11111010;  //on masque pour conserver ce qu'on veut conserver (1)
				PORTB = temp | i;			//
				temp = 0;
				temp2=0;
				temp = i<<6; 				//On switch pb1 de 6 pour mettre sur pd7
				temp2 = PORTD & 0b01111111; //on masque pour conserver ce qu'on veut conserver (1)
				PORTD = temp2|temp;
				STROBEPORT &=~(1<<StrobePin); //StrobePin 0
				_delay_us(54);//output is in multiplexer le condensateur (base transistor) se charge.
			
				STROBEPORT |=(1<<StrobePin); //StrobePin 1
				_delay_us(18); //attends 36us et on recommence. avec 2 delais de 36us on a le minimun de 72us demande. S'ajoute a ca le temps des in-13
			
				if (bit_is_clear(PIND,SoundSensePin))
				{OffFlag--;
				}
			else
			{OffFlag = Autooff;
			}
			if (OffFlag == 0)
			{Power_Supply_Off();
			}
		}
		RecenterFlag --;	// apres le for 1 a 7 ou 7 a 0 on decremente de recenter flag de 1. Quand = 00 on recentre les tube en envoyant reset
		if (RecenterFlag == 0)
		{RepartAZeroFlag =1;
		RecenterFlag = 1984;
		}
	}
	
	
	
}
}
//******************************************************************************************************************************
//******************************************************************************************************************************
//******************************************************************************************************************************
//Envoie un ResetPin state au msgeq7
void Send_Reset(void){
		//start at 0
		RESETPORT &=~(1<<ResetPin); //ResetPin 0
		STROBEPORT &=~(1<<StrobePin); //StrobePin 0
		_delay_us(100); //StrobePin and ResetPin to 0 to begin with
		//StrobePin high ResetPin high
		RESETPORT |=(1<<ResetPin); //ResetPin 1
		STROBEPORT |=(1<<StrobePin); //StrobePin 1
		_delay_us(18);
		//StrobePin low ResetPin stay high
		STROBEPORT &=~(1<<StrobePin); //StrobePin 0
		_delay_us(18);
		//StrobePin high ResetPin low
		STROBEPORT |=(1<<StrobePin); //StrobePin 1
		RESETPORT &=~(1<<ResetPin); //ResetPin 0
		_delay_us(72);
		//ready to grab 63 hz
		//after each StrobePin to low level we wait 36us before swich multiplexer
}

//******************************************************************************************************************************
//************************************************INT 1 PUSH Button ************************************************************
//Push button appuye. Soit pour repartir a zero soit pour ajuter les tubes
ISR(INT1_vect)
{
	if (ConfigModeFlag == 1)		//Configmodeflag = 1 au demarrage Si pushbutton enfonce.
		{FrequenceFlag ++;			//frequence flag de 1 a 8 60 a 16000 hz
		ConfigModeFlag = 2;			// Est mis a 2 pour dire en retour qu'on veut updater la frequence. Aller dans la sous-ritine Configmode.	
			if (FrequenceFlag == 8)
			{FrequenceFlag = 1;
		}
	} 
	else
	{
		RepartAZeroFlag = 1; //Les tube sont décallé. Pushbutton appuyé et repartazero demandé
	}
	
}

ISR(INT0_vect)
{
	OffFlag = Autooff;
	Power_Supply_On();
	RepartAZeroFlag = 1; //réaligne les tube au cas.
	EIMSK &=~(1<<INT0); //deactive int0 tant que power off n'est pas atteint. empeche tout plein de int0 pour rien
	
}
//******************************************************************************************************************************
//******************************************* Power_Supply_On *******************************************************************
void Power_Supply_On(void){
    SoundSenseDDR |=(1<<SoundSensePin); // pd2 en sortie
    SoundSensePORT	&=~(1<<SoundSensePin); // a 0 )vide le condensateur et empeche le son d'etre present lors du power on. Il doit arriver apres.
    _delay_ms(100);
	DDRC = 0;	//Port c en entree. pull up activé sur 7 msb et pinc0 en entree sans pull-up. Si en sortie = 0v = no psu
	PORTC &=~(1<<PINC0);
    SoundSenseDDR &=~(1<<SoundSensePin); // pour pd2 en entrée
    SoundSensePORT	&=~(1<<SoundSensePin); // sans pullup
    _delay_ms(200);
}

void Power_Supply_Off(void){
	
	if (bit_is_set(PINB,PINB4))	{
	DDRC |=(1<<PINC0);	//Pinc0 en sortie avec 0v = power supply off
	PINC &=~(1<<PINC0);	
	OffFlag = 1;
	EIMSK |=(1<<INT0); //reactive int0 pour pouvoir le rallumer.
	}
	else
	{
	OffFlag = Autooff;
	}
}
//******************************************************************************************************************************
//********************************************* ConfigMode *********************************************************************
//Ici on alterne entre chaque frequence a chaque fois qu'on appuie sur le bouton
//sorti sur pd7
void ConfigMode(void){
	
		TCCR1A |=(1<<COM1A0); //ctc mode 2. Toggle OC0A on Compare Match
		OCR1A = 0;
		TCCR1B = 0;				//Arrete le compteur pour le repartir a la bonne vitesse et bon prescaler
		TCNT1 = 0;				//IMPORTANT pour que le compteur reparte de 0
		if (FrequenceFlag ==1)
		{//Peak 64
			//16-bit Timer/Counter1 64.3hz output on pb1
			OCR1A = 243;
			TCCR1B |=(1<<WGM12)|(1<<CS02); //start counter 256 prescale
		}
		if (FrequenceFlag ==2)
		{//Peak 166
			//16-bit Timer/Counter1 166.22hz output on pb1
			OCR1A = 93;
			TCCR1B |=(1<<WGM12)|(1<<CS02); //start counter 256 prescale
		}
		if (FrequenceFlag ==3)
		{//Peak 400
			//16-bit Timer/Counter1 400.3hz output on pb1
			OCR1A = 155;
			TCCR1B |=(1<<WGM12)|(1<<CS01)|(1<<CS00); //start counter 64 prescale
		}		
		if (FrequenceFlag ==4)
		{//Peak 970
			//16-bit Timer/Counter1 976.56hz output on pb1
			OCR1A = 63;
			TCCR1B |=(1<<WGM12)|(1<<CS01)|(1<<CS00); //start counter 64 prescale
		}	
		if (FrequenceFlag ==5)
		{//Peak 2390
			//16-bit Timer/Counter1 2403.84hz output on pb1
			OCR1A = 25;
			TCCR1B |=(1<<WGM12)|(1<<CS01)|(1<<CS00); //start counter 64 prescale
		}
		if (FrequenceFlag ==6)
		{//Peak 5940
			//16-bit Timer/Counter1 5952.38hz output on pb1
			OCR1A = 83;
			TCCR1B |=(1<<WGM12)|(1<<CS01); //start counter 8 prescale
		}
		if (FrequenceFlag ==7)
		{//Peak 14850
			//16-bit Timer/Counter1 14705.88hz output on pb1
			OCR1A = 33;
			TCCR1B |=(1<<WGM12)|(1<<CS01); //start counter 8 prescale
		}
		ConfigModeFlag = 1;
	
}
//******************************************************************************************************************************
//******************************************************************************************************************************
void ConfigOutputHigh(void){
	ConfigOutputPort |=(1<<ConfigOutput); //0
	
}

void ConfigOutputLow(void){
	ConfigOutputPort &=~(1<<ConfigOutput); //0
}



/*Alternative
//Peak 14850
//16-bit Timer/Counter1 15625hz output on pb1
TCCR1A |=(1<<COM1A0); //ctc mode 2. Toggle OC0A on Compare Match
TCCR1B |=(1<<WGM12);
OCR1A = 255;
TCCR1B |=(0<<CS02)|(0<<CS01)|(1<<CS00); //start counter 1 prescale

//Peak 14850
//16-bit Timer/Counter1 14705.88hz output on pb1
TCCR1A |=(1<<COM1A0); //ctc mode 2. Toggle OC0A on Compare Match
TCCR1B |=(1<<WGM12);
OCR1A = 33;
TCCR1B |=(0<<CS02)|(1<<CS01)|(0<<CS00); //start counter 8 prescale
*/