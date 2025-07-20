# portfs — Portable Linux Filesystem on a Regular File

![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)
![Language: C](https://img.shields.io/badge/language-C-blue.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![Status: WIP](https://img.shields.io/badge/status-Work_in_Progress-yellow)

## Status

**Work in progress. Not production-ready.**

## Overview

`portfs` is an experimental filesystem for Linux designed to operate on a large file residing within an existing host filesystem, rather than a traditional block device.
This project is primarily an educational endeavor aimed at understanding Linux kernel internals, module development, and low-level kernel-level C programming.

While portfs is currently under active development and not yet suitable for production use,
it already demonstrates significant functionality. Future plans include potential real-world applications.

Project is actively developed and used as a playground for deep kernel-space exploration.
Design is evolving as the author learns and improves the internal architecture.

## Implemented Features

`portfs` already supports all core file operations, allowing users to interact with data within its storage file just like a regular directory:
- Full File Operations: Create, delete, copy, move, and search for files.
- Linux Utility Compatibility: Standard commands like ls (in the root directory), cp, cmp, mv, hexdump, cat, and dd work correctly.
- Application Compatibility: Files stored within portfs can be opened by standard applications such as text editors or image viewers.
- Currently supports only single root directory.

## Technologies and Approaches Used

- Linux Kernel and C: The core of the project is kernel module written in C to interface directly with the kernel and VFS.
- C++20 for Userspace Utility: A modern C++ utility is used for convenient user interaction and formatting the storage file.
- Block Addressing and Extents: Fundamental approaches for efficient management of storage space.
- Custom Block Allocator: portfs uses its own block allocator, based on the Red-Black Tree (`rb_tree`) implementation from the Linux kernel.

## Planned Enhancements

The project is continuously evolving, with the following key improvements planned for future releases:
- Full Directory Support: Expanding functionality to allow for more complex and nested directory structures.
- Kernel Page Cache Integration: Integrating with the kernel's page cache mechanism for significant performance improvements, enhanced responsiveness, and mmap support.
- Built-in Encryption: The option to encrypt the contents of the storage file during formatting, enhancing data security.
- Journaling: Implementing a journaling system to ensure data integrity and recovery after system crashes.
- Testing: Adding different ways to test the FS such as bash scripts and custom C programs to ensure correctness and stability.

## Current Limitations and Compromises

The current version of portfs includes some temporary solutions that will be refined:
- Metadata Loading: Currently, all filesystem metadata is loaded into RAM upon mounting. Future optimizations will focus on loading only necessary metadata on demand.
- Lack of Locking: Shared data is currently not protected by locking mechanisms.
  This will be addressed in future versions by implementing spinlocks, mutexes, or other suitable synchronization primitives to ensure thread safety.

## Building and Running

Follow these instructions to build and run portfs:

### Requirements
- Operating System: Linux (latest stable version recommended). Tested on kernel 6.13 and 6.14 and `EndeavourOS`, `CachyOS` distros.
- Compiler: GCC (latest version).
- Build Tool: make.
- Linux kernel source tree (required for building the kernel module).

### Building the Project

1. Clone the repository:
```bash
git clone git@github.com:Romanyyz/portfs.git
cd portfs
```
2. Build the user-space utility:
```bash
cd user
make
cd ..
```
3. Build the kernel module:
```bash
cd kernel
make
cd ..
```

### Running and Using

1. Load the kernel module:
   Navigate to the kernel directory and load the portfs.ko module (may require superuser privileges):
```bash
sudo insmod portfs.ko
# Or sudo modprobe portfs
```
2. Create and format the storage file:
  Use the `portfs_tool` user-space utility to create a file that will serve as your filesystem image. Follow the interactive prompts:
```bash
user/portfs_tool.out
```
3. Mount portfs:
   Create a mount point and mount the filesystem:
``` bash
mkdir -p /mnt/portfs_dir
sudo mount -t portfs none /mnt/portfs_dir/ -o path=/var/tmp/storage.pfs
```
Replace `/var/tmp/storage.pfs` with the path to the file created in the previous step.

4. Unmount portfs:
  Always unmount the filesystem when you are finished:
```bash
sudo umount /mnt/portfs_dir
# After unmounting, you can unload the kernel module:
# sudo rmmod portfs
```

## Contact

If you have any questions or suggestions, feel free to reach out:

Email: roman.khokhlachov@tuta.io
