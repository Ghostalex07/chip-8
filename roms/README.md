# ROMs are not included in this repository.

Most CHIP-8 programs are hobbyist works in a copyright gray area, so ROMs
are kept out of version control. Good legal sources:

- **CHIP-8 Archive** (CC0, with previews and controls):
  https://johnearnest.github.io/chip8Archive/
  raw files: https://github.com/JohnEarnest/chip8Archive/tree/master/roms
- **Public domain pack**:
  https://archive.org/details/Chip-8RomsThatAreInThePublicDomain

Drop the `.ch8` (or extensionless) files in this folder and run:

    ./chip8 roms/<name>.ch8

`8-scrolling` and other SCHIP ROMs will not work: this emulator targets
plain CHIP-8 only.
