/*********************************************************************
 *  Main Application Entry Point and TCP/IP Stack Demo
 *  Module for Microchip TCP/IP Stack
 *   -Demonstrates how to call and use the Microchip TCP/IP stack
 *	 -Reference: AN833
 *********************************************************************
 * FileName:        MainDemo.c
 * Dependencies:    TCPIP.h
 * Processor:       PIC18, PIC24F, PIC24H, dsPIC30F, dsPIC33F, PIC32
 * Compiler:        Microchip C32 v1.05 or higher
 *					Microchip C30 v3.12 or higher
 *					Microchip C18 v3.30 or higher
 *					HI-TECH PICC-18 PRO 9.63PL2 or higher
 * Company:         Microchip Technology, Inc.
 *
 * Marc Lobelle         03/15/10- adapted to support sdcc compiler
 *                              - removed some code not related to networking
 *                              - removed serial line support
 *                              - added a few debuging functions
 ********************************************************************/
/*
 * This symbol uniquely defines this file as the main entry point.
 * There should only be one such definition in the entire project,
 * and this file must define the AppConfig variable as described below.
 * The processor configuration will be included in HardwareProfile.h
 * if this symbol is defined.
 */
#define THIS_INCLUDES_THE_MAIN_FUNCTION
#define THIS_IS_STACK_APPLICATION

#define __18F97J60	// define the processor we use

#define __SDCC__	// define the compiler we use

// inlude all hardware and compiler dependent definitions
#include "Include/HardwareProfile.h"

// Include all headers for any enabled TCPIP Stack functions
#include "Include/TCPIP_Stack/TCPIP.h"

#include "Include/TCPIP_Stack/Delay.h"

// Include functions specific to this stack application
#include "Include/MainDemo.h"

// Declare AppConfig structure and some other supporting stack variables
APP_CONFIG AppConfig;
BYTE AN0String[8];


// Private helper functions.
// These may or may not be present in all applications.
static void InitAppConfig(void);
static void InitializeBoard(void);
void DisplayWORD(BYTE pos, WORD w); //write WORDs on LCD for debugging

//
// PIC18 Interrupt Service Routines
// 

//LowISR
#if defined(__18CXX)
    #if defined(HI_TECH_C)
        void interrupt low_priority LowISR(void)
    #elif defined(__SDCC__)
        void LowISR(void) __interrupt (2) //ML for sdcc
    #else
        #pragma interruptlow LowISR
        void LowISR(void)
    #endif
    {
	TickUpdate();
    }
    #if !defined(__SDCC__) && !defined(HI_TECH_C)
           //automatic with these compilers
        #pragma code lowVector=0x18
	void LowVector(void){_asm goto LowISR _endasm}
	#pragma code // Return to default code section
    #endif

//HighISR	
    #if defined(HI_TECH_C)
        void interrupt HighISR(void)
    #elif defined(__SDCC__)
        void HighISR(void) __interrupt(1) //ML for sdcc        
    #else
        #pragma interruptlow HighISR
        void HighISR(void)
    #endif
    {
      //insert here code for high level interrupt, if any
    }
    #if !defined(__SDCC__) && !defined(HI_TECH_C)
           //automatic with these compilers
	#pragma code highVector=0x8
	void HighVector(void){_asm goto HighISR _endasm}
	#pragma code // Return to default code section
    #endif

#endif

const char* message;  //pointer to message to display on LCD

//
// Main application entry point.
//

#if defined(__18CXX) || defined(__SDCC__)
void main(void)
#else
int main(void)
#endif
{
	static TICK t = 0; 
	TICK nt = 0;  //TICK is DWORD, thus 32 bits
	BYTE loopctr = 0;  //ML Debugging
	WORD lloopctr = 14; //ML Debugging
	static DWORD dwLastIP = 0;	    
    InitializeBoard();	// Initialize interrupts and application specific hardware    
    LCDInit();		// Initialize and display message on the LCD
    DelayMs(100);
    DisplayString (0,"Olimex"); //first arg is start position on 32 pos LCD    
    TickInit();		// Initialize Timer0, and low priority interrupts, used as clock.    
    InitAppConfig();	// Initialize Stack and application related variables in AppConfig.
    // Initialize core stack layers (MAC, ARP, TCP, UDP) and application modules (HTTP, SNMP, etc.)
    StackInit();
    // Begin the co-operative multitasking loop.  This infinite loop will continuously 
    // execute all stack-related tasks, as well as your own application's functions.
    // Custom functions should be added at the end of this loop.
    while(1) {
        // Blink LED0 (right most one) every second.
        nt =  TickGetDiv256();
        if((nt - t) >= (DWORD)(TICK_SECOND/1024ul)) {
            t = nt;
            LED0_IO ^= 1;
            ClrWdt();  //Clear the watchdog
        }
        // This task performs normal stack task including checking
        // for incoming packet, type of packet and calling
        // appropriate stack entity to process it.
        StackTask();        
        // This tasks invokes each of the core stack application tasks
		// StackApplications(); //all except dhcp, ping and arp

        // Process application specific tasks here.

        // If the local IP address has changed (ex: due to DHCP lease change)
        // write the new IP address to the LCD display, UART, and Announce 
        // service
		if(dwLastIP != AppConfig.MyIPAddr.Val) {
			dwLastIP = AppConfig.MyIPAddr.Val;
			#if defined(__SDCC__)
			DisplayIPValue(dwLastIP); // must be a WORD: sdcc does not pass aggregates
			#else
			DisplayIPValue(AppConfig.MyIPAddr);
			#endif
		}
	}
}
/*******************************************************
 Function DisplayWORD:
 writes a WORD in hexa on the position indicated by pos. 
 - pos=0 -> 1st line of the LCD
 - pos=16 -> 2nd line of the LCD
 __SDCC__ only: for debugging
********************************************************/
#if defined(__SDCC__)
void DisplayWORD(BYTE pos, WORD w) {	//WORD is a 16 bits unsigned
    BYTE WDigit[6]; //enough for a  number < 65636: 5 digits + \0
    BYTE j;
    BYTE LCDPos=0;  //write on first line of LCD
    unsigned radix=10; //type expected by sdcc's ultoa()
    LCDPos=pos;
    ultoa(w, WDigit, radix);      
    for(j = 0; j < strlen((char*)WDigit); j++) {
       LCDText[LCDPos++] = WDigit[j];
    }
    if(LCDPos < 32u) {
		LCDText[LCDPos] = 0;
    }
    LCDUpdate();
}
/******************************************************************
 Function DisplayString: 
 Writes an IP address to string to the LCD display starting at pos
*******************************************************************/
void DisplayString(BYTE pos, char* text) {
   BYTE l= strlen(text)+1;
   BYTE max= 32-pos;
   strlcpy((char*)&LCDText[pos], text,(l<max)?l:max );
   LCDUpdate();
}
#endif

/*************************************************
 Function DisplayIPValue: 
 Writes an IP address to the LCD display
**************************************************/
#if defined(__SDCC__)
void DisplayIPValue(DWORD IPdw) // 32 bits
#else
void DisplayIPValue(IP_ADDR IPVal) 
#endif
{
    BYTE IPDigit[4]; //enough for a number <256: 3 digits + \0
    BYTE i;
    BYTE j;
    BYTE LCDPos=16;  //write on second line of LCD
#if defined(__SDCC__)
    unsigned int IP_field, radix=10; //type expected by sdcc's uitoa()
#endif
    for(i = 0; i < sizeof(IP_ADDR); i++) {	//sizeof(IP_ADDR) is 4
#if defined(__SDCC__)
		IP_field =(WORD)(IPdw>>(i*8))&0xff;      //ML
		uitoa(IP_field, IPDigit, radix);      //ML
#else
		uitoa((WORD)IPVal.v[i], IPDigit);
#endif
		for(j = 0; j < strlen((char*)IPDigit); j++) {
			LCDText[LCDPos++] = IPDigit[j];
		}
		if(i == sizeof(IP_ADDR)-1) {
			break;
		LCDText[LCDPos++] = '.';
	}
    if(LCDPos < 32u) {
		LCDText[LCDPos] = 0;
	}
    LCDUpdate();
}

/****************************************************************************
  Function:
    static void InitializeBoard(void)
  Description:
    This routine initializes the hardware.  It is a generic initialization
    routine for many of the Microchip development boards, using definitions
    in HardwareProfile.h to determine specific initialization.
  ***************************************************************************/
static void InitializeBoard(void) {	
	// LEDs
	LED0_TRIS = 0;  //LED0
	LED1_TRIS = 0;  //LED1
	LED2_TRIS = 0;  //LED2
	LED3_TRIS = 0;  //LED_LCD1
	LED4_TRIS = 0;  //LED_LCD2
	LED5_TRIS = 0;  //LED5=RELAY1
	LED6_TRIS = 0;  //LED7=RELAY2
#if (!defined(EXPLORER_16) &&!defined(OLIMEX_MAXI))    // Pin multiplexed with 
	// a button on EXPLORER_16 and not used on OLIMEX_MAXI
	LED7_TRIS = 0;
#endif
	LED_PUT(0x00);  //turn off LED0 - LED2
	RELAY_PUT(0x00); //turn relays off to save power
	//set clock to 25 MHz
	// Enable PLL but disable pre and postscalers: the primary oscillator
	// runs at the speed of the 25MHz external quartz
	OSCTUNE = 0x40;
	// Switch to primary oscillator mode, 
	// regardless of if the config fuses tell us to start operating using 
    // the the internal RC
	// The external clock must be running and must be 25MHz for the 
	// Ethernet module and thus this Ethernet bootloader to operate.
	if(OSCCONbits.IDLEN) {	//IDLEN = 0x80; 0x02 selects the primary clock
		OSCCON = 0x82;
	} else {
		OSCCON = 0x02;
	}
	// Enable Interrupts
	RCONbits.IPEN = 1;		// Enable interrupt priorities
    INTCONbits.GIEH = 1;
	INTCONbits.GIEL = 1;
}

/*********************************************************************
 * Function:        void InitAppConfig(void)
 *
 * PreCondition:    MPFSInit() is already called.
 *
 * Output:          Write/Read non-volatile config variables.
 ********************************************************************/
static void InitAppConfig(void) {
	AppConfig.Flags.bIsDHCPEnabled = TRUE;
	AppConfig.Flags.bInConfigMode = TRUE;
//ML using sdcc (MPLAB has a trick to generate serial numbers)
// first 3 bytes indicate manufacturer; last 3 bytes are serial number
	AppConfig.MyMACAddr.v[0] = 0;
	AppConfig.MyMACAddr.v[1] = 0x04;
	AppConfig.MyMACAddr.v[2] = 0xA3;
	AppConfig.MyMACAddr.v[3] = 0x01;
	AppConfig.MyMACAddr.v[4] = 0x02;
	AppConfig.MyMACAddr.v[5] = 0x03;
//ML if you want to change, see TCPIPConfig.h
	AppConfig.MyIPAddr.Val = MY_DEFAULT_IP_ADDR_BYTE1 | 
            MY_DEFAULT_IP_ADDR_BYTE2<<8ul | MY_DEFAULT_IP_ADDR_BYTE3<<16ul | 
            MY_DEFAULT_IP_ADDR_BYTE4<<24ul;
	AppConfig.DefaultIPAddr.Val = AppConfig.MyIPAddr.Val;
	AppConfig.MyMask.Val = MY_DEFAULT_MASK_BYTE1 | 
            MY_DEFAULT_MASK_BYTE2<<8ul | MY_DEFAULT_MASK_BYTE3<<16ul | 
            MY_DEFAULT_MASK_BYTE4<<24ul;
	AppConfig.DefaultMask.Val = AppConfig.MyMask.Val;
	AppConfig.MyGateway.Val = MY_DEFAULT_GATE_BYTE1 | 
            MY_DEFAULT_GATE_BYTE2<<8ul | MY_DEFAULT_GATE_BYTE3<<16ul | 
            MY_DEFAULT_GATE_BYTE4<<24ul;
	AppConfig.PrimaryDNSServer.Val = MY_DEFAULT_PRIMARY_DNS_BYTE1 | 
            MY_DEFAULT_PRIMARY_DNS_BYTE2<<8ul  | 
            MY_DEFAULT_PRIMARY_DNS_BYTE3<<16ul  | 
            MY_DEFAULT_PRIMARY_DNS_BYTE4<<24ul;
	AppConfig.SecondaryDNSServer.Val = MY_DEFAULT_SECONDARY_DNS_BYTE1 | 
            MY_DEFAULT_SECONDARY_DNS_BYTE2<<8ul  | 
            MY_DEFAULT_SECONDARY_DNS_BYTE3<<16ul  | 
            MY_DEFAULT_SECONDARY_DNS_BYTE4<<24ul;
}

/*-------------------------------------------------------------------------
 * strlcpy.c
 *    strncpy done right
 *
 * This file was taken from OpenBSD and is used on platforms that don't
 * provide strlcpy().  The OpenBSD copyright terms follow.
 *-------------------------------------------------------------------------
 */
size_t strlcpy(char *dst, const char *src, size_t siz) {
    char       *d = dst;
    const char *s = src;
    size_t      n = siz;
    /* Copy as many bytes as will fit */
    if (n != 0) {
        while (--n != 0) {
            if ((*d++ = *s++) == '\0') {
                break;
			}
        }
    }
    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (siz != 0) {
            *d = '\0';          /* NUL-terminate dst */
		}
        while (*s++) {
            ;
		}
    }
    return (s - src - 1);       /* count does not include NUL */
}
