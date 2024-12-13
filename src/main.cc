#include "adt/logs.hh"
#include "adt/defer.hh"
#include "TextBuff.hh"

#include <csignal>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

using namespace adt;

#define CTRL_KEY(k) ((k) & 0b11111)
struct TermSize
{
    int width {};
    int height {};
    int pixWidth {};
    int pixHeight {};
};

struct State
{
    termios termOg {};
};

State g_state {};

static bool g_bRunning {};
static TextBuff s_textBuff {};

static void
cursorXY(int x, int y)
{
    char aBuff[100] {};
    print::toBuffer(aBuff, sizeof(aBuff) - 1, "\x1b[H\x1b[{}C\x1b[{}B", x, y);
    TextBuffPush(&s_textBuff, aBuff);
}

[[nodiscard]] static TermSize
getTermSize()
{
    TermSize term;

    term.width = term.height = term.pixWidth = term.pixHeight = -1;

#ifdef G_OS_WIN32
    {
        HANDLE chd = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csb_info;

        if (chd != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(chd, &csb_info))
        {
            term_size.width_cells = csb_info.srWindow.Right - csb_info.srWindow.Left + 1;
            term_size.height_cells = csb_info.srWindow.Bottom - csb_info.srWindow.Top + 1;
        }
    }
#else
    {
        struct winsize w;
        bool bHaveWINSZ = false;

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) >= 0 || ioctl(STDERR_FILENO, TIOCGWINSZ, &w) >= 0 ||
            ioctl(STDIN_FILENO, TIOCGWINSZ, &w) >= 0)
            bHaveWINSZ = true;

        if (bHaveWINSZ)
        {
            term.width = w.ws_col;
            term.height = w.ws_row;
            term.pixWidth = w.ws_xpixel;
            term.pixHeight = w.ws_ypixel;
        }
    }
#endif

    if (term.width <= 0)
        term.width = -1;
    if (term.height <= 2)
        term.height = -1;

    /* If .ws_xpixel and .ws_ypixel are filled out, we can calculate
     * aspect information for the font used. Sixel-capable terminals
     * like mlterm set these fields, but most others do not. */

    if (term.pixWidth <= 0 || term.pixHeight <= 0)
    {
        term.pixWidth = -1;
        term.pixHeight = -1;
    }

    return term;
}

static void
disableRawMode()
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_state.termOg) == -1)
        LOG_DIE("tcsetattr\n");

    print::toFILE<5>(stderr, "\n");
}

static void
enableRawMode()
{
    if (tcgetattr(STDIN_FILENO, &g_state.termOg) == -1)
        LOG_DIE("tcgetattr\n");

    atexit(disableRawMode);

    struct termios raw = g_state.termOg;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /*raw.c_cc[VMIN] = 0;*/
    /*raw.c_cc[VTIME] = 1;*/

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        LOG_DIE("tcsetattr\n");
}

static void
procKey()
{
    char c {};
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
        LOG_DIE("read\n");

    if (iscntrl(c)) LOG("{}\n", (int)c);
    else LOG("{} ('{}')\r\n", (int)c, c);

    if (c == CTRL_KEY('q')) g_bRunning = false;
}

static void
drawRows()
{
    for (int y = 0; y < 24; y++)
        write(STDOUT_FILENO, "~\r\n", 3);
}

static void
updateScreen()
{
    TextBuffPush(&s_textBuff, "\x1b[2J");
    TextBuffPush(&s_textBuff, "\x1b[H");

    TermSize term = getTermSize();
    LOG("term: {}, {}, pix: {}, {}\n", term.width, term.height, term.pixWidth, term.pixHeight);

    cursorXY((term.width-5) / 2, term.height / 2);
    TextBuffPush(&s_textBuff, "HELLO");

    TextBuffFlush(&s_textBuff);
}

static void
sigwinchHandler(int sig)
{
    LOG_GOOD("Got SIGWINCH: {}\n", sig);
    /*updateScreen();*/
}

int
main()
{
    Arena arena(SIZE_1M);
    defer( freeAll(&arena) );
    s_textBuff = TextBuff(&arena);

    signal(SIGWINCH, sigwinchHandler);

    enableRawMode();

    g_bRunning = true;

    do
    {
        updateScreen();
        procKey();

        ArenaReset(&arena);
    }
    while (g_bRunning);
}
