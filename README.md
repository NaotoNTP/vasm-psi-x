# vasm

This is a customized fork of VASM that aims to provide a syntax and feature set comparable to the PSY-Q family of assemblers, whilst retaining support for some of VASM's more uniquely useful features. While this fork makes no promises to be 100% compatible existing codebases, users of PSY-Q assemblers should in theory find migration to this fork of VASM much easier than the syntax module options available with the standard version of VASM.

You can find the original at the [vasm home](http://sun.hasenbraten.de/vasm).

## About

vasm is a portable and retargetable assembler to create linkable objects in various formats or absolute code. Multiple CPU-, syntax and output-modules can be selected.

Many common directives/pseudo-opcodes are supported (depending on the syntax module) as well as CPU-specific extensions.

The assembler supports optimizations (e.g. choosing the shortest possible branch instruction or addressing mode) and relaxations (e.g. converting a branch to an absolute jump when necessary).

Most syntax modules support macros, include directives, repetitions, conditional assembly and local symbols.


## Compilation Instructions

To compile vasm you can either use CMake, or choose a Makefile which fits for your host architecture.

The following Makefiles are available:

* Makefile - standard Unix/gcc Makefile
* Makefile.68k - makes AmigaOS 68020 executable with vbcc
* Makefile.Haiku - gcc Makefile which doesn't link libm
* Makefile.MiNT - makes Atari MiNT 68020 executable with vbcc
* Makefile.MOS - makes MorphOS executable with vbcc
* Makefile.OS4 - makes AmigaOS4 executable with vbcc
* Makefile.PUp - makes PowerUp executable with vbcc
* Makefile.TOS - makes Atari TOS 68000 executable with vbcc
* Makefile.WOS - makes WarpOS executable with vbcc
* Makefile.Win32 - makes Windows executable with MS-VSC++
* Makefile.Win32FromLinux - makes Windows executable on Linux

Then select a CPU- and a syntax-module to compile.

The vasm-binary will be called: `vasm<CPU>_<SYNTAX>[_<HOST>]`

### Building using CMake

Define `VASM_CPU` and `VASM_SYNTAX` when running cmake, e.g:

```bash
mkdir build
cd build
cmake -DVASM_CPU=m68k -DVASM_SYNTAX=psi-x ..
make
```

### Building using make

Define `CPU` and `SYNTAX` when running make, e.g:

```bash
make CPU=m68k SYNTAX=psi-x
```

### Available CPU modules

* 6502
* arm
* m68k
* z80
* test

### Available Syntax modules

* psi-x
* test
