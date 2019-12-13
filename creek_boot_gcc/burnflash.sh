#!/bin/bash

sudo openocd -f board/stm32f7discovery.cfg -c "program build/proj_a.bin exit 0x08000000"
