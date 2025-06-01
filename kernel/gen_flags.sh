#!/bin/bash

KDIR="/usr/lib/modules/$(uname -r)/build"

cat > compile_flags.txt <<EOF
-nostdinc
-isystem /usr/lib/clang/19/include
-I$KDIR/include
-I$KDIR/arch/x86/include
-I$KDIR/arch/x86/include/generated
-I$KDIR/include/generated
-I$KDIR/include/uapi
-I$KDIR/arch/x86/include/uapi
-I$KDIR/include/generated/uapi
-I$KDIR/arch/x86/include/generated/uapi
-D__KERNEL__
-DMODULE
-std=gnu11
EOF
