# Building the Fermi disassembler (envydis) on Windows

`sass2ptx.py` shells out to `tools/envydis.exe` (not included). Build it from
[envytools](https://github.com/envytools/envytools) (MIT) with MinGW-w64 (no flex/bison needed - the
assembler parser is not required for disassembly):

* sources: `envydis/{core,core-as,core-dis,g80,gf100,gk110,gm107,ctx,falcon,hwsq,xtensa,vuc,macro,envydis}.c`,
  `easm/{easm,easm_print,easm_xfrm}.c`, `util/{aprintf,astr,colors,file,hash,mask,path,symtab,vardata,varinfo,varselect}.c`
* drop the `"vp1"` and `"vcomp"` rows from the ISA table in `envydis/core.c` (those files are not built)
* **one-line fix**: in `envydis/envydis.c` open the input with `fopen(argv[optind], "rb")` - the stock
  `"r"` is text mode on Windows, and a 0x1A byte in the cubin silently truncates the disassembly.
* `gcc -O1 -w -std=gnu11 -Iinclude -Ienvydis -Iutil -Ieasm -include stdlib.h -o envydis.exe <sources>`

Usage: `envydis.exe -m gf100 -n -i <raw .text section>`.
