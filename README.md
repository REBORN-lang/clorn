# clorn

> (!) Last reported state: `Almost finished`
> (!) This compiler implementation is based off the [Standard](https://reborn.ct.ws/std), specifically, *revision 26013* as of now.

**clorn** is a POSIX C implementation of Reborn, 100% written by Claude (Sonnet 4.6) \
clorn is *just an experiment* and it is **not recommended** to use it for production code.

###### note: This readme was human-written. When clorn will function correctly and Claude will report its state as finished, another readme written by it will come in replacement for this. after that, the clorn project will likely be archived, having served its purpose as a test for the Sonnet 4.6 model
###### secondary note: CReborn and other official Reborn-lang compiler implementations have a zero-tolerance policy for AI code unless proven that its quality in that specific instance is better than what another developer could have written.

## cli usage
- Build clorn:
  `make all` -> will produce `clorn`
&nbsp;
- Compile Reborn source code with clorn:
  `./clorn input.rn output.c`
&nbsp;
- Test `test.rn` directly with `make`:
  `make test`

## repo tree
- `clorn.c`: The entirety of clorn sits in this file.
- `test.rn`: A Reborn test file.
- `out.c`: clorn's output with `test.rn` as input.

## test notes
- All fine for now.

