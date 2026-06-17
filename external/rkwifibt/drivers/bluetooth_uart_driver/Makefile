ifneq ($(KERNELRELEASE),)
	obj-m		:= hci_uart.o
	hci_uart-y	:= hci_ldisc.o hci_h4.o hci_rtk_h5.o rtk_coex.o
	#EXTRA_CFLAGS += -DDEBUG

else
	PWD := $(shell pwd)
	KVER := $(shell uname -r)
	KDIR := /lib/modules/$(KVER)/build

all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_DIR) M=$(shell pwd) modules -j12 O=$(WIFI_BUILD_KERNEL_OBJ_DIR)
	$(CC_STRIP) --strip-unneeded $(shell pwd)/hci_uart.ko
	cp $(shell pwd)/hci_uart.ko $(M_OUT_DIR)
	#$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	rm -rf *.o *.mod.c *.mod.o *.ko *.symvers *.order *.a

endif
