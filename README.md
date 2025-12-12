# Lispy

Lispy is a Lisp a very small and basic lisp inspired interpreter written in C, based on the book "Build Your Own Lisp". It supports S-Expressions and Q-Expressions, custom functions, variables and basic aritmetic.

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

# Syntax

## S-Expressions and Q-Expressions

S-Expressions aka Symbolic Expressions aka SEXPR.

```
(+ 1 2 3) ; sexpr (evaluates immediately)
```

Q-Expressions aka Quoted Expressions aka QEXPR

```
{1 2 3} ; qexpr (a list that does not auto evaluate)
```

## Polish notation

[Polish notation (Wikipedia)](https://en.wikipedia.org/wiki/Polish_notation)

Lisp uses Polish (prefix) notation for mathematical expressions.

This is how polish notation can look:

```
(+ 4 5 6)
```

In normal infix notation aka regular math, it would look some thing like this:

```
4 + 5 + 6
```

## Operators

| Function  | Operator                                   | Example                                   |
|-----------|--------------------------------------------|-------------------------------------------|
| +         | Addition                                   | (+ 1 2)                                   |
| -         | Subtraction                                | (- 2 1)                                   |
| *         | Multiplication                             | (* 2 3)                                   |
| /         | Division                                   | (/ 4 2)                                   |
| <         | Lesser than                                | (< 2 3)                                   |
| >         | Greater than                               | (> 3 2)                                   |
| <=        | Lesser than or equal                       | (<= 5 2)                                  |
| >=        | Greater than or equal                      | (>= 4 2)                                  |
| ==        | Comparison operator                        | (== 5 5)                                  |
| !=        | Not equal                                  | (!= 3 4)                                  |
| if (cond) | if cond { do some thing}                   | (if(> x 5) {print "big"} {print "small"}) |
| &         | Gather all remaining arguments into a list | (\ {head & rest} {1 2 3 4})               |


## Builtin functions

| Function name | Function                           |
|---------------|------------------------------------|
| head          | Get the first element of a list    |
| tail          | Get everything exept head          |
| eval          | Evaluate the expression            |
| join          | Concatenate lists                  |
| list          | Convert a list from sexpr to qexpr |
| print         | prints to stdout                   |
| error         | prints error message to stdout     |


## Comments

You can make comments using semicolon:
(Everything after the first ";" is ignored)

```
; comment goes here

(def {x y} 1 2)
```

# Defining variables

Functions:

```
(def {var1 var2} {var1_value var2_value})
```

# Defining functions

Lambda functions:

```
\ {arg1 arg2 ...} {body}
```

# Code Examples

## Q-expr vs S-expr

```
; S-expr evaluates immediately:
(+ 1 2 3)            ; => 6

; Q-expr is just a list of stuff:
{+ 1 2 3}            ; => {+ 1 2 3}

; Turn a Q-expr into code by evaluating it:
(eval {+ 1 2 3})     ; => 6
```

## list, head, tail

```
; list creates a Q-expr list out of evaluated things
(list 1 2 (+ 3 4))   ; => {1 2 7}

(head {1 2 3 4})     ; => {1}
(tail {1 2 3 4})     ; => {2 3 4}
```

## join (concatenate list)

```
(join {1 2} {3 4})   ; => {1 2 3 4}

; join can take many lists
(join {1} {2 3} {4}) ; => {1 2 3 4}
```

## Variable with def

```
; define variables
(def {x y} 10 3)

(+ x y)              ; => 13
(* x y)              ; => 30
```

## Function

```
; define add2
(def {add2} (\ {a b} {+ a b}))

(add2 5 7)           ; => 12
```

## & /rest args (variadic)

```
; first argument + rest as a list
(def {show} (\ {head & rest} {
  print head
  print rest
}))

(show 10 20 30 40)   ; prints:
; 10
; {20 30 40}
```

## Sum many numbers

```
(def {sum} (\ {& xs} {eval (join {+} xs)}))

(sum 1 2 3 4 5)      ; => 15
```

## Conditionals

```
(def {x} 7)

(if (> x 5)
  {print "x is big"}
  {print "x is small"})
```
