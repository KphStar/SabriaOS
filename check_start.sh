
#!/bin/bash

# Check if kernel.elf exists
if [ ! -f kernel.elf ]; then
  echo "❌ Error: kernel.elf not found."
  exit 1
fi

# Get the address of the 'start' symbol
start_addr=$(nm kernel.elf | grep ' T start' | awk '{print $1}')

if [ "$start_addr" = "00001000" ]; then
  echo "✅ Entry point 'start' is correctly placed at 0x1000."
  exit 0
else
  echo "❌ WARNING: 'start' symbol is at 0x$start_addr, expected 0x1000!"
  echo "👉 Fix by setting ENTRY(start) in linker.ld and ensuring . = 0x1000"
  exit 1
fi
