// Fuzz the .keytab key-binding file reader: KeyboardTranslatorReader over a QIODevice, every
// entry added to a KeyboardTranslator, then looked up again (findEntry) as a key press would.
#include <QBuffer>
#include <QCoreApplication>
#include <cstdio>
#include <cstdlib>
#include "keyboardtranslator/KeyboardTranslator.h"
#include "keyboardtranslator/KeyboardTranslatorReader.h"

using namespace Konsole;

// Parse errors are logged per line through qCWarning; drop non-fatal messages.
static void quietHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type == QtFatalMsg) {
        fprintf(stderr, "qFatal: %s\n", msg.toLocal8Bit().constData());
        abort();
    }
}

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    qInstallMessageHandler(quietHandler);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    QByteArray bytes(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size));
    QBuffer buf(&bytes);
    buf.open(QIODevice::ReadOnly);
    KeyboardTranslator translator(QStringLiteral("fuzz"));
    KeyboardTranslatorReader reader(&buf);
    translator.setDescription(reader.description());
    while (reader.hasNextEntry()) {
        const KeyboardTranslator::Entry e = reader.nextEntry();
        translator.addEntry(e);
        const KeyboardTranslator::Entry f = translator.findEntry(e.keyCode(), e.modifiers(), e.state());
        volatile size_t sink = f.resultToString().size() + e.conditionToString().size();
        (void)sink;
    }
    (void)reader.parseError();
    return 0;
}
