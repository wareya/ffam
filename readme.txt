ffam-rip and ffam-build are conversion tools to enable end users to manually edit grub pf2 font files.

I did this so you don't have to.

Rip.cpp: rips fonts to txt definition plus bmp spritesheet. The rules pertaining the spritesheet should be obvious looking at it. Null lines divide glyphs. Please know that the colors have to be exactly the same after you're done editing.

Build.cpp: Builds txt and bmp into a pf2 file.

FAQ:

Q: My pf2 isn't bit-per-bit identical after doing a round trip.

A: grub-mkfont and ffam-build handle the height dimension of blank glyphs differently. It's inconsequential, though.

Q: ffam rejects my pf2 font, but grub loads it fine.

A: ffam is slightly stricter with glyph dimensions than grub is. ffam implements undefined behavior for later glyphs when a glyph's bitmap data goes past its boundary.