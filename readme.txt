ffam-rip and ffam-build are conversion tools to enable end users to manually edit grub pf2 font files.

I did this so you don't have to.

Main.cpp: rips fonts to txt definition plus bmp spritesheet. The rules pertaining the spritesheet should be obvious looking at it. Null lines divide glyphs. Please know that the colors have to be exactly the same after you're done editing.

Build.cpp: Builds txt and bmp into a pf2 file.
