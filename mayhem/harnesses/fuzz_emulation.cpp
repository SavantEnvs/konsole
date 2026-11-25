// Fuzz the pty byte stream: Emulation::receiveData -> Vt102Emulation (CSI/OSC/DCS/APC, sixel,
// kitty/iTerm images, OSC 8 URLs) -> Screen, then read it back the way the UI does: a ScreenWindow
// selection (possibly empty or reversed) -> selectedText(), and getCurrentScreenCharacterCounts().
// Input: 6 control bytes (selection start col/line, end col/line, flags, decoding options) + stream.
#include "fuzz_common.h"
#include "Emulation.h"
#include "Vt102Emulation.h"
#include "ScreenWindow.h"
#include "Screen.h"
#include "terminalDisplay/TerminalDisplay.h"

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
    if (size < 6) return -1;
    const uint8_t c0 = data[0], l0 = data[1], c1 = data[2], l1 = data[3], flags = data[4], opts = data[5];
    data += 6; size -= 6;

    Vt102Emulation em;
    em.reset();
    em.setCodec(QByteArray("UTF-8"));
    em.setCurrentTerminalDisplay(fuzzDisplay);
    em.setImageSize(24, 80);
    em.receiveData(reinterpret_cast<const char *>(data), static_cast<int>(size));

    ScreenWindow *w = em.createWindow();
    const int lines = w->lineCount() > 0 ? w->lineCount() : 1;
    w->setSelectionStart(c0 % 82, l0 % (lines + 1), flags & 1);
    w->setSelectionEnd(c1 % 82, l1 % (lines + 1), flags & 2);
    QString t = w->selectedText(Screen::DecodingOptions(opts & 0xff));
    if (flags & 4) {
        w->setSelectionByLineRange(l1 % lines, l0 % lines);
        t += w->selectedText(Screen::PlainText);
    }
    const QList<int> counts = em.getCurrentScreenCharacterCounts();
    volatile size_t sink = t.size() + counts.size();
    (void)sink;
    return 0;
}
