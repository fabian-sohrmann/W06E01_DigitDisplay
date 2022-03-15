/*
 * File:   main.c
 * Author: Fabian Sohrmann
 * Email:  mfsohr@utu.fi
 * 
 * This program simulates a digital scoreboard. Numbers on the keyboard will
 * appear on the microcontrollers LED-display. The control works via a serial
 * connection. A warning message is displayed in the command line if some 
 * other key is pressed.
 *
 * Created on 2. joulukuu 2021, 10:30
 */

#define F_CPU 3333333
#define USART0_BAUD_RATE(BAUD_RATE) ((float)(F_CPU*64/(16* \
                                    (float)BAUD_RATE))+0.5)

#include <avr/io.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "string.h"

// Create global queues
volatile QueueHandle_t intQueueForDriver;
volatile QueueHandle_t charQueueForError;

/**
 * Copied and modified from datasheet TB3216. Using USART0 because it uses PA0
 * and P01 for transmitting and receiving.
 */
void USART0_init(void)
{
    PORTA.DIRSET = PIN0_bm; // PA0 to output
    PORTA.DIRCLR = PIN1_bm; // PA1 to input (default)
    USART0.BAUD = (uint16_t)USART0_BAUD_RATE(9600); // Configure baud rate
    USART0.CTRLB |= USART_TXEN_bm; // Enable transmitter 
    USART0.CTRLB |= USART_RXEN_bm; // Enable receiver
}

/**
 * Copied and modified from datasheet TB3216 
 */
uint8_t USART0_readChar()
{
    while (!(USART0.STATUS & USART_RXCIF_bm))
    {
        ;
    }
    return USART0.RXDATAL;
}

/**
 * Copied and modified from datasheet TB3216
 */
void USART0_sendChar(char c)
{
    while (!(USART0.STATUS & USART_DREIF_bm))
    {
        ;
    }
    USART0.TXDATAL = c;
}

/**
 * Copied and modified from datasheet TB3216
 */
void USART0_sendString(char *str)
{
    for(size_t i = 0; i < strlen(str); i++)
    {
        USART0_sendChar(str[i]);
    }
}

/**
 * Task for reading user input. Sends the read character converted to an 
 * integer to the driver task via the intQueueForDriver-queue.
 */
void readerTask(void *parameter)
{
    intQueueForDriver = xQueueCreate(5,1); // creating queue
    charQueueForError = xQueueCreate(5,1); 
    
    for(;;)
    {
        char userInput = USART0_readChar();
        int userInputToInt = userInput-'0'; 
        xQueueSend(intQueueForDriver, (void *)&userInputToInt, 0);
        xQueueSend(charQueueForError, (void *)&userInput, 0);
    }
}

/**
 * Task for driving the display. Receives the user input as an integer via
 * the intQueueForDriver-queue. Draws the correct number according to the 
 * received integer. 
 */
void driverTask(void *parameter)
{
    // Transistor connects display ground to GND, setting pins to output
    PORTF.DIRSET = PIN5_bm; 
    PORTF.OUTSET = PIN5_bm;
    PORTC.DIRSET = 0xFF;
    
    uint8_t number[] =
    {
        // 0, 1, 2, ... 9, E
        0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 0b01101101, 
        0b01111101, 0b00000111, 0b01111111, 0b01101111, 0b01111001          
    };
    
    int recInt; // received integer stored here
    
    for(;;)
    {
        if(xQueueReceive(intQueueForDriver, (void *)&recInt, 0)==pdTRUE)
        {
            // if received integer 0-9
            if((recInt==0)||(recInt==1)||(recInt==2)||(recInt==3)||
               (recInt==4)||(recInt==5)||(recInt==6)||(recInt==7)||
               (recInt==8)||(recInt==9))
            {
                PORTC.OUTCLR = 0xFF;
                PORTC.OUTSET = number[recInt];
            }
            // if received any other integer or "character"
            else
            {
                PORTC.OUTCLR = 0xFF;
                PORTC.OUTSET = number[10]; // display E
            }
        }
    }
}

/**
 * Task for sending error message to the user. Receives the user input via the
 * charQueueForError-queue. An appropriate message is sent according to whether
 * the received characters are digits or not.
 */
void senderTask(void *parameter)
{
    char recChar; // received character stored here
    for(;;)
    {
        if(xQueueReceive(charQueueForError, (void *)&recChar, 0)==pdTRUE)
        {
            // if received item is a digit
            if((recChar=='0')||(recChar=='1')||(recChar=='2')||(recChar=='3')||
               (recChar=='4')||(recChar=='5')||(recChar=='6')||(recChar=='7')||
               (recChar=='8')||(recChar=='9'))
            {   
                USART0_sendString("Valid digit was entered.");
            }
            // if received item is no a digit
            else
            {
                USART0_sendString("Error! Not a valid digit.");
            }
        }  
    }
}

int main(void)
{
    USART0_init();
    
    // Create tasks, modified from example in materials
    xTaskCreate(
        readerTask, 
        "reader", 
        configMINIMAL_STACK_SIZE, 
        NULL, 
        tskIDLE_PRIORITY, 
        NULL);
    
    xTaskCreate(
        driverTask, 
        "driver", 
        configMINIMAL_STACK_SIZE, 
        NULL, tskIDLE_PRIORITY, 
        NULL);
    xTaskCreate(
        senderTask, 
        "sender", 
        configMINIMAL_STACK_SIZE, 
        NULL, 
        tskIDLE_PRIORITY, 
        NULL);
    
    // Start scheduler
    vTaskStartScheduler();
    
    return 0;
}