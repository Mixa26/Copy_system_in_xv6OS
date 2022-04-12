#include "types.h"
#include "x86.h"
#include "defs.h"
#include "kbd.h"

int
kbdgetc(void)
{
	static uint shift;
	static uchar *charcode[4] = {
		normalmap, shiftmap, ctlmap, ctlmap
	};
	uint st, data, c;

	st = inb(KBSTATP);
	if((st & KBS_DIB) == 0)
		return -1;
	data = inb(KBDATAP);

	if(data == 0xE0){
		shift |= E0ESC;
		return 0;
	} else if(data & 0x80){
		// Key released
		data = (shift & E0ESC ? data : data & 0x7F);
		shift &= ~(shiftcode[data] | E0ESC);
		return 0;
	} else if(shift & E0ESC){
		// Last character was an E0 escape; or with 0x80
		data |= 0x80;
		shift &= ~E0ESC;
	}

	shift |= shiftcode[data];
	shift ^= togglecode[data];
	c = charcode[shift & (CTL | SHIFT)][data];

	//detektovanje shift+alt+c inputa za pocetak copy rezima i izlaska iz copy rezima
	if((shift & ALT && shift & SHIFT) && charcode[0][data] == 'c')
	{	
		if (flagC)
		{
			flagC = 0;
			c &= 0x0000;	
		}
		else flagC = 1;
		flagP = 1;
	}

	//detektovanje shift+alt+p inputa za paste
	if(!flagC && ((shift & ALT && shift & SHIFT) && charcode[0][data] == 'p'))
	{
		flagPaste = 1;
	}

	if(shift & CAPSLOCK){
		if('a' <= c && c <= 'z')
			c += 'A' - 'a';
		else if('A' <= c && c <= 'Z')
			c += 'a' - 'A';
	}
	return c;
}

void
kbdintr(void)
{
	consoleintr(kbdgetc);
}
