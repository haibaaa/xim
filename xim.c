//https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>								//edit terminal's attributes 
#include <unistd.h>

struct termios orig_termios;

void disableRawMode() {
	/*reverts to original attributes saved in orig_termios*/
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
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
	 *  -- 
	 * */

	/*disable raw mode at exit*/
	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(disableRawMode);	 						//atexit --> stdlib.h

	struct termios raw =  orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(){
	/*enable raw mode*/
	enableRawMode();


	/*
	 *  -- reads one character at a time and writes to c
	 *  -- quit when press [q]
	 * */
	char c;
	while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
		if (iscntrl(c)) {
			printf("%d\n", c);
		}else {
			printf("%d ('%c')\n", c, c);
		}
	}

	return 0;
}
