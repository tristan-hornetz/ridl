all: clean
	make notsx=TSX -C demos
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/kernel_module modules

notsx: clean
	make notsx=NOTSX -C demos
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/kernel_module modules

clean:
	make -C demos clean
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/kernel_module clean
