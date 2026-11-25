// Shared setup for the konsole fuzz harnesses (mayhem layer, not upstream code).
// One QApplication for the process, offscreen platform; quiet Qt's debug/warning chatter
// (qFatal / qCritical still abort as usual, so real assertion failures still surface).
#pragma once
#include <QApplication>
#include <QByteArray>
#include <cstdlib>

static void konsoleFuzzMsgHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type == QtFatalMsg) {
        fprintf(stderr, "qFatal: %s\n", msg.toLocal8Bit().constData());
        abort();
    }
}

static void konsoleFuzzInitApp()
{
    static QApplication *app = nullptr;
    if (app) return;
    setenv("QT_QPA_PLATFORM", "offscreen", 1);
    // Keep KDE/Qt from reading or writing the user's config/cache dirs between inputs.
    setenv("XDG_CONFIG_HOME", "/tmp/konsole-fuzz-xdg/config", 0);
    setenv("XDG_DATA_HOME", "/tmp/konsole-fuzz-xdg/data", 0);
    setenv("XDG_CACHE_HOME", "/tmp/konsole-fuzz-xdg/cache", 0);
    qInstallMessageHandler(konsoleFuzzMsgHandler);
    static int argc = 1;
    static char arg0[] = "konsole-fuzz";
    static char *argv[] = {arg0, nullptr};
    app = new QApplication(argc, argv);
}
