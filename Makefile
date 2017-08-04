obj-m+=spisw.o

all:
	@echo " module	:	compile the kernel module"
	@echo " load	:	install the kernel module"
	@echo " nload	:	compile and install the kernel module"
	@echo " unload  :	unload the kernel module"
	@echo " test  	:	test application"

module: spisw.c
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	size spisw.ko

load: spisw.ko
	insmod spisw.ko
	modinfo spisw.ko

nload: module load

unload:
	rmmod spisw.ko
	dmesg | tail

test: test.c
	gcc -o spisw_test test.c

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
