/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define SOTA_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)
// Ctrl with alphabates are 1-26 in ASCII
// CRTL+Q to quit the program
// Some kind of bitmask

/*** data ***/

// Original Termios to restore terminal
struct termios orig_termios;

struct editorConfig {
    int cx, cy;
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    // Clear the screen on exit

    perror(s);
    // perror looks at global errno varible and peints a descriptive error message
    // It also prints the string given to it before the error message
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) // set the value of struct termios
        die("tcgetattr");

    atexit(disableRawMode); // Function when exit is called

    struct termios raw = E.orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // BRKINT is turned on, a break condition will cause a SIGINT signal to be sent to the program, like pressing Ctrl-C.

    // Ctrl-M read as 10
    // Because the terminal is translating any carriage returns (13, '\r) 
    // inputted by user into newlines (10, '\n').
    // We turn this off using ICRNL

    // INPCK enables parity checking, which doesn’t seem to apply to modern terminal emulators

    // ISTRIP causes the 8th bit of each input byte to be stripped, meaning it will set it to 0. This is probably already turned off

    // Ctrl-S strops data being trnsmitted to the terminal until Ctrl-Q is pressed
    // This was used in old day to stop transmission of data to printer so that it can catch up
    // We turn it off using IXON

    
    raw.c_oflag &= ~(OPOST);
    // Terminal translates each newline "\n"to "\r\n"
    // Terminal requires both og these char in order to start a new line of text
    // Carriage return move curesor back to begnning of the current line
    // Newline moves the cursor down a line, scrolling the screen if necessary
    // We trun off this output processing feature using OPOST
    
    raw.c_cflag |= (CS8);
    // S8 is not a flag, it is a bit mask with multiple bits, which we set using the bitwise-OR (|) operator unlike all the flags we are turning off. 
    // It sets the character size (CS) to 8 bits per byte. On my system, it’s already set that way.    
    
    
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    // ICANON allows us to turn off canonocal mode (<termios.h>)
    // We can read the input byte-to-byte, instead of line-by-line
    
    // Ctrl-C makes terminal wait for us to type another char amd then sends that char literally
    // IEXTEN is used to disble this feature

    // Ctrl-C sends SIGINT signal(current process to terminate)
    // Ctrl-Z sends SIGTSTP sigal(current process to suspend)
    // Turn off both by using ISIG     
    
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    // c_cc stands for "control characters", an array of bytes that control various terminal settings
    // VMIN value sets the minimum numbers of bytes of input needed before read() can return
    // set it to 0 so that read as soon as there is any input to be read
    // VTIME value sets the maximum amount to wait before read() returns
    // 1/10 of sec or 100 millisec

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
    // TCSAFLUSH discards any unread input before applying the changes to the terminal
}

char editorReadKey(){
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

int getCursorPosition(int *rows, int *cols){
    char buf[32];
    unsigned int i = 0;
    
    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) -1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    // printf("\r\n&buf[1]: '%s'\r\n");
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    
    editorReadKey();

    return -1;
}

int getWindowSize(int *rows, int *cols){
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
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

/*** output ***/

void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++){
        if (y == E.screenrows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                "sota editor -- version %s", SOTA_VERSION);
            if (welcomelen > E.screencols) welcomelen = E.screencols;
            int padding = (E.screencols = welcomelen) /2;
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
    // ~ on left side as vim does

}

void editorRefreshScreen(){
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    // Hide the cursor when repainting

    // writing 4 bytes to terminal
    // \x1b is escape char or 27 in decimal
    // <esc>[1J would clear the screen where the cursor is
    // <esc>[0J would clear the screen from the cursor to the end of the screen
    // <esc>[J would clear the screen from the cursor to the end
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    // abAppend(&ab, "\x1b[H", 3);
    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/

void editorProcessKeypress(){
    char c = editorReadKey();
    switch (c){
        case CTRL_KEY('q'):
            exit(0);
            break;
    }
}


/*** init ***/

void initEditor(){
    E.cx = 0;
    E.cy = 0;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1){
        die("getWindowSize");
    }
}

int main() {
    enableRawMode();
    initEditor();

    while (1) {
        // char c = '\0';
        // if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
        // // read() returns -1 on failure, and set the errno value to indicate the erro
        // if (iscntrl(c)) {
        //     // iscntrl is in <ctype.h>
        //     // test wheather a character is a control character
        //     // Control characters are non prinitable characters that we dont't want to print on the screen
        //     // ASCII 0-31 and 127 are all control characters
        //     printf("%d\r\n", c);
        // } else {
        //     printf("%d ('%c')\r\n", c, c);
        // }
        // if (c == CTRL_KEY('q')) break;
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}