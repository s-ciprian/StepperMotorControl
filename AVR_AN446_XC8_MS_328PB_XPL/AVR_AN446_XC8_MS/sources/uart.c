/*This file has been prepared for Doxygen automatic documentation generation.*/
/*! \file *********************************************************************
 *
 * \brief Uart routines.
 *
 * - File:               uart.c
 * - Compiler:           IAR EWAAVR 4.11A
 * - Supported devices:  All devices with a 16 bit timer can be used.
 *                       The example is written for ATmega48
 * - AppNote:            AVR446 - Linear speed control of stepper motor
 *
 * \author               Atmel Corporation: http://www.atmel.com \n
 *                       Support email: avr@atmel.com
 *
 * $Name: RELEASE_1_0 $
 * $Revision: 1.2 $
 * $RCSfile: uart.c,v $
 * $Date: 2006/05/08 12:25:58 $
 *****************************************************************************/
//#include <ioavr.h>
#include "xc.h"
#include "global.h"
#include "uart.h"
#include "sm_driver.h"
#include "speed_cntr.h"

//! RX buffer for uart.
unsigned char UART_RxBuffer[UART_RX_BUFFER_SIZE];
//! RX buffer pointer.
unsigned char UART_RxPtr;

// Static Variables.
//! TX buffer for uart.
static unsigned char UART_TxBuffer[UART_TX_BUFFER_SIZE];
//! TX buffer head pointer.
static volatile unsigned char UART_TxHead;
//! TX buffer tail pointer.
static volatile unsigned char UART_TxTail;

/*! \brief Init of uart.
 *
 * Setup uart. The \ref BAUD value must be modified according to clock frqequency.
 * Refer to datasheet for details.
 *
 */
void InitUART(void)
{
// Set baud rate. 19.2 kbps trasfer speed running at 8 MHz.
//#define BAUD 25
// Set baud rate. 19.2 kbps trasfer speed running at 3.6864 MHz.
//#define BAUD 11

/* Set baud rate 19.2 kbps transfer speed running at 16 MHz.
   Note: ATmega328PB Xplained Mini use this clock frequency
   Table 24-9. Examples of UBRRn Settings for Commonly Used Oscillator Frequencies
   from 328PB data sheet - DS40001906C-page 281. */
#define BAUD 51

  UBRR0H = (unsigned char)(BAUD>>8);
  UBRR0L = (unsigned char)BAUD;
  /* Enable receiver and transmitter, rx and tx int */
  UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0)|(1<<TXCIE0);
  /* Set frame format: 8data, 1stop bit */
  UCSR0C = (3<<UCSZ00);

  // Flush Buffers
  UART_RxPtr = 0;
  UART_TxTail = 0;
  UART_TxHead = 0;
}


/*! \brief send a byte.
 *
 *  Puts a byte in TX buffer and starts uart TX interrupt.
 *  If TX buffer is full it will hang until space.
 *
 *  \param data  Data to be sent.
 */
void uart_SendByte(unsigned char data)
{
  unsigned char tmphead;

  // Calculate buffer index
  tmphead = ( UART_TxHead + 1 ) & UART_TX_BUFFER_MASK;
  // Wait for free space in buffer
  while ( tmphead == UART_TxTail )
    ;
  // Store data in buffer
  UART_TxBuffer[tmphead] = data;
  // Store new index
  UART_TxHead = tmphead;
  // Enable UDRE interrupt
  SET_UDRIE;
}

/*! \brief Sends a string.
 *
 *  Loops thru a string and send each byte with uart_SendByte.
 *  If TX buffer is full it will hang until space.
 *
 *  \param Str  String to be sent.
 */
void uart_SendString(const char * const Str)
{
  unsigned char n = 0;
  while(Str[n])
    uart_SendByte(Str[n++]);
}

/*! \brief Sends a integer.
 *
 *  Converts a integer to ASCII and sends it using uart_SendByte.
 *  If TX buffer is full it will hang until space.
 *
 *  \param x  Integer to be sent.
 */
void uart_SendInt(int x)
{
  static const char dec[] = "0123456789";
  unsigned int div_val = 10000;

  if (x < 0){
    x = - x;
    uart_SendByte('-');
  }
  while (div_val > 1 && div_val > x)
    div_val /= 10;
  do{
    uart_SendByte (dec[x / div_val]);
    x %= div_val;
    div_val /= 10;
  }while(div_val);
}

/*! \brief Empties the uart RX buffer.
 *
 *  Empties the uart RX buffer.
 *
 *  \return x  Integer to be sent.
 */
void uart_FlushRxBuffer(void){
  UART_RxPtr = 0;
  UART_RxBuffer[UART_RxPtr] = 0;
}

/*! \brief RX interrupt handler.
 *
 *  RX interrupt handler.
 *  RX interrupt always enabled.
 */
//#pragma vector=USART_RX_vect
void __interrupt(USART0_RX_vect_num) UART_RX_interrupt(void)
{
  unsigned char data;

  // Read the received data.
  data = UDR0;

  if(status.running == FALSE){
    // If backspace.
    if(data == '\b')
    {
      if(UART_RxPtr)
      // Done if not at beginning of buffer.
      {
        uart_SendByte('\b');
        uart_SendByte(' ');
        uart_SendByte('\b');
        UART_RxPtr--;
        UART_RxBuffer[UART_RxPtr]=0x00;
      }
    }
    // Not backspace.
    else
    {
      // Put the data into RxBuf
      // and place 0x00 after it. If buffer is full,
      // data is written to UART_RX_BUFFER_SIZE - 1.
      if(UART_RxPtr < (UART_RX_BUFFER_SIZE - 1)){
        UART_RxBuffer[UART_RxPtr] = data;
        UART_RxBuffer[UART_RxPtr + 1]=0x00;
        UART_RxPtr++;
      }
      else
      {
        UART_RxBuffer[UART_RxPtr - 1] = data;
        uart_SendByte('\b');
      }
      // If enter.
      if(data == 13){
        status.cmd = TRUE;
      }
      else
        uart_SendByte(data);
    }
  }
}

/*! \brief TX Data Register empty interrupt handler.
 *
 *  TX Data Register empty interrupt handler.
 *  TX Data Register empty interrupt turned on by uart_SendByte,
 *  turned off when TX buffer is empty.
 */
//#pragma vector=USART_UDRE_vect
void __interrupt(USART0_UDRE_vect_num) UART0_DataRegEmpty_interrupt(void)
{
  unsigned char UART_TxTail_tmp;
  UART_TxTail_tmp = UART_TxTail;

  // Check if all data is transmitted
  if ( UART_TxHead !=  UART_TxTail_tmp )
  {
    // Calculate buffer index
    UART_TxTail_tmp = ( UART_TxTail + 1 ) & UART_TX_BUFFER_MASK;
    // Store new index
    UART_TxTail =  UART_TxTail_tmp;
    // Start transmition
    UDR0= UART_TxBuffer[UART_TxTail_tmp];
  }
  else
  {
    // Disable UDRE interrupt
    CLR_UDRIE;
  }    
    
//  TX code example from ATmega328P datasheet DS40002061B-page 186 and ATmega328PB datasheet DS40001906C-page 268 
//    /* Wait for empty transmit buffer */
//    while ( !( UCSR0A & (1<<UDRE0)) )
//     ;
}

/*! \brief TX interrupt handler.
 *
 *  TX interrupt handler.
 *  This function is here because the TX Interrupt was enabled in Init() function
 */
void __interrupt(USART0_TX_vect_num) UART0_TX_interrupt_XXX(void)
{
    /* Nothing to do here */
    volatile unsigned int test = 0;
    test++;
}
