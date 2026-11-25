// Fuzz the pty byte stream with scrollback and a resize mid-stream, then export the whole
// history the way "Save Output As" does: Emulation::writeToStream through PlainTextDecoder and
// HTMLDecoder (Screen::writeToStream / copyLineToStream, CompactHistoryScroll reflow).
// Input: 4 control bytes (history lines, new rows, new cols, split point) + stream.
#include "fuzz_common.h"
#include "Emulation.h"
#include "Vt102Emulation.h"
#include "ScreenWindow.h"
#include "Screen.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "colorscheme/ColorScheme.h"
#include "decoders/HTMLDecoder.h"
#include "decoders/PlainTextDecoder.h"
#include "history/compact/CompactHistoryType.h"
#include <QTextStream>

using namespace Konsole;

static TerminalDisplay *fuzzDisplay = nullptr;

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    konsoleFuzzInitApp();
    // Screen asks its current TerminalDisplay for font metrics when an inline image (sixel, kitty,
    // iTerm) does not give its size in cells; in the app a Session always attaches one. One display
    // for the process: it is only read from (font metrics), never attached to a ScreenWindow.
    fuzzDisplay = new TerminalDisplay(nullptr);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 4) return -1;
    const unsigned hist = 1 + data[0] % 200;
    const int rows = 1 + data[1] % 60;
    const int cols = 1 + data[2] % 200;
    data += 4; size -= 4;
    const size_t split = size ? (static_cast<size_t>(data[-1]) * size) / 256 : 0;

    Vt102Emulation em;
    em.reset();
    em.setCodec(QByteArray("UTF-8"));
    em.setCurrentTerminalDisplay(fuzzDisplay);
    em.setHistory(CompactHistoryType(hist));
    em.setImageSize(24, 80);
    em.receiveData(reinterpret_cast<const char *>(data), static_cast<int>(split));
    em.setImageSize(rows, cols);
    em.receiveData(reinterpret_cast<const char *>(data + split), static_cast<int>(size - split));

    const int last = em.lineCount() - 1;
    QString out;
    QTextStream stream(&out);
    PlainTextDecoder plain;
    plain.begin(&stream);
    em.writeToStream(&plain, 0, last);
    plain.end();
    HTMLDecoder html(ColorScheme::defaultTable);
    html.begin(&stream);
    em.writeToStream(&html, 0, last);
    html.end();

    ScreenWindow *w = em.createWindow();
    w->setSelectionByLineRange(0, w->lineCount() - 1);
    out += w->selectedText(Screen::ConvertToHtml | Screen::PreserveLineBreaks);
    volatile size_t sink = out.size();
    (void)sink;
    return 0;
}
