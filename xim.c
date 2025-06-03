//https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

/*** includes ***/
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>								//edit terminal's attributes 
#include <unistd.h>

/*** defines ***/
#define XIM_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,  
	PAGE_DOWN
}; 

/*** data ***/
/*global struct with editor config data*/
struct editorConfig {
	int cx, cy;
	int screenrows;
	int screencols;
	struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/
/*program dies when it encounters an error*/
void die(const char *s) {
	/*clear screen before err(refer to output for info <esc>-seq)*/
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);								//perror --> stdio.h
	exit(1);								//exit	 --> stdlib.h
}


/*toggle raw mode*/
void disableRawMode() {
	/*reverts to original attributes saved in E*/
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
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
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
	atexit(disableRawMode);	 						//atexit --> stdlib.h

	struct termios raw =  E.orig_termios;
	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_cflag |= (CS8);
	raw.c_oflag &= ~(OPOST);
	/*timeout for read*/
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	//raw.c_cc[VTIME] = 100;
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int editorReadKey() {
	/*
	 *  -- reads one character at a time and writes to c
	 *  -- quit when press [q]
	 *  -- test whether input is control_char/char
	 *  	- control_char	--> printsout code (no ascii char)
	 *	- char		--> prints out code and char
	 *
	 * */
	int nread;
	char c;

	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}

	if (c == '\x1b') {
		char seq[3];
		if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
	
		if (seq[0] == '[') {
			if (seq[1] >= '0' && seq[1] <= '9') {
				if(read(STDIN_FILENO, &seq[2], 1)== 1) return '\x1b';
				if(seq[2] == '~') {
					switch (seq[1]) {
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return HOME_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return HOME_KEY;
					}
				}
			} else {
				switch (seq[1]) {
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
					//case 'U': return PAGE_UP;
				}
			}
		} else if(seq[0] == 'O'){
				switch(seq[1]) {
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
				}
		}

		return '\x1b';
	} else {
		return c;
	}
}


int getCursorPosition(int *rows, int *cols) {
	/*  
	 *  -- fallback func incase ioctl() fails
	 *  -- general idea is to move the cursor to the bottom of the screen before calling this function
	 *  -- then query it's location from the terminal to determine the size of current win and parse it
	 *  -- 6n [6: specify cursor posn][n: query status info from terminal]
	 * */

	char buf[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
	
	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if (buf[i] == 'R') break;
		i++;
	}

	buf[i] = '\0';
	printf("\r\n&buf[1]: %s\r\n", &buf[1]);

	if (buf[0] != '\x1b' || buf[1] == -1) return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
}

int getWindowSize(int *rows, int*cols) {
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
			return -1;
		return getCursorPosition(rows, cols);
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** append buffer ***/
struct abuf {
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void abFree(struct abuf *ab) {
	free(ab->b);
}

/*** input ***/
void editorMoveCursor(int key) {
	/*move cursor*/
	switch(key) {
		case ARROW_LEFT:
			if(E.cx != 0) E.cx--;
			break;
		case ARROW_RIGHT:
			if(E.cx != E.screencols - 1) E.cx++;
			break;
		case ARROW_UP:
			if(E.cy != 0) E.cy--;
			break;
		case ARROW_DOWN:
			if(E.cy != E.screenrows - 1) E.cy++;
			break;
	}
}

void editorProcessKeypress() {
	 /*process input keypress stream
	*/
	int c = editorReadKey();

	switch(c) {
		/*exit*/
		case CTRL_KEY('q'):
			/*clear screen before err(refer to output for info <esc>-seq)*/
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
	
		case HOME_KEY:
			E.cx = 0;
			break;

		case END_KEY:
			E.cx = E.screenrows - 1;
			break;
		
		/*move cursor*/
		case PAGE_UP:
		case PAGE_DOWN:
			{
				int times = E.screenrows;
				while (times--)
					editorMoveCursor(c == PAGE_UP? ARROW_UP : ARROW_DOWN);
			}
			break;

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;
	}
}

/*** output ***/
void editorDrawRows(struct abuf *ab) {
	/*
	 *  -- handles drawing each row of the buffer
	 *  -- escape sequences --
	 *  -- K --> [delete in line]
	 *  -- --
	 * */
	int y;
	for (y=0; y<E.screenrows; y++) {
		if (y == E.screenrows / 3) {
			char welcome[80];
			/*welcome message (with absolute cancer formatting)*/
			int welcomelen = snprintf(welcome, sizeof(welcome), 
					"xim editor -- version %s", XIM_VERSION);
			if (welcomelen > E.screencols) welcomelen = E.screencols;
			int padding = (E.screencols - welcomelen) / 2;
			if (padding) {
				abAppend(ab, "~", 1);
				padding--;
			}
			while (padding--) abAppend(ab, " ", 1);
			abAppend(ab, welcome, welcomelen);
		} else {
			abAppend(ab, "~", 1);
		}

		abAppend(ab, "\x1b[K", 3);
		if (y < E.screenrows - 1) {
			abAppend(ab, "\r\n", 2);
		}
	}
}

void editorRefreshScreen() {
	/* 
	 * [refer to VT100 escape sequences]
	 * escape sequence
	 *  -- ESC(\x1b) + [ denotes start of esc
	 *  -- J --> escape sequence
	 *  -- 2 --> argument to J
	 *  -- ; --> to seperate args
	 *
	 *  -- escape sequences --
	 *  -- 2J --> [2: clear whole screen][J: clear screen]
	 *  -- H  --> [row no. def:1];[col no. def:1][H: move cursor]
	 *  -- h  --> [turn on(Set Mode) cursor]
	 *  -- l  --> [turn off(Reset Mode) cursor]
	 *  -- --
	 * */
	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[H", 3);

	/*insert a ~ like vim*/
	editorDrawRows(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
	abAppend(&ab, buf, strlen(buf));
	
	abAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}

/*** init ***/
void initEditor() {
	E.cx = 0;
	E.cy = 0;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWIndowSize");
}

int main(){
	/*enable raw mode*/
	enableRawMode();
	initEditor();


	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
	}

	return 0;
}
