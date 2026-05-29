# NetBSD Wii Joybus

Demos of using NetBSD to communicate with a gameboy

## Setup

TODO
But in the meantime

- You will need the custom kernel from [here](https://github.com/gummybuns/NetBSD-src/tree/wii-identify)
- You will need devkitpro installed on another machine to build the gba files
- You will need a wii set up with netbsd in the first place

## Multiboot

Implements the multiboot protocol from [FIX94](https://github.com/FIX94/gba-link-cable-rom-sender) and boots up a demo game. Use the `-g` to specify a different game file

https://github.com/user-attachments/assets/0b396abd-d4bc-44c5-9934-aab94953b360

## Echo

Send messages to/from the gameboy

https://github.com/user-attachments/assets/20626d87-c592-433a-9fab-a6adaeaa7d64



