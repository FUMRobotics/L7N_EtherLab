

## PREEMPT_RT Installtion Guide

**Note:** The following guide has been tested on Ubuntu 16.04 LTS (gcc version 5.3.1).
___
### 1. Decide on a kernel version
There are two versions to take into account when determining the kernel version:  
* **Igh EtherCAT Master:** The package has modified network card drivers only for specific versions of the kernel. 
* **PREEMPT_RT**: Kernel patches are available for some kernels and not others.
#### Igh EtherCAT Master
Since we're going to write IgH EtherCAT Master (from here on called IgH Master) programs, we consider its version first and proceed accordingly. 
The lastest stable version of the library can be downloaded from the [SourceForge repository.](https://sourceforge.net/p/etherlabmaster/code/ci/stable-1.5/tree/). 
Looking at the `devices` folder, we can see the modified (and original) drivers and their associated kernel versions.
For instance,  
```
r8169-4.4-ethercat.c  
``` 
is the modified driver for Realtek8169 family of network cards for kernel 4.4.x . Keep 4.4 in mind and move on to the next section (PREEMPT_RT).  
___
**Note:** Drivers for the more recent kernels are available in Gavin Lambert's [unofficial patchset](https://sourceforge.net/u/uecasm/etherlab-patches/ci/default/tree/#readme).    
___
#### PREEMPT_RT
Now we should look for a version of 4.4.x kernel for which a PREEMPT_RT patch is available. The existence of .patch file should be checked by visiting   
```
https://mirrors.edge.kernel.org/pub/linux/kernel/projects/rt/
``` 
In `/4.4` directory, the only available patch is that of kernel 4.4.162.
```
patch-4.4.162-rt175.patch.xz     
```
Therefore, we choose 4.4.162 as our kernel.
### 2. Download the required files
Download and extract the patch in `~/Downloads` directory,
```bash
https://mirrors.edge.kernel.org/pub/linux/kernel/projects/rt/4.4/patch-4.4.162-rt175.patch.xz
```
```bash
sudo -s
```
We use cURL for downloading the Linux kernel.
```bash
apt-get install curl
```
Kernels have to exist at `/usr/src`, and we're going to download everything to that directory.
```bash
cd /usr/src
```
Download the kernel,
```bash
curl -L https://mirrors.edge.kernel.org/pub/linux/kernel/v4.x/linux-4.4.162.tar.xz | tar xJ
```
When building the PREEMPT-patched kernel, we're going start from the default kernel configuration. That way we'll be sure that the only cause of possbile failure or improvements is **our** modifications.
___
**Note:** Entering the correct URL below requires a visit to http://kernel.ubuntu.com/~kernel-ppa/mainline.
___
```bash
curl -L http://kernel.ubuntu.com/~kernel-ppa/mainline/v4.4.162/linux-image-4.4.162-0404162-generic_4.4.162-0404162.201810200432_amd64.deb -o linux-image-4.4.162-generic-amd64.deb
```
Extract the .deb package. 
```bash
dpkg-deb -x linux-image-4.4.162-generic-amd64.deb linux-image-4.4.162-generic-amd64
```
Install the packges needed for building the kernel.  
```bash
apt-get install libncurses5-dev dpkg-dev libssl-dev
```
### 3. Patch, configure, and build the kernel
Replace the default Ubuntu .config file with the configuration file of the associated Ubuntu kernel,
```bash
cp /usr/src/linux-image-4.4.162-generic-amd64/boot/config-4.4.162-0404162-generic /usr/src/linux-4.4.162/.config
```
Apply the PREEMPT_RT patch to the kernel source files,
```bash
cd /usr/src/linux-4.4.162
```
```bash
patch -p1 < ~/Downloads/patch-4.4.162-rt175.patch
```
Now we're ready to configure the kernel.
```bash
make menuconfig
```
1. Under `General setup > Timers subsystem > Timer tick handling` Choose `Full dynticks system (tickless)`. 
2. If you're using a 64-bit CPU: `Processor type and features > Processor family > Generic x86_64`
3. Number of physical cores (i.e. not accounting for hyperthreading): `Processor type and features > Maximum number of CPU’s > 2` (My CPU was i3-4700, which has 2 physical cores)
4. Disable `Processor type and features > SMT (Hyperthreading) scheduler support`
5. Choose `Preemption Model > Fully Preemtible Kernel (RT)`
6. Choose `Processor type and features > Timer frequency (1000 HZ)` (**Note:** By setting this setting in conjuntion with the first configuration on this list, when only one task is running on an isolated core, there will be no scheduler ticks. Otherwise timer interrupts will be firing at 1 kHz. See [nohz_full=godmode?](https://jeremyeder.com/2013/11/15/nohz_fullgodmode/))
7. Under `Power management and ACPI options`, starting from the top of the list, disable anything that you can, including `CPU Frequency Scaling`, `CPU idle PM support`, and anything listed under `Memory power savings`.
8. Under `Power management and ACPI options > ACPI`, disable everything you're able to, except `Power Management Timer Support` and `Button`.  
9. Select `Exit` and save.  
___
**Note:** Also worth checking are the various guides and recommendations for the optimal kernel configuration in linuxcnc website and forum.
___
Now we should be able to compile the kernel. Note that since many, many device drivers are enabled and will be compiled, and, additionally, since dpkg doesn't support multithreading, the installation process takes a significant amount of time (with i3-4700, it takes about an hour).
```bash
make -j `getconf _NPROCESSORS_ONLN` deb-pkg 
```
Extract RTAI-patched kernel's image and headers. (Note that the name of the debian packages might be different from below).  
```bash
cd /usr/src
```
```bash
dpkg -i linux-image-4.4.162-rt175_4.4.162-rt175-2_amd64.deb
```
```bash
dpkg -i linux-headers-4.4.162-rt175_4.4.162-rt175-2_amd64.deb 
```
The bootloader should be automatically configured. Therefore, at this point, if we reboot, we can choose the RTAI kernel from Advanced Options.  
If the new kernel is not added to the bootloader list, use `grub-customizer`
```bash
add-apt-repository ppa:danielrichter2007/grub-customizer
apt-get update
apt-get install grub-customizer
```
Run `grub-customizer`.
In `General settings > default entry`, under predefined, choose  
```
Advanced options for Ubuntu>Ubuntu, with Linux 4.4.162-rt175
```
### 4. Run the latency test
Install `gnuplot`
```bash
sudo apt-get install gnuplot
```
Install `rt-tests` (for using `cyclictest`)  
```bash
sudo apt-get install rt-tests
```
Download the bash script `mklatencyplot.bash` from [here](http://www.osadl.org/Create-a-latency-plot-from-cyclictest-hi.bash-script-for-latency-plot.0.html).  
As root, 
```bash
bash ./mklatencyplot.bash
```
You can let the test finish its run, or stop it at any time. Either way, a `plot.png` file will be generated in the same directory as the script's, showing a histogram of the latencies of each CPU (core) and the maximum latency. 
### 5. Install IgH EtherCAT Master
See [IgH EtherCAT Master installation guide](https://github.com/mohse-n/L7N_EtherLab/blob/master/Documentation/IgH%20EtherCAT%20Master%20Installation%20Guide.md).
### Reinstalling the kernel
```bash
cd /usr/src/linux-4.4.162
```
````bash
make clean
`````
In `usr/src`, delete any folder and .deb file with "4.4.162-rt175" in its name.
In `/boot`, delete any file with "4.4.162-rt175" in its name. 
___
**Note**: If you wish to change only the configuration, 
```bash
make menuconfig
```
and proceed to the next steps accordingly.
___
**Note**: If you want to undo the patch and configuration steps, instead of 
````bash
make clean
`````
, use 
````bash
make mrproper
`````
With this method, your progress reverts back to the start of step 3. (default kernel config, no patch applied)
