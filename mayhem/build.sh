#!/usr/bin/env bash
#
# mayhem/build.sh — build konsole's fuzz harnesses and the subset of its QtTest suite that
# mayhem/test.sh runs. Runs in the commit image as `mayhem` in /mayhem; re-runnable and offline
# (every dependency is a Debian package installed by mayhem/Dockerfile).
#
#  1. mayhem/build-tests: upstream CMake build with NORMAL flags, only the test executables.
#  2. mayhem/build-fuzz:  the same build with $SANITIZER_FLAGS + SanCov + DWARF-3, so the static
#     libkonsoleprivate.a (emulation, screen, history, decoders, display, colour schemes, keytabs)
#     is instrumented. Each harness in mayhem/harnesses is then compiled and linked with exactly
#     the compile/link lines CMake generated for upstream's Vt102EmulationTest (same includes,
#     defines and libraries), with the test's source swapped for the harness.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export CC CXX

cd "$SRC"
M="$SRC/mayhem"
COMMON_CMAKE=(-G Ninja -DBUILD_TESTING=ON -DBUILD_SHARED_LIBS=OFF
  -DENABLE_PLUGIN_SSHMANAGER=OFF -DENABLE_PLUGIN_QUICKCOMMANDS=OFF
  -DCMAKE_BUILD_TYPE=RelWithDebInfo)
TESTS=(Vt102EmulationTest ScreenTest HistoryTest KeyboardTranslatorTest TerminalCharacterDecoderTest
  CharacterWidthTest HotSpotFilterTest)

# 1) test suite, normal flags
cmake -S "$SRC" -B "$M/build-tests" "${COMMON_CMAKE[@]}" \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS"
cmake --build "$M/build-tests" -j"$MAYHEM_JOBS" --target "${TESTS[@]}"

# 2) sanitized + coverage-instrumented project. The per-config flags carry the debug level so
#    CMake's default RelWithDebInfo "-g" (DWARF-5 on clang 19) never comes after -gdwarf-3.
FUZZ_FLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS"
cmake -S "$SRC" -B "$M/build-fuzz" "${COMMON_CMAKE[@]}" \
  -DCMAKE_C_FLAGS="$FUZZ_FLAGS" -DCMAKE_CXX_FLAGS="$FUZZ_FLAGS" \
  -DCMAKE_C_FLAGS_RELWITHDEBINFO="-O1 $DEBUG_FLAGS" -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O1 $DEBUG_FLAGS"
cmake --build "$M/build-fuzz" -j"$MAYHEM_JOBS" --target Vt102EmulationTest

# Compile/link lines of the reference target, as CMake/ninja generated them.
B="$M/build-fuzz"
REF_OBJ="src/autotests/CMakeFiles/Vt102EmulationTest.dir/Vt102EmulationTest.cpp.o"
CMDS="$(ninja -C "$B" -t commands Vt102EmulationTest)"
COMPILE="$(printf '%s\n' "$CMDS" | grep -F -- "-o $REF_OBJ " | tail -1)"
LINK="$(printf '%s\n' "$CMDS" | tail -1)"
[ -n "$COMPILE" ] && [ -n "$LINK" ] || { echo "cannot find reference compile/link lines" >&2; exit 1; }
COMPILE="${COMPILE%% -MD *}"            # drop depfile + -o/-c; we add our own
REF_BIN="bin/Vt102EmulationTest"
case "$LINK" in *"$REF_OBJ"*"-o $REF_BIN "*) ;; *) echo "unexpected link line: $LINK" >&2; exit 1;; esac

mkdir -p "$M/bin" "$B/mayhem-harness"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/mayhem-harness/standalone_main.o"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$M/lsan_off.cc" -o "$B/mayhem-harness/lsan_off.o"

for src in "$M"/harnesses/fuzz_*.cpp; do
  name="$(basename "$src" .cpp)"
  obj="$B/mayhem-harness/$name.o"
  (cd "$B" && eval "$COMPILE -I$SRC/src -I$SRC/src/characters -I$SRC/src/colorscheme -I$SRC/src/keyboardtranslator -I$B/src -I$M/harnesses -o $obj -c $src")
  fuzz_link="${LINK/$REF_OBJ/$obj $B/mayhem-harness/lsan_off.o $LIB_FUZZING_ENGINE}"
  fuzz_link="${fuzz_link/-o $REF_BIN /-o $M/bin/$name }"
  (cd "$B" && eval "$fuzz_link")
  sa_link="${LINK/$REF_OBJ/$obj $B/mayhem-harness/lsan_off.o $B/mayhem-harness/standalone_main.o}"
  sa_link="${sa_link/-o $REF_BIN /-o $M/bin/$name-standalone }"
  (cd "$B" && eval "$sa_link")
done
ls -l "$M/bin"
