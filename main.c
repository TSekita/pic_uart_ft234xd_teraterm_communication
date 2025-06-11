// PIC16F18857 Configuration Bit Settings

// 'C' source line config statements

// CONFIG1
#pragma config FEXTOSC = OFF    // 外部オシレータ無効
#pragma config RSTOSC = HFINT1  // 初期クロックをHFINTOSC（1MHz）に設定
#pragma config CLKOUTEN = OFF   // クロック出力無効
#pragma config CSWEN = ON       // クロックスイッチ許可
#pragma config FCMEN = OFF      // フェイルセーフクロック監視無効

// CONFIG2
#pragma config MCLRE = ON       // MCLRピン有効（リセット機能）
#pragma config PWRTE = OFF      // パワーアップタイマ無効
#pragma config LPBOREN = OFF    // 低電力BOR無効
#pragma config BOREN = ON       // BOR（ブラウンアウトリセット）有効
#pragma config BORV = LO        // BOR電圧を低に設定（2.45V）
#pragma config ZCD = OFF        // ゼロクロス検出無効
#pragma config PPS1WAY = ON     // PPSの設定は一度だけ可能
#pragma config STVREN = OFF     // スタックオーバーフロー／アンダーフローによるリセット無効

// CONFIG3
#pragma config WDTCPS = WDTCPS_31 // ウォッチドッグタイマ周期（1:65536）
#pragma config WDTE = OFF       // ウォッチドッグタイマ無効
#pragma config WDTCWS = WDTCWS_7 // ウィンドウ常時オープン
#pragma config WDTCCS = SC      // ソフトウェア制御クロック

// CONFIG4
#pragma config WRT = OFF        // 自己書き込み保護無効
#pragma config SCANE = not_available // スキャナ機能無効
#pragma config LVP = ON         // 低電圧書き込み有効

// CONFIG5
#pragma config CP = OFF         // プログラムメモリ保護無効
#pragma config CPD = OFF        // データEEPROM保護無効

#include <xc.h>

#define _XTAL_FREQ 16000000 // 内部クロック 16MHz
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
    // 内部発振器を16MHzに設定
    OSCCON1 = 0x60; // HFINTOSC, PLLなし
    OSCFRQ = 0x05;  // 16MHz

    // TX (RC6) を出力、RX (RC7) を入力に設定
    TRISC6 = 0; // TX出力
    TRISC7 = 1; // RX入力

    ANSC7 = 0;  // デジタル入力に設定

    PPS_UNLOCK();

    // PPS設定: TXはRC6、RXはRC7
    RC6PPS = 0x10; // RC6にTX機能を割当て
    RXPPS = 0x17;  // RC7にRX機能を割当て

    PPS_LOCK();

    // EUSART設定
    BAUD1CON = 0x08; // BRG16=1（16ビットボーレート生成器）
    TX1STA = 0x24;   // TXEN=1（送信有効）, BRGH=1（高速）
    RC1STA = 0x90;   // SPEN=1（EUSART有効）, CREN=1（連続受信）

    // ボーレート設定（9600bps, 16MHz, BRG16=1, BRGH=1）
    // 式: BRG = (Fosc / (4 * Baud)) - 1
    // → (16000000 / (4 * 9600)) - 1 = 416
    SP1BRGL = 416 & 0xFF;
    SP1BRGH = (416 >> 8) & 0xFF;
}

void UART_Write(char data) {
    while (!TXIF); // TX1IFがセットされるまで待機
    TX1REG = data;
}

char UART_Read(void) {
    // エラー時はリセット
    if (RC1STAbits.OERR) {
        RC1STAbits.CREN = 0;
        RC1STAbits.CREN = 1;
    }

    while (!PIR3bits.RCIF); // 受信待ち

    if (RC1STAbits.FERR) {
        volatile char dummy = RC1REG; // エラー読み捨て
        return 0;
    }

    return RC1REG; // 正常な受信
}

void main(void) {
    UART_Init();

    const char* msg = "UART Ready\r\n";
    while (*msg) UART_Write(*msg++);

    while (1) {
        if (PIR3bits.RCIF) {
            char c = UART_Read();

            if (c == '\r' || c == '\n') {  // 改行で送信実行
                inputBuffer[index] = '\0';  // 終端文字

                for (uint8_t i = 0; i < index; i++) {
                    UART_Write(inputBuffer[i]);
                    if (i == index - 1) {
                        UART_Write('\r');
                        UART_Write('\n');
                    }
                }
                index = 0;  // バッファクリア
            } else {
                if (index < BUFFER_SIZE - 1) {
                    inputBuffer[index++] = c;
                } else {
                    // バッファオーバーフロー対策（リセット）
                    index = 0;
                }
            }
        }
    }
}
