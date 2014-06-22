# **bc-zip** - Bicriteria Data Compressor

## Introduction

*bc-zip* is a lossless, general-purpose, *optimizing* data compressor.

It can be used to compress a file in order to achieve the *best* compression ratio. More technically, bc-zip computes the most succinct Lempel-Ziv parsing, which is the same scheme used by gzip, Snappy, LZO, and LZ4. It goes without saying that compressed files are considerably smaller when compressed with bc-zip than with the aforementioned compressors.

Moreover, and more importantly, bc-zip can be used to obtain a compressed file such that the *decompression time is below a user-specified time and the compression ratio is maximized*, and the other way round (compression size bounded, decompression speed maximized).  In this way, it is possible to achieve very good compression ratios *and* decompression speeds comparable or better than those achieved by state-of-the-art compressors Snappy and LZ4. Even better, it is possible to specify the decompression speed dictated by your application requirements, and let bc-zip automatically achieve the highest compression ratio with that constraint.

This is achieved through an innovative way of modeling data compression as an optimization problem, along with the use of a decompression time model which accurately estimates the decompression time of a compressed file.

For some experimental results backing those claims or more background about the science behind it, visit the dedicated webpage at http://acube.di.unipi.it/bc-zip/

## Compiling

bc-zip requires the following components:

- cmake
- A C++11-enabled compiler (g++ 4.7+, clang 3.2+, ICC 11+)
- Boost (uBLAS, program_options)
- A POSIX environment

The project is compiled like any standard CMake project:

- cd into the project's directory
- mkdir build
- cd build
- cmake ../
- make

## Usage

The executable is invoked with the following syntax:

	./bc-zip command <command-specific options>

The most interesting commands are the following:

- **bit-optimal:**	compress the file so compresson ratio is maximized
- **compress:**		optimize one resource (decompression speed, compression ratio) given a bound on the other one.
- **decompress:** 	decompress a file 

There are also two commands, namely encoders and gens, which we will illustrate later in this document.

### bit-optimal

We report the most important options (bold options are compulsory). For others, have a look at the command-line help 
(just type bc-zip bit-optimal to show the help).

| Option 		| Meaning 																							|
|---------------|---------------------------------------------------------------------------------------------------|
| **-i file**	| 	File to be compressed. Can also be specified by the first positional argument.
| **-o file**	| 	Filename of compressed output. Can also be specified by the second positional argument.
| **-e encoder**| 	Integer encoder used in compression. A list of all supported encoders can be retrieved by invoking the bc-zip command "encoders". We suggest to pick one among **soda09_16** (succinct, relatively slow at decompression) and **hybrid-16** (less succinct, very fast decompression speeds).
| -b bucket 	|	Logically splits the input file into blocks of bucket megabytes, compresses them individually and then concatenate them to obtain the final compressed file. Lowers compression time at the expense of optimality and, thus, compression ratio.
| -z 			|	Shows a progress bar while compressing.

**Example**:

	./bc-zip bit-optimal input output.lzo -e hybrid-16 -b 64

Compress file *input* into *output.lzo*, using the fast encoder *hybrid-16*, with buckets of 64 megabytes.

	./bc-zip bit-optimal input output.lzo -e soda09_16 -z

Optimally compress file *input* into *output.lzo*, using the succinct encoder *soda09_16*.

### compress

Compress implements "bicriteria" compression.
In order to evaluate decompression times, a **target** file encoding the decompression time model for the target machine (that is, the machine in which decompression will take place) is required. Have a look at section **Target file** for more informations about this matter.

In the following we illustrate the most important options (bold options are compulsory).

| Option 		| Meaning 																							|
|---------------|---------------------------------------------------------------------------------------------------|
|**-i file**	| File to be compressed. Can also be specified by the first positional argument.
|**-e encoder**	| Integer encoder used in compression. A list of all supported encoders can be retrieved by invoking the bc-zip command "encoders". We suggest to pick one among **soda09_16** (succinct, relatively slow at decompression) and **hybrid-16** (less succinct, very fast decompression speeds).
|**-t target**	| Specify the target machine model file, as obtained by the calibrator tool (see the section about the calibrator for further informations), WITHOUT the .tgt suffix. That is, if the model is names X.tgt, then just pass X to this option.
|**-b bound**	| Specify the compression bound. If the bound has the form Xk, then the compression size will be at most X kilobytes and the decompression time minimized. If the bound has the form Xm, then the  decompression time will be at most X milliseconds (according to the target model specified with  option -t ) and compressed size minimized. Multiple bounds can be specified by separating different bounds with a comma: in this case the file will be compressed multiple times, once for each bound.
|-l level 		| Specify a compression level (float in range [0,1]), which might be in compression space or  decompression time. A compression level is a "relative" way of setting bounds. A level in the form XS (such as 0.5S) defines a bound on the compression space. It means that, if the most space-succinct compressed file for the input file has space S_1, and the compressed file with the highest decompression speed has space S_2 (with S_1 < S_2), then the level specifies a bound on compressed space equal to S_1 + X * (S_2 - S_1). In other words, compressing a file with level 0S is the same as compressing the file with the bit-optimal command, while compressing a file with level 1S instructs the compressor to obtain the decompression time-optimal compression file. Linearly increasing the level increases linearly the compressed space of the final parsing. A level in the form XT (such as 0.2T) similarly defines a bound on the decompression time, so a level of 0T obtains the compression with the lowest decompression time, while a level of 1T obtains the most succinct compressed file. A level of 0.5T has a decompression time which is a mean of the decompression time of the most succinct compressed file and the decompression time of the compressed file with the highest decompression speed. Multiple levels can be specified by separating different bounds with a comma: in this case the file will be compressed multiple times, once for each level.
|-z 			| Shows a progress bar while compressing.

At least one among -b or -l must be selected. The output file name is a combination of the input file name and the compression options specified.

**Examples**

Suppose you have a file input.txt to be compressed and a target file smartphone.tgt, which models the decompression time of a compressed file
on a particular smartphone.
Suppose the most succinct compressed file has a decompression time of 600 msec (with encoder hybrid-16) and takes 200MB, while the fastest compressed file is decompressed in 300msec (also with encoder hybrid-16) and takes 400MB of space.
Then:

	./bc-zip compress input.txt -e hybrid-16 -t smartphone -b 300000k -z

Compress file *input.txt* with encoder *hybrid-16*, obtaining the fastest compressed file which is not greater than 300 megabytes (300,000 kilobytes), showing a progress bar during the process. The compressed file is input.txt#hybrid-16#300MB.lzo.

	./bc-zip compress input.txt -e soda09_16 -t smartphone -b 300000k,600m

Compress file *input.txt* two times with encoder *soda09_16*: the first time with a bound of 300MB in space, saving in input.txt#soda09_16#300MB.lzo; the second with a time bound of 600 msecs (according to model *smartphone.tgt*, stored in the calling directory), saving in input.txt#soda09_16#600msec.lzo

	./bc-zip compress input.txt -e hybrid-16 -t smartphone -l 0.5T,0.1T,0.1S -b 300000k

Compress file input.txt four times:
- first time with a level of 0.5T, so with a time bound of 450 msec (remmber the assumptions made at the beginning), saving in input.txt#hybrid-16#0.5T.lzo
- second time with a level of 0.1T, so with a time bound of 330 msec, saving in input.txt#hybrid-16#0.5T.lzo
- third time with a level of 0.1S, so with a space bound of 220MB, saving in input.txt#hybrid-16#0.1S.lzo
- fourth time with a bound of 300MB, saving in input.txt#hybrid-16#300MB.lzo

### decompress

Usage: bc-zip decompress input_file output_file

**Example**

	./bc-zip decompress compressed.lzo original

Decompress file "compressed.lzo" into file "original".

## Target file

### What it is and why it's needed?

Different machines decompress the same file with different decompression times. So, in order to properly compute the decompression time of a compressed file for a given target machine, *bc-zip* needs a decompression time model tuned for that target machine (which may be different than the machine used in compression!).

bc-zip does not ship with any decompression time model by default. Instead, bc-zip needs a *target file* when it is invoked with the "compress" command.

A target file is a small, human-readable file which encodes the decompression time model for a particular machine.


### How do I get it?

A target file can be obtained in an automatic or semi-automatic fashion through the use of the provided *calibrator* tool.

A calibrator obtains the target file of the machine in which it is executed.

The calibrator tool is composed of two objects: a shell script **calibrator.sh** and an executable **calibrator**, both located in the tool/ directory.

The user must invoke the shell script, taking care of having both the shell script and the executable on the same directory.

The tool is invoked as follows:

	./calibrator.sh 	<target name> <memory hierarchy descriptor>

Where:
- **target name**: the name of that particular machine. If X is passed, the outputted target file will be named X.tgt.
- **memory hierarchy descriptor**: A file with a line for each memory level (e.g. L1 and L2 caches, main memory), sorted by capacity, with just two fields for each line separated by some space: level capacity, in kilobytes, and latency, in nanoseconds.

A valid memory hierarchy descriptor could be the following:

```
32 				0.41
256 			1.2
6144			3.3
4294967296		80
```

Which means that that particular machine has a L1 cache of 32KB with access latency of 0.4ns, a 256kb L2 cache with latency 1.2ns, a L3 cache with latency 3.3 and a main memory of 4GB with latency of 80ns.

This file can also be generated automatically via the get_latencies tool, which can be invoked in the following way:

	./get_latencies lmbench_file

Where lmbench_file is the output of tool lat_mem_rd in the LMBench3 suite (downloadable at the following url: http://www.bitmover.com/lmbench/get_lmbench.html).

**Example**

	./calibrator client client_hierarchy.txt

Obtains the target file client.tgt of the machine in which it is executed, provided that client_hierarchy.txt properly describes the memory hierarchy of that machine.
