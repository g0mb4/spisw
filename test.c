/* test.c - Test applicaton for the SPI software driver
 *
 * Driver has to be loaded!
 * Writes the bíte 0xAA to the device then reads the output byte.
 *
 * 2015, gmb */

#include <stdio.h>
#include <stdint.h>

#include <fcntl.h>
#include <sys/ioctl.h>

#include "spisw/spisw.h"

#define SPI_DEVICE       "/dev/spisw"

int
main(void){

	int spifd;

	if((spifd = open(SPI_DEVICE, O_RDWR)) < 0){
		fprintf(stderr, "open() failed\n");
		return 1;
	}

	if(ioctl(spifd, SPISW_INIT, 0) == -1){
		fprintf(stderr, "ioctl() failed on SPISW_INIT\n");
		return 2;
	}

	uint8_t w = 0xaa;
	ioctl(spifd, SPISW_W_BYTE, w);
	printf("written byte : %#02x\n", w);

	uint8_t r = ioctl(spifd, SPISW_R_BYTE, 0);
	printf("read byte : %#02x\n", r);

	close(spifd);

	return 0;
}
