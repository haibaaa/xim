//https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

/*** includes ***/
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <termios.h>								//edit terminal's attributes 
#include <unistd.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/
struct termios orig_termios;

/*** terminal ***/
/*program dies when it encounters an error*/
void die(const char *s) {
	perror(s);								//perror --> stdio.h
	exit(1);								//exit	 --> stdlib.h
}

void disableRawMode() {
	/*reverts to original attributes saved in orig_termios*/
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
		die("tcsetaddr");
}

void enableRawMode() {
	/*
	 *  -- parses the terminal's attributes to a termios struct [tcgetattr()]
	 *  -- edit the struct
	 *  -- push the new terminal attributes [tcsetattr()]
	 *
	 *  -- c_lflags --> msc flags
	 *  -- c_cflags --> control flags 
	 *  -- c_iflags --> input flags
	 *  -- c_oflags --> output flags
	 *
	 *  -- ~ negates the flag value (it is turned off)
	 *
	 *  -- local flags --
	 *  -- [ECHO] 	terminal feedback (turning off feels like typing passwd after a sudo)
	 *  -- [ICANON]	controls canonical mode - basically allow toggling b/w line/char buffering
	 *  -- [ISIG]	signals for CTRL - C(SIGINT > terminate) / Z(SIGSTP > suspend)
	 *  -- [IEXTEN]	allows typing control signals as literals signal after CTRL-V (CTRL-C --> ^C)
	 *  -- -- 
	 *
	 *  -- input flags --
	 *  -- [IXON]	software flow control > CTRL - S(terminates) / Q(enables)
	 *  -- [ICRLN]	converts all \r(carrige return > CTRL-M) to \n(enter > CTRL-J/ENTER);
	 *  -- [BRKINT]	a break condition causes SIGINT
	 *  -- [INPCK]	enables parity checking (not applicable to modern terminals);
	 *  -- [ISTRIP]	strips the 8th bit (sets to zero)(any 8 bit char is garbled to 7 bit ASCII)
	 *  -- --
	 *
	 *  -- control flags/masks --
	 *  -- [CS8]	[BIT MASK] sets CS(character size) to 8 bits 
	 *  -- --
	 *
	 *  -- output flags --
	 *  -- [OPOST]	post processing of the output
	 *  -- --
	 *
	 *  -- control characters --
	 *  -- [VMIN]	min bytes needed before read() can return
	 *  -- [VTIME]	max time to wait before read() returns (unit is 1/10s or 100ms -- god knows why)
	 *  -- --
	 * */

	/*disable raw mode at exit*/
	if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
	atexit(disableRawMode);	 						//atexit --> stdlib.h

	struct termios raw =  orig_termios;
	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_cflag |= (CS8);
	raw.c_oflag &= ~(OPOST);
	/*timeout for read*/
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

/*** init ***/
int main(){
	/*enable raw mode*/
	enableRawMode();


	/*
	 *  -- reads one character at a time and writes to c
	 *  -- quit when press [q]
	 *  -- test whether input is control_char/char
	 *  	- control_char	--> prints out code (no ascii char)
	 *	- char		--> prints out code and char
	 *
	 * */
	while (1) {
		char c = '\0';
		if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
		if (iscntrl(c)) {						//iscntrl --> ctype.h
			printf("%d\r\n", c);					//printf  --> stdio.h			
		}else {
			printf("%d ('%c')\r\n", c, c);
		}
		if(c == CTRL_KEY('q')) break;
	}

	return 0;
}
