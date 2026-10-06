#!/bin/bash
# Local pre-CI syntax check. Stubs live in build-local-check/stubs (generated,
# matching real headers). Encodes the (*encoder)-> C++ idiom CI accepts.
set -u
GCC="C:/Users/Kekko/scoop/apps/mingw-winlibs/current/bin/gcc.exe"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STUB="$ROOT/build-local-check/stubs"
FILES="${@:-src/main.c src/ihs_glue.c src/osd_fb.c src/font5x7.c src/webos/osd.c src/webos/media.c src/host/host_video.c}"
FAIL=0
for f in $FILES; do
  OUT=$("$GCC" -fsyntax-only -DTARGET_WEBOS -DAPPID='"com.kekko.steamview"' \
    -include "$STUB/force_posix.h" -I"$STUB" -I"$ROOT/src" -I"$ROOT/src/lgnc/include" \
    -Wall -Werror=implicit-function-declaration "$ROOT/$f" 2>&1 | grep -vE "unused variable|\^|\||In function" | head -8)
  if [ -n "$OUT" ]; then echo "=== $f"; echo "$OUT"; FAIL=1; fi
done
[ $FAIL -eq 0 ] && echo "LOCAL CHECK CLEAN"
exit $FAIL
