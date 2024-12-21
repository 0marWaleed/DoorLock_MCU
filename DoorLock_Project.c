#include <reg51.h>
#include <stdio.h>
#include <intrins.h>

#define PWM_Period 0xB7FE
sbit Servo_Motor_Pin = P2^0;

// Define I2C pins
sbit scl = P2^2;
sbit sda = P2^1;

// Define numpad row pins
sbit r1 = P1^4;
sbit r2 = P1^5;
sbit r3 = P1^6;
sbit r4 = P1^7;

// Define numpad column pins
sbit c1 = P1^0;
sbit c2 = P1^1;
sbit c3 = P1^2;
sbit c4 = P1^3;

// Global variable for LCD slave address
unsigned char slave_add = 0x4E; // Adjust based on your LCD's I2C address

// Function prototypes
void i2c_start(void);
void i2c_stop(void);
void i2c_ACK(void);
void i2c_write(unsigned char);
void lcd_send_cmd(unsigned char);
void lcd_send_data(unsigned char);
void lcd_send_str(unsigned char *);
void lcd_slave(unsigned char);
void delay_ms(unsigned int);
void lcd_init(void);
unsigned char read_keypad(void);
void Timer_init(void);
void Set_DutyCycle_To(float);
void Timer0_ISR(void);

// Correct password and admin password
unsigned char correct_password[] = {1, 2, 3, 4};
unsigned char admin_password[] = {0, 0, 0, 0};
unsigned char entered_password[4];
unsigned char new_password[4];  // For storing new password entered by admin
unsigned char is_admin = 0;  // Flag to track if in admin mode
unsigned int ON_Period, OFF_Period, DutyCycle;

void Timer_init()
{
	TMOD = 0x01;		/* Timer0 mode1 */
	TH0 = (PWM_Period >> 8);/* 20ms timer value */
	TL0 = PWM_Period;
	TR0 = 1;		/* Start timer0 */
}

/* Timer0 interrupt service routine (ISR) */
void Timer0_ISR() interrupt 1	
{
	Servo_Motor_Pin = !Servo_Motor_Pin;
	if(Servo_Motor_Pin)
	{
		TH0 = (ON_Period >> 8);
		TL0 = ON_Period;
	}	
	else
	{
		TH0 = (OFF_Period >> 8);
		TL0 = OFF_Period;
	}			
}

/* Calculate ON & OFF period from duty cycle */
void Set_DutyCycle_To(float duty_cycle)
{
	float period = 65535 - PWM_Period;
	ON_Period = ((period/100.0) * duty_cycle);
	OFF_Period = (period - ON_Period);	
	ON_Period = 65535 - ON_Period;	
	OFF_Period = 65535 - OFF_Period;
}

// I2C Functions
void i2c_start(void) {
    sda = 1;
    scl = 1;
    sda = 0;
}

void i2c_stop(void) {
    scl = 0;
    sda = 0;
    scl = 1;
    sda = 1;
}

void i2c_ACK(void) {
    scl = 0;
    sda = 1;
    scl = 1;
    while (sda);
}

void i2c_write(unsigned char dat) {
    unsigned char i;
    for (i = 0; i < 8; i++) {
        scl = 0;
        sda = (dat & (0x80 >> i)) ? 1 : 0;
        scl = 1;
    }
}

void lcd_slave(unsigned char slave) {
    slave_add = slave;
}

void lcd_send_cmd(unsigned char cmd) {
    unsigned char cmd_l = (cmd << 4) & 0xF0;
    unsigned char cmd_u = cmd & 0xF0;

    i2c_start();
    i2c_write(slave_add);
    i2c_ACK();
    i2c_write(cmd_u | 0x0C);
    i2c_ACK();
    delay_ms(1);
    i2c_write(cmd_u | 0x08);
    i2c_ACK();
    delay_ms(10);
    i2c_write(cmd_l | 0x0C);
    i2c_ACK();
    delay_ms(1);
    i2c_write(cmd_l | 0x08);
    i2c_ACK();
    delay_ms(10);
    i2c_stop();
}

void lcd_send_data(unsigned char dataw) {
    unsigned char dataw_l = (dataw << 4) & 0xF0;
    unsigned char dataw_u = dataw & 0xF0;

    i2c_start();
    i2c_write(slave_add);
    i2c_ACK();
    i2c_write(dataw_u | 0x0D);
    i2c_ACK();
    delay_ms(1);
    i2c_write(dataw_u | 0x09);
    i2c_ACK();
    delay_ms(10);
    i2c_write(dataw_l | 0x0D);
    i2c_ACK();
    delay_ms(1);
    i2c_write(dataw_l | 0x09);
    i2c_ACK();
    delay_ms(10);
    i2c_stop();
}

void lcd_send_str(unsigned char *p) {
    while (*p != '\0') {
        lcd_send_data(*p++);
    }
}

void delay_ms(unsigned int n) {
    unsigned int m;
    for (; n > 0; n--) {
        for (m = 121; m > 0; m--);
    }
}

// LCD Initialization
void lcd_init() {
    lcd_send_cmd(0x02); // Return home
    lcd_send_cmd(0x28); // 4-b mode
    lcd_send_cmd(0x0C); // Display ON, Cursor OFF
    lcd_send_cmd(0x06); // Increment Cursor
    lcd_send_cmd(0x01); // Clear display
    delay_ms(10); // Wait for clear command
}

// Read numpad input
unsigned char read_keypad() {
    unsigned char row, col;
    unsigned char key_map[4][4] = {
        {1, 2, 3, 'A'},
        {4, 5, 6, 'B'},
        {7, 8, 9, 'C'},
        {'*', 0, '#', 'D'}
    };

    // Set all rows high initially
    r1 = 1; r2 = 1; r3 = 1; r4 = 1;

    // Loop through rows
    for (row = 0; row < 4; row++) {
        // Set one row low at a time
        if (row == 0) { r1 = 0; r2 = 1; r3 = 1; r4 = 1; }
        else if (row == 1) { r1 = 1; r2 = 0; r3 = 1; r4 = 1; }
        else if (row == 2) { r1 = 1; r2 = 1; r3 = 0; r4 = 1; }
        else if (row == 3) { r1 = 1; r2 = 1; r3 = 1; r4 = 0; }

        // Check columns
        if (c1 == 0) { col = 0; }
        else if (c2 == 0) { col = 1; }
        else if (c3 == 0) { col = 2; }
        else if (c4 == 0) { col = 3; }
        else { continue; } // No key pressed

        delay_ms(20); // Debounce delay
        if ((row == 0 && r1 == 0) || (row == 1 && r2 == 0) || 
            (row == 2 && r3 == 0) || (row == 3 && r4 == 0)) {
            while (c1 == 0 || c2 == 0 || c3 == 0 || c4 == 0); // Wait until the key is released
            return key_map[row][col];
        }
    }

    return 0xFF; // No key pressed
}

// Check entered password
unsigned char check_password(unsigned char *password) {
    unsigned char i;
    for (i = 0; i < 4; i++) {
        if (entered_password[i] != password[i]) {
            return 0; // Password incorrect
        }
    }
    return 1; // Password correct
}

// Main function
void main() {
    unsigned char key;
    unsigned char password_index = 0;
    unsigned char i;
	
    lcd_slave(0x4E); // Set LCD I2C slave address
    lcd_init(); // Initialize LCD
		
    lcd_send_cmd(0x80); 	// Set cursor to top line, first position
    lcd_send_str("Enter Pass:");	 // Display prompt
		EA  = 1;					/* Enable global  */
		ET0 = 1;         	/* Enable timer0  */
		Timer_init();

    while (1) {
        key = read_keypad(); // Read key from numpad

        if (key != 0xFF) { // If a valid key is pressed
            if (key == 'A') { // If 'A' is pressed
								is_admin = 1;
                lcd_send_cmd(0x01); // Clear display
                lcd_send_cmd(0x80); // Set cursor to top line, first position
                lcd_send_str("Admin Mode!"); // Prompt for admin password
								lcd_send_cmd(0xC0);
								lcd_send_str("Enter Pass:"); // Prompt for admin password
                password_index = 0; // Reset password index for admin mode
            }
            else if (key >= 0 && key <= 9) { // If the key is a digit
                entered_password[password_index++] = key;
                lcd_send_data('*'); // Display '*' for each digit

                if (password_index == 4) {  // If 4 digits entered
                    if (is_admin == 0 && check_password(correct_password)) {
                        lcd_send_cmd(0xC0); // Move to second line
                        lcd_send_str("Access Granted");
												delay_ms(1000);
											  lcd_send_cmd(0x01); // Clear display
												lcd_send_cmd(0x80); // Set cursor to top line, first position
												lcd_send_str("Door Opened!");
												
												Set_DutyCycle_To(7);/* 1.4ms(7%) of 20ms(100%) period */
												delay_ms(400);
												Set_DutyCycle_To(2);/* 2.4ms(12%) of 20ms(100%) period */
												delay_ms(5000);
											
											  lcd_send_cmd(0x01); // Clear display
												lcd_send_cmd(0x80); // Set cursor to top line, first position
												lcd_send_str("Closing Door...");
												delay_ms(800);
												Set_DutyCycle_To(7);/* 1.4ms(7%) of 20ms(100%) period */
												delay_ms(400);
                    } else if (is_admin == 1 && check_password(admin_password)) {
                        lcd_send_cmd(0xC0); // Admin access granted
                        lcd_send_str("Admin Mode");
                        is_admin = 1; // Enable Admin mode
												delay_ms(200);
                        // Admin mode: Prompt for new password
                        lcd_send_cmd(0x01); // Clear display
                        lcd_send_cmd(0x80); // Set cursor to top line, first position
                        lcd_send_str("New Pass:");

                        password_index = 0; // Reset password index
                        while (password_index < 4) {
                            key = read_keypad();
                            if (key != 0xFF) {
                                if (key >= 0 && key <= 9) {
                                    new_password[password_index++] = key;
                                    lcd_send_data('*'); // Display '*' for each digit
                                }
                            }
                        }
                        // Store new password
                        for (i = 0; i < 4; i++) {
                            correct_password[i] = new_password[i];
                        }
                        // Exit admin mode
                        lcd_send_cmd(0xC0);
                        lcd_send_str("Password Changed");
                        delay_ms(1200); // Wait for 1.5 seconds
                        is_admin = 0; // Reset Admin mode
                    } else {
                        lcd_send_cmd(0xC0); // Move to second line
                        lcd_send_str("Wrong Password");
                    }

                    delay_ms(2000); // Wait for 2 seconds
                    lcd_send_cmd(0x01); // Clear display
                    lcd_send_cmd(0x80); // Set cursor to top line, first position
                    if(is_admin){
                        lcd_send_str("Admin Pass:"); // Prompt for password again
                        password_index = 0; // Reset password index
                    }
                    else {
                        lcd_send_str("Enter Pass:"); // Prompt for password again
                        password_index = 0; // Reset password index
                    }
                }
            }
            else if (key == 'C') { // Clear entered digits
                password_index = 0; // Clear password
                lcd_send_cmd(0x01); // Clear display
                lcd_send_cmd(0x80); // Set cursor to top line, first position
                if (is_admin) {
                    lcd_send_str("Admin Mode!"); // Prompt for admin password
										lcd_send_cmd(0xC0);
										lcd_send_str("Enter Pass:"); 
                } else {
                    lcd_send_str("Enter Pass:"); // Normal password prompt
                }
            }
        }
    }
}
