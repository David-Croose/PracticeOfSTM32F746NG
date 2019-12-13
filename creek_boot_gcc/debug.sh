#!/bin/bash

cgdb -d arm-atollic-eabi-gdb -- build/proj_a.elf -x gdbinit
