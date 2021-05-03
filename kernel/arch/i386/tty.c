#include <kernel/tty.h>

#include "vga.h"
#include "../../drivers/kbd.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

void terminal_initialize() {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = VGA_MEMORY;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
}

void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}

void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}

void terminal_scroll(){
    for(int i = 0; (long unsigned int)i < VGA_HEIGHT; i++){
        for (int m = 0; (long unsigned int)m < VGA_WIDTH; m++){
            terminal_buffer[i * VGA_WIDTH + m] = terminal_buffer[(i + 1) * VGA_WIDTH + m];
        }
    }
}

void terminal_clear_line(size_t y) {
	size_t x = 0;
	while (x < VGA_WIDTH) {
		terminal_putentryat(' ', terminal_color, x, y);
		x ++;
	}
}

void terminal_clearscreen(void) {
	size_t y = 0;
	while (y < VGA_HEIGHT) {
		terminal_clear_line(y);
		y ++;
	}
	terminal_row = 0;
	terminal_column = 0;
}

void terminal_putchar(char c) {
	if (terminal_row + 1 == VGA_HEIGHT) {
		terminal_scroll();
		terminal_row --;
		terminal_column = 0;
	} 

	
	if (c == '\n') {
		terminal_row ++;
		terminal_column = 0;
	} else {
		unsigned char uc = c;
		terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
		if (++terminal_column == VGA_WIDTH) {
			terminal_column = 0;
			terminal_row ++;
		}
	}
}

void terminal_write(const char* data, size_t size) {
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
	terminal_write(data, strlen(data));
}

int strcmp(const char *str1, const char *str2){
  int res = 1;
  int i = 0;
  if(strlen(str1) == strlen(str2)){
    while(str1[i] != 0x0A && str2[i] != 0x0A){
      if(str1[i] != str2[i]){
        res = 0;
      }
      i++;
    }
  }else{res = 0;}
  return res;
}

bool isascii(int c) {
	return c >= 0 && c < 128;
}

uint8_t kybrd_enc_read_buf () {
  return inb(KYBRD_ENC_INPUT_BUF);
}

uint8_t kybrd_ctrl_read_status(){
  return inb(KYBRD_CTRL_STATS_REG);
}

char getchar() {
	uint8_t code = 0;
	uint8_t key = 0;
	while(1) {
		if (kybrd_ctrl_read_status() & KYBRD_CTRL_STATS_MASK_OUT_BUF) {
			code = kybrd_enc_read_buf();
			if(code <= 0x58) {
				key = _kkybrd_scancode_std[code];
				break;
			}
		}
	}
	return key;
}

void getline(char* string, int len) {
	uint8_t i = 0;
	char temp = 0;
	memset(string, 0, len);
	
	while(i < len && temp != 0x0D) {
		temp = getchar();
		if(isascii(temp) && temp != 0x0D) {
			if(temp == 0x08) {
				terminal_column --;
				terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
				i --;
			} else {
				terminal_putchar(temp);
				string[i] = temp;
				i ++;
			}
		}
	}
	string[i] = 0x0A;
}

void prompt() {
	terminal_writestring("root@DaggerOS> ");
}

void hello_user(char* user) {
	terminal_setcolor(VGA_COLOR_BLUE);
	terminal_writestring("                          --- Welcome to DaggerOS ---\n");
	printf(              "                                   Hello, ");
	terminal_writestring(user);
	printf("\n");
	terminal_setcolor(VGA_COLOR_LIGHT_GREY);
}

void help() {
	terminal_writestring("---- HELP MENU ----\n");
  	terminal_writestring("Commands:\n");
  	terminal_writestring("  help -------> this menu\n");
  	terminal_writestring("  shutdown ---> power off machine\n");
  	terminal_writestring("  echo -------> type and receive a response\n");
  	terminal_writestring("  clear ------> clear screen\n");
	terminal_writestring("Warning: special keys (esc, ctrl, ...) will not work.\n");
}

void echo() {
	char string[50];
	terminal_writestring("Enter string: ");
	getline(string, 50);
	printf("\n");
	terminal_writestring(string);
}

void shutdown() {
	terminal_clearscreen();
	terminal_writestring("Shutting down...");
	__asm__ __volatile__("outw %1, %0" : : "dN" ((uint16_t)0xD004), "a" ((uint16_t)0x2000));
}

int get_command() {
	int cmd = 1;
	char string[10];
	getline(string, 10);
	if(strcmp(string,"help\x0D") == 1){
		cmd = 1;
	} else {
		if(strcmp(string,"shutdown\x0D") == 1) {
		cmd = 2;
		} else {
		if(strcmp(string,"echo\x0D") == 1){
			cmd = 3;
		} else { 
			if(strcmp(string,"clear\x0D") == 1) {
			cmd = 4;
			} else {
			cmd = 5;
			}
		}
		}
	}
	memset(string,0,10);
	terminal_writestring("\n");
	return cmd;
}

void execute_command(int cmd){
  switch(cmd){
    case 1:
      help();
      break;
    case 2:
      shutdown();
      break;
    case 3:
      echo();
      break;
    case 4:
      terminal_clearscreen();
      break;
    default: 
      terminal_writestring("[!]nsh: Command not found\n");
  }
}