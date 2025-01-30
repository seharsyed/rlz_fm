# Relative Lempel-Ziv (RLZ)

## Description

This software computes the Relative Lempel Ziv (RLZ) parse of the target sequence file using a reference file. By default, the software does character-level encoding.
However, character-level encoding can fail if the reference file does not contain all the unique characters in the sequence file. We have also provided an option to 
do the encoding at the bit-level. Doing the encoding this way prevents the issue that occurs when the sequence contains a character that is not present in the reference.
However, the parse might be larger than the character-level parse due to encoding the bit representation of the sequence file. The decompression expects that the files were originally ASCII (8 bit) encoded.

The software performs pattern matching with the FM-index by reversing both the sequence and the reference text internally. This approach enables forward matching and determines the length of the forward match. The correct reference position is obtained by applying an involution to the suffix array position retrieved from the FM-index (which is built on the reversed reference text), a constant-time operation. 

## Algorithm Workflow

To compress the target sequence file relative to a reference file, the software follows these steps:

Bit-level encoding:

0. Convert both the reference and sequence files to their binary representation.

Common steps (for all encoding types):

1. Reverse the reference and sequence files.

2. Build an FM-index from the reversed reference (in bit or character form).

3. Perform reverse matching:
    Match each character or bit of the reversed sequence against the reversed reference using the FM-index's backward matching capabilities (to simulate forward matching).
   
    3a. If a match is found, check if the next character or bit also matches.
   
    3b. If a match is found and it's the end of the sequence, apply involution to the suffix array (SA) position, subtract the length of the match from the position, and record the (pos, len) pair.
   
    3c. If a mismatch occurs, apply involution to the SA position, subtract the length of the match from the position, record the (prev pos, len - 1) pair, and restart the search from the mismatched character or bit.
   
4. Write all (pos, len) pairs sequentially to a file. This constitutes the RLZ parse.

> [!NOTE]
> The RLZ parse is in reference to the reference file.

## Prerequisites

- [CMake](https://cmake.org/) 3.15 or higher.
- GCC 9+
- C++17-compatible compiler.
- OpenMP

## Getting Started

### Building the Project

```
git clone https://github.com/rvarki/RLZ.git
cd RLZ
mkdir build
cd build
cmake ..
make -j
```

### Running the project

After building the project, an executable named rlz will be created in the build directory. Run it with:
```
./rlz -r [reference file] -s [file to compress] [options] 
```

### Default Compression Example

In this section, we will go through a small example using the default character-level compression. In the data/dna directory, we have provided an example reference and target sequence file that were derived from DNA FASTA files.

1. To compress the sequence file with character compression, run the following command from the build directory

```
./rlz -r ../data/dna/dna_ref.txt -s ../data/dna/dna_seq.txt
```
This command will produce the following file in the data/dna directory: `dna_seq.txt.rlz`. The .rlz file contains the RLZ parse.

> [!NOTE]
> Multithreading is supported in the compression step with the -t [num. of threads] option which can significantly make the compression step faster. However, the RLZ parse is slightly different from what you would get if you run with a single thread. The reason is we cannot identify phrases that span where the file was split. Potentially might add an additional thread number of parse entries that would not exist if you ran with a single thread.

> [!NOTE]
> The compression ratio is quite high in this example. The reason for this is partly due to the similarity between the reference and sequence file. Another reason is due to writing the parse with uint64_t numbers. For small files, using 8 bytes for each number is too large and therefore wasteful. Maybe will change in the future. 

2. To decompress the file, run the following command

```
./rlz -r ../data/dna/dna_ref.txt -s ../data/dna/dna_seq.txt -d
```
This command should produce a file called `dna_seq.txt.out` in the data/dna directory. This is the decompressed sequence file.

3. Check to see if the file decompressed correctly
```
diff ../data/dna/dna_seq.txt ../data/dna/dna_seq.txt.out
```

There should be no output from this command if compressed and decompressed correctly. 


### Bit Compression Example

In this section, we will go through a small example using the bit-level compression option. In the data/english directory, we have provided an example reference and target sequence file that were derived from the English text in the [Pizza&Chili Corpus](https://pizzachili.dcc.uchile.cl/texts/nlang/).

1. To compress the sequence file, run the following command from the build directory

```
./rlz -r ../data/english/english_ref.txt -s ../data/english/english_seq.txt --bit
```

This command will produce the following file in the data/english directory: `english_seq.txt.rlz`. The .rlz file contains the RLZ parse.

2. To decompress the file, run the following command

```
./rlz -r ../data/english/english_ref.txt -s ../data/english/english_seq.txt --bit -d
```
This command should produce a file called `english_seq.txt.out` in the data/english directory. This is the decompressed sequence file.

3. Check to see if the file decompressed correctly
```
diff ../data/english/english_seq.txt ../data/english/english_seq.txt.out
```

There should be no output from this command if compressed and decompressed correctly. 

> [!NOTE]
> To get more information from the tool. Run the command with --verbose flag.

### License

This project is licensed under the MIT License - see the [LICENSE](https://github.com/rvarki/rlz/blob/main/LICENSE) file for details

## Dependencies
- [SDSL](https://github.com/simongog/sdsl-lite)
- [CLI11](https://github.com/CLIUtils/CLI11)
- [spdlog](https://github.com/gabime/spdlog)

## Acknowledgements

- [Dhruv R. Makwana](https://github.com/Dhruv-mak) [Helped develop the code]

- [S. Kuruppu, S. J. Puglisi and J. Zobel, Relative Lempel-Ziv Compression of Genomes for Large-Scale Storage and Retrieval](http://dx.doi.org/10.1007/978-3-642-16321-0_20). Proc. 17th International Symposium on String Processing and Information Retrieval (SPIRE 2010) Lecture Notes in Computer Science, Volume 6393, (2010) pp. 201-206. 
