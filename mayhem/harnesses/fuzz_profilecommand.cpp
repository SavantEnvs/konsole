// Fuzz ProfileCommandParser::parse, which turns "Property=value;..." strings into profile
// property changes; reached from the OSC 50 profile-change escape sequence and `konsole -p`.
#include "fuzz_common.h"
#include "profile/ProfileCommandParser.h"

using namespace Konsole;

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    konsoleFuzzInitApp();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ProfileCommandParser parser;
    const Profile::PropertyMap map = parser.parse(QString::fromUtf8(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size)));
    size_t n = 0;
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        n += it->second.toString().size();
    volatile size_t sink = n;
    (void)sink;
    return 0;
}
