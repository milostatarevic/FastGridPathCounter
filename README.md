# FastGridPathCounter

**FastGridPathCounter** computes the number of corner-to-corner simple paths, cycles,
and Hamiltonian cycles in an NxN grid.

## Usage

### Compilation

To compile the program, run:
```sh
make N=[grid_size] CYCLES=[0|1] HAMILTONIAN=[0|1] N_THREADS=[number_of_threads] BITS=[8|16|32|64]
```

### Single Run Using Modular Arithmetic

To run the program for a desired modulus:
```sh
./path-counter [modulus]
```

For example:
```sh
make N=21 CYCLES=0 HAMILTONIAN=0 N_THREADS=16 BITS=32
./path-counter 4294966661
```
This counts the number of paths modulo 4294966661 in a 21x21 grid graph using 16 threads and 32-bit precision.

### Complete Solution Using Chinese Remainder Theorem

To find a full count using the Chinese Remainder Theorem, there is a Ruby script that performs modular
counts multiple times and reconstructs the solution using the Chinese Remainder Theorem.

Usage:
```sh
ruby run.rb [n] [bits] [threads] [paths|cycles|hamiltonian]
```

Example:
```sh
ruby run.rb 26 16 16 hamiltonian
```
This example counts the number of Hamiltonian cycles in a 26x26 grid graph, using 16-bit modulus and 16 threads.
On an M3 Ultra MacBook Pro with 128GB of RAM, this program completes in about 9 hours, taking ~65GB of RAM, and
yields the correct solution as listed in [A003763](https://oeis.org/A003763):
```
25578285385897276060130031526614700187075412685764186583833403069393167252132218312152073569856334502
```

## Performance

**FastGridPathCounter** is based on the same algorithm as **GGCount** but is 3-6x faster, depending on the use case
and platform. The main difference is that **FastGridPathCounter** uses an optimized order of operations and the
efficient use of lookup tables and bit operations.

**GGCount**, developed by the authors of the technical report on which both programs are based,
can be found at: https://github.com/kunisura/GGCount

## Acknowledgments

This work uses the techniques introduced in the following technical report:
> H. Iwashita, Y. Nakazawa, J. Kawahara, T. Uno, and S. Minato,
"Efficient Computation of the Number of Paths in a Grid Graph with Minimal
Perfect Hash Functions",
Technical Report TCS-TR-A-13-64, Division of Computer Science, Graduate
School of Information Science and Technology, Hokkaido University, 2013.
([pdf](http://www-alg.ist.hokudai.ac.jp/~thomas/TCSTR/tcstr_13_64/tcstr_13_64.pdf))
