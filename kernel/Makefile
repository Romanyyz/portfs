obj-m += portfs.o
portfs-objs := super.o inode.o file.o storage.o

PWD := $(CURDIR)
PROJECT_ROOT := $(abspath ..)
EXTRA_CFLAGS += -I$(PROJECT_ROOT)/common

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
