# Lispy

Creating the lisp variation from this book; https://www.buildyourownlisp.com/

# How to run this program

Clone this repo; https://github.com/orangeduck/mpc?tab=readme-ov-file

Then copy the mpc.c and mpc.h into the Lispy(this folder) folder.

Now get GCC (Linux and OSX) or Mingw on windows

GCC (Linux and OSX); https://gcc.gnu.org/

How to get Mingw (windows); https://code.visualstudio.com/docs/cpp/config-mingw

When you have the compiler installed, you can compile the program with this command;

Linux and OSX;

```
cc main.c mpc.c -o lispy -leditline -lm -Wall
```

Windows;
```
cc -Wall parsing.c mpc.c -o parsing
```
