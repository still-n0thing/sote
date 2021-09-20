/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** data ***/

// Original Termios to restore terminal
struct termios orig_termios;

/*** terminal ***/

void die(const char *s) {
    perror(s);
    // perror looks at global errno varible and peints a descriptive error message
    // It also prints the string given to it before the error message
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) // set the value of struct termios
        die("tcgetattr");

    atexit(disableRawMode); // Function when exit is called

    struct termios raw = orig_termios;

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

/*** init ***/

int main() {
    enableRawMode();

    while (1) {
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
        // read() returns -1 on failure, and set the errno value to indicate the erro
        if (iscntrl(c)) {
            // iscntrl is in <ctype.h>
            // test wheather a character is a control character
            // Control characters are non prinitable characters that we dont't want to print on the screen
            // ASCII 0-31 and 127 are all control characters
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }
        if (c == 'q') break;
    }

    return 0;
}