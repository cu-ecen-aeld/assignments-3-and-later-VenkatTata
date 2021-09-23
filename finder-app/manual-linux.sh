#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.
#Modified by : Venkat Tata
#modified on: 09/12/2021
#brief: Completed the TODO section to build the kernel image

set -e
set -u

CROSS_COMPILE=aarch64-none-linux-gnu-
OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64


if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    #Deep cleaning the kernel build tree and removing .config file with any existing configurations
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} mrproper
    
    #Configure for the "virt" arm board that is simulated by QEMU
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} defconfig
    
    #Building a kernel image for booting with QEMU since QEMU does not boot up automatically
    make -j8 ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} all
    
    #Build any kernel modules
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} modules
    
    #Build the device tree
    make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} dtbs
	
	echo "Adding the Image in outdir"
	cp ./arch/${ARCH}/boot/Image ${OUTDIR}/
fi 

    


echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

mkdir ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
# TODO: Create necessary base directories
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
	make defconfig
else
    cd busybox
fi

# TODO: Make and insatll busybox
#Making busybox which compiles with cross compiler
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

#Installing busybox
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

#Change directory to root file system to add library dependencies from sysroot of cross compiler to lib64 of rootfs
cd ${OUTDIR}/rootfs

#Locating interpreter and shared libraries for the cross compiler
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

#SYSROOT was exported by adding the following command to the end of the ~./bashrc file
#export SYSROOT=/home/venkat/Downloads/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/../aarch64-none-linux-gnu/libc
export SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)


# TODO: Add library dependencies to rootfs
#Adding the library dependencies to rootfs
cp -L $SYSROOT/lib/ld-linux-aarch64.* lib
cp -L $SYSROOT/lib64/libm.so.* lib64
cp -L $SYSROOT/lib64/libresolv.so.* lib64
cp -L $SYSROOT/lib64/libc.so.* lib64



# TODO: Make device nodes
#Making device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1 

# TODO: Clean and build the writer utility
#changing directory to assignment 3 directory
cd $FINDER_APP_DIR/

#Cleaning and building the writer utility with cross compiler
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp $FINDER_APP_DIR/finder-test.sh ${OUTDIR}/rootfs/home
cp $FINDER_APP_DIR/conf/ -r ${OUTDIR}/rootfs/home
cp $FINDER_APP_DIR/finder.sh ${OUTDIR}/rootfs/home
cp $FINDER_APP_DIR/writer ${OUTDIR}/rootfs/home
cp $FINDER_APP_DIR/autorun-qemu.sh ${OUTDIR}/rootfs/home

# TODO: Chown the root directory
# Making the contents owned by root (Chown the root directory)
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
#Creating initramfs.cpio.gz in the OUTDIR folder
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}

#removing previous file to make the whole test non-interactive
#If not removed, asks user if they intend to overwrite an already present initramfs.cpio.gz
rm -f initramfs.cpio.gz
gzip initramfs.cpio 
