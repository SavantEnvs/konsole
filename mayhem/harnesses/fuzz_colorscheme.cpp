// Fuzz the .colorscheme reader: the input is written to a scratch file, parsed by KConfig and
// read by ColorScheme::read (colour entries, randomization ranges, wallpaper settings), then the
// colour table is computed with randomization (hsluv) for a seed taken from the input.
#include "fuzz_common.h"
#include <KConfig>
#include <QFile>
#include <QDir>
#include <unistd.h>
#include "colorscheme/ColorScheme.h"

using namespace Konsole;

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    konsoleFuzzInitApp();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const char *tmp = getenv("TMPDIR");
    QString path = QString::fromLocal8Bit(tmp && *tmp ? tmp : "/tmp") + QStringLiteral("/konsole-fuzz-%1.colorscheme").arg(getpid());
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 0;
        f.write(reinterpret_cast<const char *>(data), static_cast<qint64>(size));
    }
    {
        KConfig cfg(path, KConfig::SimpleConfig);
        ColorScheme scheme;
        scheme.read(cfg);
        QColor table[TABLE_COLORS];
        scheme.getColorTable(table, size ? data[0] : 0);
        volatile int sink = table[0].red() + scheme.description().size();
        (void)sink;
    }
    QFile::remove(path);
    return 0;
}
