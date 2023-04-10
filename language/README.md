This is a programming langauge I made because I really liked Jonathan Blow's langage, Jai.  
His langauge was (and still is) in closed beta so I thought I might as well make my own that's a blatant cooy of his so I could program in a langauge that fixes a lot of the issues I have with C and C++.  
It's also significantly less featured and only transpiles to C.  

It includes a lexer, parse, typechecker (including type inference) and C code generator.  
Every part of it was written from scratch and has a single library from [stb](https://github.com/nothings/stb) for faster and customised sprintf.  
  
I have now abandoned this and have repurposed the good parts for C/C++ metaprogramming.  
It went through a pretty big refactor, to make it easier to repurpose, so a lot of the code is probably pretty messy.  
  
You will probably find many bugs since it was abandonded mid development,  
but at least it's about 9k lines of my code to be looked at.
