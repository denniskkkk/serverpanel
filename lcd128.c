/*
 ============================================================================
 Name        : lcd128.c
 Author      : 
 Version     :
 Copyright   : 
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <math.h>
#include "ftdi.h"
#include "font.h"

unsigned char buf[1024];
struct ftdi_context ftdic;

void printhex(unsigned char *tmp2, int readed) {
	unsigned int z;
	if (readed > 0) {
		printf("readed %d, ", readed);
		for (z = 0; z < readed; z++) {
			printf(":%02X:", tmp2[z]);
		}
		printf("\n");
	}
}

int initchip() {
	int size = 0;
	int f;
	if (ftdi_init(&ftdic) < 0) {
		fprintf(stderr, "init failed \n");
		return (-1);
	}
	//
	//ftdi_set_interface(&ftdic, INTERFACE_A);
	f = ftdi_usb_open(&ftdic, 0x0403, 0x6014);
	//f = ftdi_usb_open_string(&ftdic, "i:0x0403:0x6014:0");
	if (f < 0 && f != -5) {
		fprintf(stderr, "unable to open device: %d (%s)\n", f,
				ftdi_get_error_string(&ftdic));
		exit(-1);
	}
	if (ftdi_usb_reset(&ftdic) < 0) {
		fprintf(stderr, "error\n");
		exit(-1);
	}
	//printf("ftdi port PRA open succeeded: %d\n", f);
	if (ftdi_setflowctrl(&ftdic, SIO_RTS_CTS_HS) < 0) {
		fprintf(stderr, "error\n");
		exit(-1);
	}
	if (ftdi_set_latency_timer(&ftdic, 5) < 0) {
		fprintf(stderr, "error\n");
		exit(-1);
	}
	if (ftdi_usb_purge_buffers(&ftdic) < 0) {
		fprintf(stderr, "error\n");
		exit(-1);
	}
	//printf("SBR enabling bitmode RESET(channel 1)\n");
	if (ftdi_set_bitmode(&ftdic, 0xfb, 0x00) < 0) {
		fprintf(stderr, "error\n");
		exit(-1);
	}
	//ftdi_set_bitmode(&ftdic, 0xFF, BITMODE_BITBANG);
	//printf("SBR enabling bitmode MPSSE mode\n");
	if (ftdi_set_bitmode(&ftdic, 0xfb, BITMODE_MPSSE) < 0) {
		fprintf(stderr, "error\n");
		exit(-1);
	}

	// bit
	// 0, TCLK
	// 1, Dout
	// 2, Din
	// 3, +CS
	// 4,
	// 5,
	// 6,
	// 7, -RESET
	size = 0;
	buf[size++] = 0x8b & 0xff; // disable divid by 5 60Mhz clock, 0x8B= enable
	buf[size++] = 0x97 & 0xff; // turn off adaptive clocking, 0x96=on
	buf[size++] = 0x8d & 0xff; // disable 3 phase, 0x8c=enable
	buf[size++] = 0x86 & 0xff; // set TCLK
	buf[size++] = 0x08 & 0xff; // lo clk/12 = Tclk=1000khz/500ns/500ns clock, 5v=2.5Mhz, 3.3v= 1.5Mhz
	buf[size++] = 0x00 & 0xff; // hi
	if (ftdi_write_data(&ftdic, buf, size) < 0) {
		fprintf(stderr, "error\n");
		exit(-1);
	}

	size = 0;
	fprintf(stdout, "reset low\n");
	buf[size++] = 0x80 & 0xff; // write AD
	buf[size++] = 0x00 & 0xff; // data = 0b10000000
	buf[size++] = 0xff & 0xff; // data all bit output
	if (ftdi_write_data(&ftdic, buf, size) < 0) {
		fprintf(stderr, "error\n");
		exit(-1);
	}

	size = 0;
	usleep(100000); // 500ms +reset
	fprintf(stdout, "reset high\n");
	buf[size++] = 0x80 & 0xff; // write AD
	buf[size++] = 0x80 & 0xff; // data = 0b10000000
	buf[size++] = 0xff & 0xff; // data all bit output
	if (ftdi_write_data(&ftdic, buf, size) < 0) {
		fprintf(stderr, "error\n");
		exit(-1);
	}
}

void writecmd(unsigned char cmd) {
	int size = 0;
	//fprintf (stdout, "cmd 0x%02X\n", cmd);
	buf[size++] = 0x80 & 0xff; // write AD
	buf[size++] = 0x88 & 0xff; // data = 0b10001000, +rst, +cs
	buf[size++] = 0xff & 0xff; // data all bit output

	buf[size++] = 0x11 & 0xff; // +ve clk data, MSB first
	buf[size++] = 0x02 & 0xff; // length lo; 3 bytes
	buf[size++] = 0x00 & 0xff; // length hi
	buf[size++] = 0xf8 & 0xff; // byte 1 , sync bits msb(11111), rw=0(w), rs=0 (cmd), 0b11111000
	buf[size++] = cmd & 0xf0;       // byte 2 , high byte
	buf[size++] = (cmd << 4) & 0xff; // byte 3 , low byte

	buf[size++] = 0x80 & 0xff; // write AD
	buf[size++] = 0x80 & 0xff; // data = 0b10000000, +rst, -cs
	buf[size++] = 0xff & 0xff; // data all bit output
	if (ftdi_write_data(&ftdic, buf, size) < 0) {
		fprintf(stderr, "error\n");
		exit(-1);
	}
	//usleep(80); // wait 72us
}

void writedata(unsigned char dat) {
	int size = 0;
	//fprintf (stdout, "dat 0x%02X\n", dat);
	buf[size++] = 0x80 & 0xff; // write AD
	buf[size++] = 0x88 & 0xff; // data = 0b10001000, +rst, +cs
	buf[size++] = 0xff & 0xff; // data all bit output

	buf[size++] = 0x11 & 0xff; // +ve clk data, MSB first
	buf[size++] = 0x02 & 0xff; // length lo; 3 bytes
	buf[size++] = 0x00 & 0xff; // length hi
	buf[size++] = 0xfa & 0xff; // byte 1 , sync bits msb(11111), rw=0(w), rs=1 (cmd), 0b11111010
	buf[size++] = dat & 0xf0;       // byte 2 , high byte
	buf[size++] = (dat << 4) & 0xff; // byte 3 , low byte

	buf[size++] = 0x80 & 0xff; // write AD
	buf[size++] = 0x80 & 0xff; // data = 0b10000000, +rst, -cs
	buf[size++] = 0xff & 0xff; // data all bit output
	if (ftdi_write_data(&ftdic, buf, size) < 0) {
		fprintf(stderr, "error\n");
		exit(-1);
	}
	//wait(80);   // wait 72us
}

void clearlcd() {
	writecmd(0x01);
	usleep(2000); // wait 2ms
}

void lcdhome() {
	writecmd(0x02);
	usleep(2000); // wait 2ms
}

void lcdinvert() {
	writecmd(0x07);
	usleep(72); // wait 2ms
}

void lcdinit() {
	writecmd(0x30); // 8bit data
	writecmd(0x30); // 8bit data
	writecmd(0x0c); // lcd on

	writecmd(0x01);
	usleep(10000);
	writecmd(0x06); // inc, no shift
	//writecmd (0x0f); // block, cursor on , flash
}

void settextrow(int row) {
	switch (row) {
	case 1:
		writecmd(0x80);
		break;
	case 2:
		writecmd(0x90);
		break;
	case 3:
		writecmd(0x88);
		break;
	case 4:
		writecmd(0x98);
		break;
	}
}
void lcdinitgraphic() {
	writecmd(0x30); // 8bit data
	writecmd(0x30); // 8bit data
	writecmd(0x0c); // lcd on / cursor off

	writecmd(0x01);
	usleep(10000);
	fprintf(stdout, "graphicmode\n");
	//writecmd (0x06); // inc, no shift
	// graphic display
	writecmd(0x24); // extend command, graphic off (00110110)
	writecmd(0x26); // graphic display on
	//writecmd (0x20); usleep (1000);
	//writecmd (0x24); usleep (1000);
	//writecmd (0x26); usleep (1000);
}

void lcddisablegraphic() {
	writecmd(0x20);
	usleep(1000);
}

void filltext() {
	clearlcd();
	int c, q;
	for (q = 0; q < 64; q++) {
		lcdhome();
		//clearlcd();
		for (c = 0; c < 64; c++) {
			//writedata ( q);
			writedata(' ' + c + q);
			//writedata (0x00);
		}
		usleep(200000);
	}
}

/*void cleargraphics() {
	int x, y;
	for (y = 0; y < 64; y++) // vertical 0-63 dot
			{
		if (y < 32) {
			writecmd(0x80 | y);
			writecmd(0x80);
		} else {
			writecmd(0x80 | (y - 32));
			writecmd(0x88);
		}
		for (x = 0; x < 8; x++)  // horizontal 0-127 dot
				{
			writedata(0x00); //left half 16X16
			writedata(0x00); //right half 16X16
		}
	}
}*/

void fillgraphics() {
	int x, y;
	for (y = 0; y < 64; y++) // vertical 0-63 dot
			{
		if (y < 32) {
			writecmd(0x80 | y);
			writecmd(0x80);
		} else {
			writecmd(0x80 | (y - 32));
			writecmd(0x88);
		}
		for (x = 0; x < 16; x++)  // horizontal 0-127 dot
				{
			if (y % 2 == 0) {
				writedata(0x55); //left half 16X16
			} else {
				writedata(0xaa); //right half 16X16
			}
		}
	}
}

unsigned linebuf[1024]; // 8 rows * 16 char (8x8)
void refscreen() {
	int x, y, addr = 0;
	for (y = 0; y < 64; y++) // vertical 0-63 dot
			{
		if (y < 32) {
			writecmd(0x80 | y);
			writecmd(0x80);
		} else {
			writecmd(0x80 | (y - 32));
			writecmd(0x88);
		}
		for (x = 0; x < 8; x++)  // horizontal 0-127 dot
				{
			writedata(linebuf[addr]);
			addr++;
			writedata(linebuf[addr]);
			addr++;
		}
	}
}

void refscreenex(int line) {
	int x, y, addr = line * 128;
	for (y = line * 8; y < line *8 + 8; y++) // vertical 0-63 dot
			{
		if (y < 32) {
			writecmd(0x80 | y);
			writecmd(0x80);
		} else {
			writecmd(0x80 | (y - 32));
			writecmd(0x88);
		}
		for (x = 0; x < 8; x++)  // horizontal 0-127 dot
			{
			writedata(linebuf[addr]);
			addr++;
			writedata(linebuf[addr]);
			addr++;
		}
	}
}

void testfill() {
	int n;
	for (n = 0; n < 1024; n++) {
		if ((n / 16) % 2 == 0) {
			linebuf[n] = 0x55;
		} else {
			linebuf[n] = 0xaa;
		}
	}
	refscreen();
}

void cleargraphics() {
	int n;
	for (n = 0; n < 1024; n++) {
			linebuf[n] = 0x00;
	}
	refscreen();
}

void printchar(char c, int x, int y) {
	//fprintf(stdout, "%d, %d, %c\n", x, y, c);
	int l;
	if (c > 127 ) {
		c = ' ';
	}
	for (l = 0; l < 8; l++) // vertical 0-63 dot
	{
		linebuf[x + (l * 16) + (y * 16 * 8)] = rev[ascii[c][l]]; //left half 8x8
	}
}

void printstring(char *str, int y) {
	int n = 0;
	while (*str != 0 && n < 16) {
		printchar(*str, n, y);
		str++;
		n++;
	}
	refscreenex( y );
}

int main(void) {
	int q = 0;
	char stmp [16];
	fprintf(stdout, "start LCD128\n");
	initchip();
	//usleep (10000);
	//lcdinit();
	//filltext ();
	lcdinitgraphic();
	cleargraphics();
	testfill();
	sleep (2);
	cleargraphics();
	sleep (2);
	printstring ("ABCDEFG12345abcd", 0);
	printstring ("1BCDEFG12345abc1", 1);
	printstring ("2BCDEFG12345abc2", 2);
	printstring ("3BCDEFG12345abc3", 3);
	printstring ("4BCDEFG12345abc4", 4);
	printstring ("5BCDEFG12345abc5", 5);
	while (1) {
	sprintf (stmp, "counter:%08d", q);
	printstring (stmp, 6);
	printstring ("7BCDEFG12345abc7", 7);
	q ++;
	usleep (1000000);
	}
	//while (1) {
	//fillgraphics ();
	//usleep (100000);
	//}
	fprintf(stdout, "completed\n");
	return EXIT_SUCCESS;
}
