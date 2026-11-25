#include <cstdlib>
// Fuzz the pty byte stream rendered through the terminal widget, wired the way upstream's
// TerminalDisplayAccessibleTest wires it: Session (no shell is spawned) + SessionController +
// TerminalDisplay with a profile that turns bidi rendering on, then the stream goes through
// Vt102Emulation -> ScreenWindow -> TerminalDisplay::updateImage (bidiMap) -> TerminalPainter
// (painted into an offscreen QImage) and the hotspot filter chain (URL/file/colour filters).
// Input: 1 flag byte (bidi line direction / table override / widget size) + stream.
#include "fuzz_common.h"
#include "Emulation.h"
#include "ScreenWindow.h"
#include "colorscheme/ColorScheme.h"
#include "filterHotSpots/FilterChain.h"
#include "profile/Profile.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include "session/SessionManager.h"
#include "terminalDisplay/TerminalDisplay.h"
#include <QImage>
#include <memory>

using namespace Konsole;

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    konsoleFuzzInitApp();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    const uint8_t flags = data[0];
    data += 1; size -= 1;

    TerminalDisplay display(nullptr);
    const ColorScheme scheme;
    display.setWallpaper(scheme.wallpaper());

    auto session = std::make_unique<Session>(nullptr);
    // Never start a shell: Session::run is what imageSizeInitialized would trigger.
    QObject::disconnect(session->emulation(), &Emulation::imageSizeInitialized, session.get(), &Session::run);
    Profile::Ptr profile(new Profile());
    profile->setProperty(Profile::BidiRenderingEnabled, true);
    profile->setProperty(Profile::BidiLineLTR, bool(flags & 1));
    profile->setProperty(Profile::BidiTableDirOverride, bool(flags & 2));
    SessionManager::instance()->setSessionProfile(session.get(), profile);
    auto controller = std::make_unique<SessionController>(session.get(), &display, nullptr);
    display.setSessionController(controller.get());
    session->addView(&display);
    display.applyProfile(profile);
    display.resize(320 + 4 * (flags >> 2), 200 + 2 * (flags >> 2));

    Emulation *em = session->emulation();
    if (const ScreenWindow *w = display.screenWindow())
        em->setImageSize(qMax(1, w->windowLines()), qMax(1, w->windowColumns()));
    em->setCodec(QByteArray("UTF-8"));
    em->receiveData(reinterpret_cast<const char *>(data), static_cast<int>(size));

    display.updateImage();
    display.filterChain()->process();
    QImage img(display.size(), QImage::Format_ARGB32);
    display.render(&img);

    display.setScreenWindow(nullptr);
    controller.reset();
    // Drop the session from SessionManager's maps (as Session::finished does in the app),
    // so no entry or Profile outlives this input. The slot is protected, so call it through
    // the meta-object; its deleteLater() is cancelled when the unique_ptr destroys the session.
    if (!QMetaObject::invokeMethod(SessionManager::instance(), "sessionTerminated", Qt::DirectConnection,
                                   Q_ARG(Konsole::Session *, session.get())))
        abort();
    return 0;
}
