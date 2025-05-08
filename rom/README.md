To compile asm file(s) do:

```
./zasm -uwy bios.asm
./zasm -uwy testrom.asm
```

Then convert bin to C array in header file:

```
xxd -i bios.rom > bios.h
xxd -i testrom.rom > testrom.h
```