// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

//flag za copy rezim
extern int flagC;
//flag za reset/pamcenje pozicije kursora
extern int flagP;
//flag da treba odraditi paste
extern int flagPaste;
//pozicija kursora 
int CPos;
//flag za pocetak copy selekcije
int q = 0;
//pozicija pocetka copy selekcije
int qPos;
//pozicija kraja copy selekcije
int ePos;
//kopirani hex prikaz 
short unsigned int CWord[2000];
//broj slova u CWord
int CWordC = 0;
//poslednja pozicija kursora
int LastPos;

static void consputc(int);

static int panicked = 0;

static struct {
	struct spinlock lock;
	int locking;
} cons;

static void
printint(int xx, int base, int sign)
{
	static char digits[] = "0123456789abcdef";
	char buf[16];
	int i;
	uint x;

	if(sign && (sign = xx < 0))
		x = -xx;
	else
		x = xx;

	i = 0;
	do{
		buf[i++] = digits[x % base];
	}while((x /= base) != 0);

	if(sign)
		buf[i++] = '-';

	while(--i >= 0)
		consputc(buf[i]);
}

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
	int i, c, locking;
	uint *argp;
	char *s;

	locking = cons.locking;
	if(locking)
		acquire(&cons.lock);

	if (fmt == 0)
		panic("null fmt");

	argp = (uint*)(void*)(&fmt + 1);
	for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
		if(c != '%'){
			consputc(c);
			continue;
		}
		c = fmt[++i] & 0xff;
		if(c == 0)
			break;
		switch(c){
		case 'd':
			printint(*argp++, 10, 1);
			break;
		case 'x':
		case 'p':
			printint(*argp++, 16, 0);
			break;
		case 's':
			if((s = (char*)*argp++) == 0)
				s = "(null)";
			for(; *s; s++)
				consputc(*s);
			break;
		case '%':
			consputc('%');
			break;
		default:
			// Print unknown % sequence to draw attention.
			consputc('%');
			consputc(c);
			break;
		}
	}

	if(locking)
		release(&cons.lock);
}

void
panic(char *s)
{
	int i;
	uint pcs[10];

	cli();
	cons.locking = 0;
	// use lapiccpunum so that we can call panic from mycpu()
	cprintf("lapicid %d: panic: ", lapicid());
	cprintf(s);
	cprintf("\n");
	getcallerpcs(&s, pcs);
	for(i=0; i<10; i++)
		cprintf(" %p", pcs[i]);
	panicked = 1; // freeze other CPU
	for(;;)
		;
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory

volatile static void
cgaputc(int c)
{
	int pos;

	// Cursor position: col + 80*row.
	outb(CRTPORT, 14);
	pos = inb(CRTPORT+1) << 8;
	outb(CRTPORT, 15);
	pos |= inb(CRTPORT+1);

	if (!flagC)
	{		
		if(c == '\n')
			pos += 80 - pos%80;
		else if(c == BACKSPACE){
			if(pos > 0) --pos;
		} else
			crt[pos++] = (c&0xff) | 0x0700;  // black on white

		if(pos < 0 || pos > 25*80)
			panic("pos under/overflow");

		if((pos/80) >= 24){  // Scroll up.
			memmove(crt, crt+80, sizeof(crt[0])*23*80);
			pos -= 80;
			memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
		}
	}
	else
	{
		//Pamcenje poslednje pozicije kursora pri ulazak u copy rezim
		if (flagP)
		{
			flagP = 0;
			CPos = pos;			
		}		

		//Kretanje u copy rezimu i odabir pocetka selekcije
		if (c == 'w')
		{
			if (pos - 80 >= 0)
			{
				if(!q)
					crt[pos] = (crt[pos] & 0x00FF) | 0x0700;
				pos -= 80;
			}
		}
		else if (c == 's')
		{
			if (pos + 80 <= 2000)
			{
				if(!q)
					crt[pos] = (crt[pos] & 0x00FF) | 0x0700;
				pos += 80;
			}
		}
		else if (c == 'a')
		{
			if (pos - 1 >= 0)
			{
				if(!q)
					crt[pos] = (crt[pos] & 0x00FF) | 0x0700;
				pos -= 1;
			}
		}
		else if (c == 'd')
		{
			if (pos + 1 <= 2000)
			{
				if(!q)
					crt[pos] = (crt[pos] & 0x00FF) | 0x0700;
				pos += 1;
			}
		}
		else if (!q && c == 'q')
		{
			q = 1;
			qPos = pos;
		}
		crt[pos] = (crt[pos] & 0x00FF) | 0x7000;
	}
	//bojenje slova koja su selektovana u copy rezimu kada je q odabrano
	if (q)
	{
		for (int i = 0; i < 2000; i++)
		{
			crt[i] = (crt[i] & 0x00FF) | 0x0700;
		}
		if (qPos < pos)
		{
			for (int i = qPos; i < pos; i++)
			{
				crt[i] = (crt[i] & 0x00FF) | 0x7000;
			}
		}
		else
		{
			for (int i = pos; i < qPos; i++)
			{
				crt[i] = (crt[i] & 0x00FF) | 0x7000;
			}
		}
		//obradjivanje prekida selekcije za kopiranje
		if (c == 'e')
		{
			ePos = pos;
			CWordC = 0;
			//kopiranje hex prikaza reci i boje 
			if (ePos < qPos)
			{
				for (int i = ePos; i < qPos; i++)
				{	
					CWord[i-ePos] = crt[i];
					CWordC++;
				}
			}
			else
			{
				for (int i = qPos; i < ePos; i++)
				{
					CWord[i-qPos] = crt[i];
					CWordC++;
				}
			}
			//flagC = 0;
			//pos = CPos;
			//iskljucivanje moda selekcije ukoliko je ukljucen
			q = 0;
			//bojenje ekrana nazad u crno
			for (int i = 0; i < 2000; i++)
			{
				crt[i] = (crt[i] & 0x00FF) | 0x0700;
			}
		}
	}
	LastPos = pos;
	outb(CRTPORT, 14);
	outb(CRTPORT+1, pos>>8);
	outb(CRTPORT, 15);
	outb(CRTPORT+1, pos);
	if (!flagC)
		crt[pos] = ' ' | 0x0700;
}

void
consputc(int c)
{
	if(panicked){
		cli();
		for(;;)
			;
	}

	if(c == BACKSPACE){
		uartputc('\b'); uartputc(' '); uartputc('\b');
	} else
		uartputc(c);
	cgaputc(c);
}

#define INPUT_BUF 128
struct {
	char buf[INPUT_BUF];
	uint r;  // Read index
	uint w;  // Write index
	uint e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

void
consoleintr(int (*getc)(void))
{

		//Vracanje kursora na poslednju poziciju pri izlasku iz copy rezima
		if (flagP)
		{		
			flagP = 0;
			//vracanje boje za poslednje slovo gde je kursor bio
			crt[LastPos] = (crt[LastPos] & 0x00FF) | 0x0700;
			LastPos = CPos;		
			
			//iskljucivanje moda selekcije ukoliko je ukljucen
			q = 0;
			//bojenje ekrana nazad u crno
			for (int i = 0; i < 2000; i++)
			{
				crt[i] = (crt[i] & 0x00FF) | 0x0700;
			}

			outb(CRTPORT, 14);
			outb(CRTPORT+1, LastPos>>8);
			outb(CRTPORT, 15);
			outb(CRTPORT+1, LastPos);
			
			return;
		}

	int c, doprocdump = 0;

	acquire(&cons.lock);
	while((c = getc()) >= 0){
		switch(c){
		case C('P'):  // Process listing.
			// procdump() locks cons.lock indirectly; invoke later
			doprocdump = 1;
			break;
		case C('U'):  // Kill line.
			while(input.e != input.w &&
			      input.buf[(input.e-1) % INPUT_BUF] != '\n'){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		case C('H'): case '\x7f':  // Backspace
			if(input.e != input.w){
				input.e--;
				consputc(BACKSPACE);
			}
			break;
		default:
			if(c != 0 && input.e-input.r < INPUT_BUF){
				c = (c == '\r') ? '\n' : c;

				//dodat flag check da ne bismo pisali slova u bafer dok se krecemo u copy rezimu
				if (!flagC && !flagPaste)
					input.buf[input.e++ % INPUT_BUF] = c;
	
				//pasteovanje selekcije u bafer
				if (flagPaste)
				{
					for (int i = 0; i < CWordC; i++)
					{
						input.buf[input.e++ % INPUT_BUF] = CWord[i];
						consputc(CWord[i]);
					}					
				}

				//necemo da ispisemo slovo ukoliko pasteujemo (slovo P bi se ispisalo)
				//pored toga ovde obaramo flag za paste jer smo odradili sve sto nam treba sa njim
				if(!flagPaste)
				{
					consputc(c);
				}
				else flagPaste = 0;

				if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
					input.w = input.e;
					wakeup(&input.r);
				}
			}
			break;
		}
	}
	release(&cons.lock);
	if(doprocdump) {
		procdump();  // now call procdump() wo. cons.lock held
	}
}

int
consoleread(struct inode *ip, char *dst, int n)
{
	uint target;
	int c;

	iunlock(ip);
	target = n;
	acquire(&cons.lock);
	while(n > 0){
		while(input.r == input.w){
			if(myproc()->killed){
				release(&cons.lock);
				ilock(ip);
				return -1;
			}
			sleep(&input.r, &cons.lock);
		}
		c = input.buf[input.r++ % INPUT_BUF];
		if(c == C('D')){  // EOF
			if(n < target){
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r--;
			}
			break;
		}
		*dst++ = c;
		--n;
		if(c == '\n')
			break;
	}
	release(&cons.lock);
	ilock(ip);

	return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
	int i;

	iunlock(ip);
	acquire(&cons.lock);
	for(i = 0; i < n; i++)
		consputc(buf[i] & 0xff);
	release(&cons.lock);
	ilock(ip);

	return n;
}

void
consoleinit(void)
{
	initlock(&cons.lock, "console");

	devsw[CONSOLE].write = consolewrite;
	devsw[CONSOLE].read = consoleread;
	cons.locking = 1;

	ioapicenable(IRQ_KBD, 0);
}

