# clorn

**clorn** is a POSIX C implementation of Reborn, 100% written by Claude (Sonnet 4.6) \
clorn is *just an experiment* and it is **not recommended** to use it for production code.

###### note: this readme was human-written, when clorn will function correctly and Claude will report its state as finished, a readme written by it will come in replacement for this. after that, the clorn project will likely be archived, having served its purpose as a test for the Sonnet 4.6 model

## cli usage
- Build clorn:
  `make all` -> will produce `clorn`
&nbsp;
- Compile Reborn source code with clorn:
  `./clorn input.rn output.c`

## repo tree
- `clorn.c`: The entirety of clorn sits in this file.
- `test.rn`: A Reborn test file.
- `out.c`: clorn's output with `test.rn` as input.

## test notes
- As of testing this version, the only bugs that were found are:
- 1. Braceless control flow statements not getting parsed successfully and returning error.
