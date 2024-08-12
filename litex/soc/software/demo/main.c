// This file is Copyright (c) 2020 Florent Kermarrec <florent@enjoy-digital.fr>
// License: BSD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <irq.h>
#include <libbase/uart.h>
#include <liblitedram/sdram.h>
#include <libbase/console.h>
#include <generated/csr.h>

#include <liblitesdcard/spisdcard.h>
#include <libfatfs/ff.h>
/*-----------------------------------------------------------------------*/
/* Uart                                                                  */
/*-----------------------------------------------------------------------*/

static char *readstr(void)
{
	char c[2];
	static char s[64];
	static int ptr = 0;

	if(readchar_nonblock()) {
		c[0] = getchar();
		c[1] = 0;
		switch(c[0]) {
			case 0x7f:
			case 0x08:
				if(ptr > 0) {
					ptr--;
					fputs("\x08 \x08", stdout);
				}
				break;
			case 0x07:
				break;
			case '\r':
			case '\n':
				s[ptr] = 0x00;
				fputs("\n", stdout);
				ptr = 0;
				return s;
			default:
				if(ptr >= (sizeof(s) - 1))
					break;
				fputs(c, stdout);
				s[ptr] = c[0];
				ptr++;
				break;
		}
	}

	return NULL;
}

static char *get_token(char **str)
{
	char *c, *d;

	c = (char *)strchr(*str, ' ');
	if(c == NULL) {
		d = *str;
		*str = *str+strlen(*str);
		return d;
	}
	*c = 0;
	d = *str;
	*str = c+1;
	return d;
}

static void prompt(void)
{
	printf("\e[92;1mlitex-demo-app\e[0m> ");
}

/*-----------------------------------------------------------------------*/
/* Help                                                                  */
/*-----------------------------------------------------------------------*/

static void help(void)
{
	puts("\nLiteX minimal demo app built "__DATE__" "__TIME__"\n");
	puts("Available commands:");
	puts("help               - Show this command");
	puts("reboot             - Reboot CPU");
#ifdef CSR_LEDS_BASE
	puts("led                - Led demo");
#endif
	puts("donut              - Spinning Donut demo");
	puts("helloc             - Hello C");
#ifdef WITH_CXX
	puts("hellocpp           - Hello C++");
#endif
	puts("bench              - MMC Benchmark");
}

/*-----------------------------------------------------------------------*/
/* Commands                                                              */
/*-----------------------------------------------------------------------*/

static void reboot_cmd(void)
{
	ctrl_reset_write(1);
}

#ifdef CSR_LEDS_BASE
static void led_cmd(void)
{
	int i;
	printf("Led demo...\n");

	printf("Counter mode...\n");
	for(i=0; i<32; i++) {
		leds_out_write(i);
		busy_wait(100);
	}

	printf("Shift mode...\n");
	for(i=0; i<4; i++) {
		leds_out_write(1<<i);
		busy_wait(200);
	}
	for(i=0; i<4; i++) {
		leds_out_write(1<<(3-i));
		busy_wait(200);
	}

	printf("Dance mode...\n");
	for(i=0; i<4; i++) {
		leds_out_write(0x55);
		busy_wait(200);
		leds_out_write(0xaa);
		busy_wait(200);
	}
	printf("Dhruvin mode...\n");
	for(i=0; i<4; i++) {
		leds_out_write(0x55);
		busy_wait(200);
		leds_out_write(0xaa);
		busy_wait(200);
	}
}
#endif

extern void donut(void);

static void donut_cmd(void)
{
	printf("Donut demo...\n");
	donut();
}

extern void helloc(void);

static void helloc_cmd(void)
{
	printf("Hello C demo...\n");
	helloc();
}

#ifdef WITH_CXX
extern void hellocpp(void);

static void hellocpp_cmd(void)
{
	printf("Hello C++ demo...\n");
	hellocpp();
}
#endif

static unsigned long  rdcycle(){
	unsigned long cycles;
	// asm volatile("rdcycle %0" : "=r" (cycles));
	timer0_uptime_latch_write(1);
	cycles = timer0_uptime_cycles_read();
	return cycles;
}

static void bench_cmd(void)
{
	sdram_controller_bandwidth_update_write(0x1L);
	int tmp[1000];
	for(int i = 0; i < 1000; i++){
		tmp[i] = tmp[i] + 1;
	}
	
	printf("Clock Freq: %d\n", CONFIG_CLOCK_FREQUENCY);
	sdram_controller_bandwidth_update_write(0x1L);
	// uint32_t update = sdram_controller_bandwidth_update_read();
	uint32_t nreads = sdram_controller_bandwidth_nreads_read();
	uint32_t nwrites = sdram_controller_bandwidth_nwrites_read();
	uint32_t data_width = sdram_controller_bandwidth_data_width_read();
	printf("DRAM Bandwidth...\n");
	// printf("UPDATE:     %ld\n", update);
	printf("NWRITES:     %ld\n", nreads);
	printf("MAX NWRITES:    %ld\n", nwrites);
	printf("DATA WIDTH: %ld\n", data_width);
	printf("UPTIME: %ld\n", rdcycle()/CONFIG_CLOCK_FREQUENCY);
	// for(int i = 0; i < 1000; i++){
	// 	busy_wait(200);
	// 	for(int i = 0; i < 1000; i++){
	// 		tmp[i] = tmp[i] + 1;
	// 	}
	// 	sdram_controller_bandwidth_update_write(0x1L);
	// 	// uint32_t update = sdram_controller_bandwidth_update_read();
	// 	uint32_t nreads = sdram_controller_bandwidth_nreads_read();
	// 	uint32_t nwrites = sdram_controller_bandwidth_nwrites_read();
	// 	uint32_t data_width = sdram_controller_bandwidth_data_width_read();

	// 	if(nreads != 1 || nwrites != 1){
	// 		printf("DRAM Bandwidth...\n");
	// 		// printf("UPDATE:     %ld\n", update);
	// 		printf("NREADS:     %ld\n", nreads);
	// 		printf("NWRITES:    %ld\n", nwrites);
	// 		printf("DATA WIDTH: %ld\n", data_width);
	// 	}
	// }
	// printf("MMC Benchmark...\n");
	// spisdcard_init();
	// fatfs_set_ops_spisdcard();


	// FATFS FatFs;

	// FIL fil;        /* File object */
    // char line[4096]; /* Line buffer */
    // FRESULT fr;     /* FatFs return code */
	// UINT bw;

	// unsigned long start, end;

    // /* Give a work area to the default drive */
    // f_mount(&FatFs, "", 0);

    // /* Open a text file */
    // fr = f_open(&fil, "Image", FA_READ);
    // if(fr != FR_OK){
	// 	printf("Error while opening file. FRESULT: %d\n", fr);
	// }
	// unsigned long bytesRead = 0;

	// start = rdcycle();
	// while((fr = f_read(&fil, line, sizeof(line), &bw)) == FR_OK && bw != 0) {
	// 	// printf("Bytes read %d", bw);
	// 	// printf(line);
	// 	bytesRead += bw;
	// }

	// end = rdcycle();



	// if(fr != FR_OK) {
    //     printf("Error while reading file. FRESULT: %d\n", fr);
    // }else{
	// 	unsigned long timeTaken = (end-start)/CONFIG_CLOCK_FREQUENCY;
	// 	unsigned int bandwidth = bytesRead/timeTaken/1000;
	// 	printf("READ: Time taken: %lu seconds, data: %luB, bandwidth: %d kB/s \n", timeTaken, bytesRead, bandwidth);

	// }
	// return;



	// /* Open a text file */
	// FIL fil2;
    // fr = f_open(&fil2, "0:text.txt", FA_CREATE_ALWAYS | FA_WRITE );
    // if(fr != FR_OK){
	// 	printf("Error while opening file. FRESULT: %d\n", fr);
	// 	return;
	// }
	// unsigned long bytesWritten = 0;
	// unsigned long fileSize = 10*1024*1024;

	// start = rdcycle();
	// // int res = f_puts(line, &fil2); //(&fil2, line, sizeof(line), &bw);
	// // if(res < 0){
	// // 	printf("Error while writing file. FRESULT: %d\n", res);
	// // 	return;
	// // }else{
	// // 	// f_sync(&fil2);	
	// // }

	// fr = f_write(&fil2, line, sizeof(line), &bw);

	// if(fr != FR_OK){
	// 	printf("Error while writing file. FRESULT: %d\n", fr);
	// 	return;
	// }
	// // while((fr = f_write(&fil2, line, sizeof(line), &bw)) == FR_OK && bw == sizeof(line) && bytesWritten <= fileSize) {
	// // 	printf("Bytes read %d", bw);
	// // 	// printf(line);
	// // 	bytesWritten += bw;
	// // }
	// // f_sync(&fil2);

	// end = rdcycle();



	
	// unsigned int timeTaken = (end-start)/CONFIG_CLOCK_FREQUENCY;
	// unsigned int bandwidth = bytesWritten/timeTaken/1000;
	// printf("WRITE: Cycles taken: %d seconds, data: %luB, bandwidth: %d kB/s \n", timeTaken, bytesWritten, bandwidth);





    // /* Close the file */
    // // f_close(&fil2);
	// f_unmount("0:");

	
	
}

/*-----------------------------------------------------------------------*/
/* Console service / Main                                                */
/*-----------------------------------------------------------------------*/

static void console_service(void)
{
	
	char *str;
	char *token;

	str = readstr();
	if(str == NULL) return;
	token = get_token(&str);
	if(strcmp(token, "help") == 0)
		help();
	else if(strcmp(token, "reboot") == 0)
		reboot_cmd();
#ifdef CSR_LEDS_BASE
	else if(strcmp(token, "led") == 0)
		led_cmd();
#endif
	else if(strcmp(token, "donut") == 0)
		donut_cmd();
	else if(strcmp(token, "helloc") == 0)
		helloc_cmd();
#ifdef WITH_CXX
	else if(strcmp(token, "hellocpp") == 0)
		hellocpp_cmd();
#endif
	else if(strcmp(token, "bench") == 0)
		bench_cmd();


	prompt();
}

int main(void)
{
#ifdef CONFIG_CPU_HAS_INTERRUPT
	irq_setmask(0);
	irq_setie(1);
#endif
	uart_init();

	help();
	prompt();


	while(1) {
		console_service();
	}

	return 0;
}
