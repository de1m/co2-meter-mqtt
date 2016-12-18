/*
 * shift-regist-7Seg.c
 *
 *  Created on: 18.12.2016
 *      Author: de1m
 */


/*
 * shiftregistr.c
 *
 *  Created on: 18.12.2016
 *      Author: de1m
 */

#include "shift-regist-7Seg.h"

#include <ets_sys.h>
#include <osapi.h>
#include <ets_sys.h>
#include <os_type.h>
#include <gpio.h>
#include <mem.h>

void ICACHE_FLASH_ATTR strob(void){
	gpio_output_set(BIT13, 0, BIT13, 0); //high
	gpio_output_set(0, BIT13, BIT13, 0); //low
}

void ICACHE_FLASH_ATTR latch(void){
	gpio_output_set(BIT12, 0, BIT12, 0); //high
	gpio_output_set(0, BIT12, BIT12, 0); //low
}

void ICACHE_FLASH_ATTR setOn(void){
	gpio_output_set(0, BIT14, BIT14, 0); //low
	strob();
}

void ICACHE_FLASH_ATTR setOff(void){
	gpio_output_set(BIT14, 0, BIT14, 0); //high
	strob();
}

void ICACHE_FLASH_ATTR writeNum(int num, uint8 custom){

	int num0 = 0b11111100;
	int num1 = 0b01100000;
	int num2 = 0b11011010;
	int num3 = 0b11110010;
	int num4 = 0b01100110;
	int num5 = 0b10110110;
	int num6 = 0b10111110;
	int num7 = 0b11100000;
	int num8 = 0b11111110;
	int num9 = 0b11110110;

	if(custom){
		//write custom int to display
		// 0b11101100 - N;
		// 0b10011100 - C;
		// 0b10001110 - F;
		// 0b10011110 - E

	} else {
		switch(num){
			case 0: num = num0;break;
			case 1: num = num1;break;
			case 2: num = num2;break;
			case 3: num = num3;break;
			case 4: num = num4;break;
			case 5: num = num5;break;
			case 6: num = num6;break;
			case 7: num = num7;break;
			case 8: num = num8;break;
			case 9: num = num9;break;
		}
	}

	int i;
	for(i = 0;i<8;i++){
		if ((num&(1<<i)) == 0){
			setOff();
			latch();

		} else {
			setOn();
			latch();
		}
	}

};

void ICACHE_FLASH_ATTR sendNumToDisp(uint32 num){

	if(num < 1000){
		writeNum(0b00000000,1);
		writeNum(0b00000000,1);
		writeNum(0b00000000,1);
		writeNum(0b00000000,1);
	}
    int temp, factor = 1;

    temp = num;
    while (temp)
    {
      temp = temp / 10;
      factor = factor * 10;
    }

    while (factor>1)
    {
      factor = factor / 10;
      writeNum(num / factor, 0);
      num = num % factor;
    }
    latch();
}
