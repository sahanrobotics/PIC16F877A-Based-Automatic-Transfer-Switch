

#define _XTAL_FREQ 8000000

#include <xc.h>
#include <stdio.h>

// =============================================================================
// CONFIGURATION BITS
// =============================================================================
#pragma config FOSC = HS, WDTE = OFF, PWRTE = OFF, BOREN = ON, LVP = OFF, CPD = OFF, WRT = OFF, CP = OFF

// =============================================================================
// HARDWARE PIN DEFINITIONS
// =============================================================================
#define RS RD2
#define EN RD3
#define D4 RD4
#define D5 RD5
#define D6 RD6
#define D7 RD7
#define BUZZER            RD1
#define GEN_ON_INPUT      RA1
#define CEB_ON_INPUT      RA0
#define GEN_RELAY         RB0
#define CEB_RELAY         RB1
#define GEN_START_RELAY   RB2

// =============================================================================
// LCD DRIVER (Integrated)
// =============================================================================
void Lcd_Port(char a);
void Lcd_Cmd(char a);
void Lcd_Clear();
void Lcd_Set_Cursor(char a, char b);
void Lcd_Init();
void Lcd_Write_Char(char a);
void Lcd_Write_String(char *a);
void Lcd_Send_Full_Byte_Cmd(char cmd);

void Lcd_Port(char a) {
	if(a & 1) D4=1; else D4=0; if(a & 2) D5=1; else D5=0;
	if(a & 4) D6=1; else D6=0; if(a & 8) D7=1; else D7=0;
}
void Lcd_Cmd(char a) { RS=0; Lcd_Port(a); EN=1; __delay_ms(4); EN=0; }
void Lcd_Send_Full_Byte_Cmd(char cmd) { Lcd_Cmd(cmd >> 4); Lcd_Cmd(cmd & 0x0F); }
void Lcd_Clear() { Lcd_Send_Full_Byte_Cmd(0x01); __delay_ms(2); }
void Lcd_Set_Cursor(char a, char b) {
	char temp = (a == 1) ? (0x80 + b - 1) : (0xC0 + b - 1);
    Lcd_Send_Full_Byte_Cmd(temp);
}
void Lcd_Init() {
    Lcd_Port(0x00); __delay_ms(20); Lcd_Cmd(0x03); __delay_ms(5); Lcd_Cmd(0x03);
    __delay_ms(11); Lcd_Cmd(0x03); Lcd_Cmd(0x02); Lcd_Send_Full_Byte_Cmd(0x28);
    Lcd_Send_Full_Byte_Cmd(0x0C); Lcd_Send_Full_Byte_Cmd(0x06); Lcd_Clear();
}
void Lcd_Write_Char(char a) {
   RS=1; Lcd_Port(a >> 4); EN=1; __delay_us(40); EN=0;
   Lcd_Port(a & 0x0F); EN=1; __delay_us(40); EN=0;
}
void Lcd_Write_String(char *a) { for(int i=0; a[i]!='\0'; i++) Lcd_Write_Char(a[i]); }

// =============================================================================
// ATS APPLICATION LOGIC
// =============================================================================
#define CEB_FAIL_DELAY        3
#define GEN_START_CRANK_TIME    7
#define GEN_WARMUP_TIME         10
#define CEB_RETURN_DELAY      15
#define GEN_COOLDOWN_TIME       30

typedef enum {
    STATE_INIT, STATE_ON_CEB, STATE_CEB_FAILED, STATE_STARTING_GEN,
    STATE_GEN_WARMING_UP, STATE_ON_GENERATOR, STATE_CEB_RETURNED,
    STATE_GEN_COOLDOWN, STATE_GEN_START_FAIL, STATE_GEN_RUNTIME_FAIL
} ATS_State;

ATS_State currentState = STATE_INIT;
ATS_State previousDisplayState = -1;
unsigned int timer_seconds = 0;

const unsigned char ceb_char[8]   = {0x0E,0x1F,0x1F,0x0E,0x04,0x0E,0x11,0x0E};
const unsigned char gen_char[8]   = {0x00,0x1F,0x11,0x11,0x1F,0x1B,0x1B,0x00};
const unsigned char arrow_char[8] = {0x00,0x04,0x06,0x1F,0x06,0x04,0x00,0x00};
#define CEB_CHAR_CODE   0
#define GEN_CHAR_CODE   1
#define ARROW_CHAR_CODE 2

void System_Init(void);
void Create_Custom_Chars(void);
void Buzzer_Beep(unsigned int ms);
void Update_Display(void);
void ATS_StateMachine_Run(void);

// =============================================================================
// MAIN PROGRAM
// =============================================================================
void main(void) {
    System_Init();
    Lcd_Clear(); Lcd_Set_Cursor(1, 2); Lcd_Write_String("ATS Controller");
    Lcd_Set_Cursor(2, 4); Lcd_Write_String("Starting...");
    Buzzer_Beep(200); __delay_ms(2000);
    currentState = STATE_ON_CEB;
    while (1) {
        ATS_StateMachine_Run();
        Update_Display();
        __delay_ms(1000);
        if(timer_seconds > 0) timer_seconds--;
    }
}

void System_Init(void) {
    ADCON1 = 0x07; CMCON = 0x07;
    TRISA = 0xFF; TRISB = 0x00; TRISD = 0x00;
    GEN_RELAY = 0; GEN_START_RELAY = 0; CEB_RELAY = 0; BUZZER = 0;
    Lcd_Init(); Create_Custom_Chars();
}

void Create_Custom_Chars(void) {
    Lcd_Send_Full_Byte_Cmd(0x40);
    for (int i = 0; i < 8; i++) Lcd_Write_Char(ceb_char[i]);
    for (int i = 0; i < 8; i++) Lcd_Write_Char(gen_char[i]);
    for (int i = 0; i < 8; i++) Lcd_Write_Char(arrow_char[i]);
}

void Buzzer_Beep(unsigned int ms) {
    BUZZER = 1; for (unsigned int i = 0; i < ms; i++) __delay_ms(1); BUZZER = 0;
}

// =============================================================================
// MAIN ATS STATE MACHINE (with Auto-Recovery)
// =============================================================================
void ATS_StateMachine_Run(void) {
    unsigned char is_ceb_on = CEB_ON_INPUT;
    unsigned char is_gen_on = GEN_ON_INPUT;

    switch (currentState) {
        case STATE_ON_CEB:
            CEB_RELAY = 1; GEN_RELAY = 0; GEN_START_RELAY = 0;
            if (!is_ceb_on) {
                currentState = STATE_CEB_FAILED; timer_seconds = CEB_FAIL_DELAY; Buzzer_Beep(500);
            }
            break;
        case STATE_CEB_FAILED:
            CEB_RELAY = 0;
            if (timer_seconds == 0) {
                currentState = STATE_STARTING_GEN; timer_seconds = GEN_START_CRANK_TIME;
            }
            if (is_ceb_on) { currentState = STATE_ON_CEB; }
            break;
        case STATE_STARTING_GEN:
            GEN_START_RELAY = 1;
            if (is_gen_on) {
                GEN_START_RELAY = 0;
                currentState = STATE_GEN_WARMING_UP; timer_seconds = GEN_WARMUP_TIME;
            } else if (timer_seconds == 0) {
                GEN_START_RELAY = 0;
                if (!is_gen_on) { currentState = STATE_GEN_START_FAIL; }
                else { currentState = STATE_GEN_WARMING_UP; timer_seconds = GEN_WARMUP_TIME; }
            }
            break;
        case STATE_GEN_WARMING_UP:
            if (timer_seconds == 0) { currentState = STATE_ON_GENERATOR; Buzzer_Beep(100); }
            break;
        case STATE_ON_GENERATOR:
            GEN_RELAY = 1; CEB_RELAY = 0;
            if (!is_gen_on) {
                GEN_RELAY = 0; Buzzer_Beep(1000);
                currentState = STATE_GEN_RUNTIME_FAIL;
            } else if (is_ceb_on) {
                currentState = STATE_CEB_RETURNED; timer_seconds = CEB_RETURN_DELAY; Buzzer_Beep(150);
            }
            break;
        case STATE_CEB_RETURNED:
             if (timer_seconds == 0) {
                GEN_RELAY = 0; __delay_ms(500); CEB_RELAY = 1;
                currentState = STATE_GEN_COOLDOWN; timer_seconds = GEN_COOLDOWN_TIME;
            }
             if (!is_ceb_on) { currentState = STATE_ON_GENERATOR; }
            break;
        case STATE_GEN_COOLDOWN:
            if (!is_ceb_on) {
                Buzzer_Beep(300); CEB_RELAY = 0; __delay_ms(500); GEN_RELAY = 1;
                currentState = STATE_ON_GENERATOR;
            }
            else if (timer_seconds == 0) { currentState = STATE_ON_CEB; }
            break;

        // --- SELF-HEALING LOGIC IS HERE ---
        case STATE_GEN_START_FAIL:
        case STATE_GEN_RUNTIME_FAIL:
            // Check for the recovery condition: Has CEB power returned?
            if (is_ceb_on) {
                // YES! Recover the system automatically.
                BUZZER = 0; // Stop the alarm.
                Buzzer_Beep(250); // Short beep to signal recovery.
                currentState = STATE_ON_CEB; // Go to the safe CEB state.
            } else {
                // NO, CEB is still off. Continue the alarm.
                GEN_RELAY = 0; CEB_RELAY = 0; GEN_START_RELAY = 0;
                BUZZER = !BUZZER; // Intermittent beep
            }
            break;
        // --- END OF SELF-HEALING LOGIC ---

        default:
            currentState = STATE_ON_CEB;
            break;
    }
}

// =============================================================================
// LCD UPDATE FUNCTION (No changes needed)
// =============================================================================
void Update_Display(void) {
    char buffer[17];
    if (currentState == previousDisplayState && currentState != STATE_GEN_START_FAIL && currentState != STATE_GEN_RUNTIME_FAIL) {
        if(currentState == STATE_CEB_FAILED || currentState == STATE_STARTING_GEN ||
           currentState == STATE_GEN_WARMING_UP || currentState == STATE_CEB_RETURNED ||
           currentState == STATE_GEN_COOLDOWN)
        {
            Lcd_Set_Cursor(2, 14); sprintf(buffer, "%02uS", timer_seconds); Lcd_Write_String(buffer);
        }
        return;
    }
    previousDisplayState = currentState;
    if (currentState != STATE_GEN_START_FAIL && currentState != STATE_GEN_RUNTIME_FAIL) Lcd_Clear();

    switch (currentState) {
        case STATE_ON_CEB:
            Lcd_Set_Cursor(1,1); Lcd_Write_Char(CEB_CHAR_CODE); Lcd_Write_String(" CEB");
            Lcd_Set_Cursor(1,10); Lcd_Write_Char(ARROW_CHAR_CODE); Lcd_Write_String(" LOAD");
            Lcd_Set_Cursor(2,1); Lcd_Write_String("Gen Status: IDLE");
            break;
        case STATE_CEB_FAILED:
            Lcd_Set_Cursor(1,1); Lcd_Write_String("!!  CEB FAIL  !!");
            Lcd_Set_Cursor(2,1); Lcd_Write_String("Starting Gen in ");
            break;
        case STATE_STARTING_GEN:
            Lcd_Set_Cursor(1,1); Lcd_Write_String("Source: <NONE>");
            Lcd_Set_Cursor(2,1); Lcd_Write_String("Cranking Gen... ");
            break;
        case STATE_GEN_WARMING_UP:
            Lcd_Set_Cursor(1,1); Lcd_Write_Char(GEN_CHAR_CODE); Lcd_Write_String(" Gen Running");
            Lcd_Set_Cursor(2,1); Lcd_Write_String("Warming up...   ");
            break;
        case STATE_ON_GENERATOR:
            Lcd_Set_Cursor(1,1); Lcd_Write_Char(GEN_CHAR_CODE); Lcd_Write_String(" GENERATOR");
            Lcd_Set_Cursor(1,10); Lcd_Write_Char(ARROW_CHAR_CODE); Lcd_Write_String(" LOAD");
            Lcd_Set_Cursor(2,1); Lcd_Write_String("CEB Status: OFF");
            break;
        case STATE_CEB_RETURNED:
            Lcd_Set_Cursor(1,1); Lcd_Write_Char(CEB_CHAR_CODE); Lcd_Write_String(" CEB Stable");
            Lcd_Set_Cursor(2,1); Lcd_Write_String("Transfer in...  ");
            break;
        case STATE_GEN_COOLDOWN:
            Lcd_Set_Cursor(1,1); Lcd_Write_String("Source: CEB");
            Lcd_Set_Cursor(2,1); Lcd_Write_String("Gen Cooldown... ");
            break;
        case STATE_GEN_START_FAIL:
            Lcd_Clear(); Lcd_Set_Cursor(1, 1); Lcd_Write_String("!!  CRITICAL  !!");
            Lcd_Set_Cursor(2, 1); Lcd_Write_String("GEN START FAILED");
            break;
        case STATE_GEN_RUNTIME_FAIL:
            Lcd_Clear(); Lcd_Set_Cursor(1, 1); Lcd_Write_String("!!  CRITICAL  !!");
            Lcd_Set_Cursor(2, 1); Lcd_Write_String("GEN RUN FAILURE");
            break;
    }
}