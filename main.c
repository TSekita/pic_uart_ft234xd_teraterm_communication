// PIC16F18857 Configuration Bit Settings

// 'C' source line config statements

// CONFIG1
#pragma config FEXTOSC = OFF    // External Oscillator mode selection bits (Oscillator not enabled)
#pragma config RSTOSC = HFINT1  // Power-up default value for COSC bits (HFINTOSC (1MHz))
#pragma config CLKOUTEN = OFF   // Clock Out Enable bit (CLKOUT function is disabled; i/o or oscillator function on OSC2)
#pragma config CSWEN = ON       // Clock Switch Enable bit (Writing to NOSC and NDIV is allowed)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable bit (FSCM timer disabled)

// CONFIG2
#pragma config MCLRE = ON       // Master Clear Enable bit (MCLR pin is Master Clear function)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config LPBOREN = OFF    // Low-Power BOR enable bit (ULPBOR disabled)
#pragma config BOREN = ON       // Brown-out reset enable bits (Brown-out Reset Enabled, SBOREN bit is ignored)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (VBOR) set to 1.9V on LF, and 2.45V on F Devices)
#pragma config ZCD = OFF        // Zero-cross detect disable (Zero-cross detect circuit is disabled at POR.)
#pragma config PPS1WAY = ON     // Peripheral Pin Select one-way control (The PPSLOCK bit can be cleared and set only once in software)
#pragma config STVREN = OFF     // Stack Overflow/Underflow Reset Enable bit (Stack Overflow or Underflow will not cause a reset)

// CONFIG3
#pragma config WDTCPS = WDTCPS_31// WDT Period Select bits (Divider ratio 1:65536; software control of WDTPS)
#pragma config WDTE = OFF       // WDT operating mode (WDT Disabled, SWDTEN is ignored)
#pragma config WDTCWS = WDTCWS_7// WDT Window Select bits (window always open (100%); software control; keyed access not required)
#pragma config WDTCCS = SC      // WDT input clock selector (Software Control)

// CONFIG4
#pragma config WRT = OFF        // UserNVM self-write protection bits (Write protection off)
#pragma config SCANE = not_available// Scanner Enable bit (Scanner module is not available for use)
#pragma config LVP = ON         // Low Voltage Programming Enable bit (Low Voltage programming enabled. MCLR/Vpp pin function is MCLR.)

// CONFIG5
#pragma config CP = OFF         // UserNVM Program memory code protection bit (Program Memory code protection disabled)
#pragma config CPD = OFF        // DataNVM code protection bit (Data EEPROM code protection disabled)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>

#define _XTAL_FREQ 16000000 //Internal oscillator Hz
#define BUFFER_SIZE 64
char inputBuffer[BUFFER_SIZE];
uint8_t index = 0;

// PPS設定ロック解除
#define PPS_UNLOCK()  do { \
    INTCONbits.GIE = 0; /* 割り込み無効 */ \
    PPSLOCK = 0x55; PPSLOCK = 0xAA; PPSLOCKbits.PPSLOCKED = 0; \
} while(0)

// PPS設定ロック有効
#define PPS_LOCK()  do { \
    PPSLOCK = 0x55; PPSLOCK = 0xAA; PPSLOCKbits.PPSLOCKED = 1; \
    INTCONbits.GIE = 1; /* 割り込み有効 */ \
} while(0)

void UART_Init(void) {
    // 内部振動子周波数16MHz設定
    OSCCON1 = 0x60; // HFINTOSC, no PLL
    OSCFRQ = 0x05; // 16 MHz

    // TX (RC6)を出力、RX (RC7)を入力に設定
    TRISC6 = 0; // TXピンを出力
    TRISC7 = 1; // RXピンを入力

    ANSC7 = 0; // 出力をデジタルに設定

    PPS_UNLOCK();

    // PPS設定:TXはRC6、RXはRC7
    RC6PPS = 0x10; // RC6はTXの機能を使用のため、0x10を代入
    RXPPS = 0x17; // RC7はRXの機能を使用のため、RC7のアドレス0x17を代入

    PPS_LOCK();


    // EUSART設定
    BAUD1CON = 0x08; // BRG16=1�i16�r�b�g�{�[���[�g�j
    TX1STA = 0x24; // TXEN=1�i���M�L���j, BRGH=1
    RC1STA = 0x90; // SPEN=1�iEUSART�L���j, CREN=1�i��M�L���j

    // �{�[���[�g�ݒ�i9600bps, 16MHz�N���b�N, BRG16=1, BRGH=1�j
    // �v�Z���FBRG = (Fosc / (4 * Baud)) - 1
    // = (16000000 / (4 * 9600)) - 1 = 416.666... �� 416
    SP1BRGL = 415 & 0xFF;
    SP1BRGH = (415 >> 8) & 0xFF;
}

void UART_Write(char data) {
    while (!TXIF); // �� �C���FTX1IF �� TXIF
    TX1REG = data;
}

char UART_Read(void) {
    // �G���[������ꍇ�͕���
    if (RC1STAbits.OERR) {
        RC1STAbits.CREN = 0;
        RC1STAbits.CREN = 1;
    }

    while (!PIR3bits.RCIF); // ��M�҂�

    if (RC1STAbits.FERR) {
        volatile char dummy = RC1REG; // �G���[���͓ǂݎ̂�
        return 0; // �������� 0xFF �� '?' ��Ԃ�
    }

    return RC1REG; // �����M
}

void main(void) {
    UART_Init();

    const char* msg = "UART Ready\r\n";
    while (*msg) UART_Write(*msg++);

    while (1) {
        if (PIR3bits.RCIF) {
            char c = UART_Read();

            if (c == '\r' || c == '\n') {  // ���s�œ��͊���
                inputBuffer[index] = '\0';  // ������I�[
           
                for (uint8_t i = 0; i < index; i++) {
                    UART_Write(inputBuffer[i]);
                    if (i == index - 1) {
                        UART_Write('\r');
                        UART_Write('\n');
                    }
                }
                index = 0;  // �o�b�t�@�N���A
            } else {
                if (index < BUFFER_SIZE - 1) {
                    inputBuffer[index++] = c;
                } else {
                    // �o�b�t�@�I�[�o�[�t���[�΍�i�K�v�Ȃ珈���ǉ��j
                    index = 0;
                }
            }
        }
    }
}
