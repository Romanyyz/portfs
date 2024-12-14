obj-m += portfs.o
portfs-objs := super.o inode.o file.o storage.o

PWD := $(CURDIR)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell unsame -r)/build M=$(PWD) clean
