# GVM

A basic virtual machine and assembly language.

## Work-in-progress

This demo is largely untested; use at your own risk.

## Demo

```
c

cat prog.g

gasm prog.g

d prog.b

gvm prog.b

gdis prog.b
```

The `err*.g` files are erroneous, so `gasm` correctly fails to compile them.

The `rerr*.g` files contain runtime errors, so `gvm` exits with an error code.
