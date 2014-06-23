# **bc-zip** - Bicriteria Data Compressor

## Introduction

*bc-zip* is a lossless, general-purpose, *optimizing* data compressor.

It can be used to compress a file in order to achieve the *best* compression ratio. More technically, bc-zip computes the most succinct Lempel-Ziv parsing, which is the same scheme used by gzip, Snappy, LZO, and LZ4. It goes without saying that compressed files are considerably smaller when compressed with bc-zip than with the aforementioned compressors.

Moreover, and more importantly, bc-zip can be used to obtain a compressed file such that the *decompression time is below a user-specified time and the compression ratio is maximized*, and the other way round (compression size bounded, decompression speed maximized).  In this way, it is possible to achieve very good compression ratios *and* decompression speeds comparable or better than those achieved by state-of-the-art compressors Snappy and LZ4. Even better, it is possible to specify the decompression speed dictated by your application requirements, and let bc-zip automatically achieve the highest compression ratio with that constraint.

This is achieved through an innovative way of modeling data compression as an optimization problem, along with the use of a decompression time model which accurately estimates the decompression time of a compressed file.

Have a look at the [dedicated website](http://farruggia.github.io/bc-zip/) for compiling and usage instructions and more information about the bicriteria technology.
