# RIDL (MFBDS) Proof-of-Concept

This repository contains Proof-of-Concept exploits for vulnerabilities described in [**RIDL: Rogue In-Flight Data Load**](https://mdsattacks.com/files/ridl.pdf) by van Schaik, Milburn, Österlund, Frigo, Maisuradze, Razavi, Bos, and Giuffrida.
The demos in this repository utilise the _Microarchitectural Fill Buffer Data Sampling_ variant of RIDL as originally described in the paper.

All demos were tested on an Intel Core i7-7700K CPU with Debian and Linux Kernel 5.10.0.

## Setup

To successfully run these exploits, an x86_64 Intel CPU with SMT (Hyperthreading) and a fairly modern Linux-based OS are required.
The demos will not run on Windows or any other OS directly. We also recommend using a CPU with Intel TSX, but this is not strictly required.  

You will need root permissions.

**Dependencies**

You need _cmake_ to build the demos. Since this repository also contains a kernel-module, you should also install the Linux-Headers for your Kernel version.  
Example for Debian and Ubuntu:
<!-- prettier-ignore -->
```shell
 sudo apt-get install cmake linux-headers-$(uname -r) 
 ```
**Build**

Building is as simple a cloning the repository and running
```shell
 make
 ```
If your CPU supports Intel TSX, you can manually disable TSX support with
```shell
 make notsx
 ```

**Run**

Before running any of the demos, you should allocate some huge pages with
```shell
 echo 16 | sudo tee /proc/sys/vm/nr_hugepages
 ```

If your CPU supports Intel TSX, you can pass the _--taa_ parameter to any of the demos to
utilise _TSX Asynchronous Abort_ instead of basic RIDL. This may drastically increase the success rate.


## Demo #1: Determining success rates

The first demo will repeatedly try to sample a known value from the line fill buffers. 
The demo will start two threads: A victim thread that tries to constantly fill the line fill buffers,
and an attacker thread that tries to recover the values.

There are two variants:

1) Fill the the line fill buffers by storing to memory:

```shell
 ./demo_read1
 ```
2) Fill the the line fill buffers by loading from memory:
```shell
 ./demo_read2
 ```

On a vulnerable system, the success rates should be between 0.5% and 5% (or around 30% with _--taa_).
Note that even tiny success rates (well below 1%) are sufficient to leak information with RIDL.

## Demo #2: Transmitting Strings

RIDL can be used to transmit data between threads and across privilege boundaries.
This demo transmits a character string. Again, there are multiple variants:

1) Fill the the line fill buffers by storing to memory:

```shell
 ./demo_string1
 ```
2) Fill the the line fill buffers by loading from memory:
```shell
 ./demo_string2
 ```
3) Run the victim in kernel mode.
 Please load the kernel module before running the demo itself.
```shell
 sudo insmod kernel_module/ridl_module.ko
 sudo ./demo_string3
 sudo rmmod ridl_module
 ``` 
 
    

## Acknowledgements

Parts of this project are based on the [Meltdown Proof-of-Concept](https://github.com/IAIK/meltdown).  

[A PoC for several RIDL-class vulnerabilities](https://github.com/vusec/ridl) was published by the authors of the 
original RIDL-paper. Their implementation (especially for TAA) was used for troubleshooting and served as an additional source
for information on RIDL.
