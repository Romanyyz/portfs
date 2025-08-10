obj-m += portfs.o
portfs-objs := super.o inode.o file.o storage.o extent_tree.o extent_alloc.o directory.o

PWD := $(CURDIR)
KBUILD_CFLAGS += -I../common/

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
