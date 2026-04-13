#!/usr/bin/env bash
# Executar des de la terminal MSYS2 MINGW64:
#   cd /c/Users/USER/ControlSortides
#   bash build.sh

export TEMP=/tmp
export TMP=/tmp
export TMPDIR=/tmp

if command -v x86_64-w64-mingw32-gcc-posix &>/dev/null; then
    CC="x86_64-w64-mingw32-gcc-posix"
    CXX="x86_64-w64-mingw32-g++-posix"
    QP_PORT="win32-qv"
    STATIC_FLAG="-static"
elif command -v x86_64-w64-mingw32-gcc &>/dev/null; then
    CC="x86_64-w64-mingw32-gcc"
    CXX="x86_64-w64-mingw32-g++"
    QP_PORT="win32-qv"
    STATIC_FLAG="-static"
else
    CC="gcc"
    CXX="g++"
    QP_PORT="posix-qv"
    STATIC_FLAG=""
fi

ROOT="$(cd "$(dirname "$0")" && pwd)"
QPCPP="$ROOT/qpcpp"
OUT="$ROOT/build"

mkdir -p "$OUT"

echo "Generant index_html.h..."
printf 'static const char INDEX_HTML[] = R"RAWHTML(\n' > "$ROOT/HttpServer/index_html.h"
cat "$ROOT/HttpServer/index.html" >> "$ROOT/HttpServer/index_html.h"
printf ')RAWHTML";\n' >> "$ROOT/HttpServer/index_html.h"

echo "Generant json_horari.h..."
mkdir -p "$ROOT/ControlHorari"
printf 'static const char JSON_HORARI[] = R"RAWJSON(\n' > "$ROOT/ControlHorari/json_horari.h"
cat "$ROOT/docs/json_horari.json" >> "$ROOT/ControlHorari/json_horari.h"
printf ')RAWJSON";\n' >> "$ROOT/ControlHorari/json_horari.h"

echo "Compilant mongoose..."
$CC -c -O1 \
    -I"$ROOT/mongoose" \
    "$ROOT/mongoose/mongoose.c" \
    -o "$OUT/mongoose.o" 2>/dev/null \
|| $CXX -c -O1 -x c \
    -I"$ROOT/mongoose" \
    "$ROOT/mongoose/mongoose.c" \
    -o "$OUT/mongoose.o"

[ $? -ne 0 ] && echo "ERROR compilant mongoose" && exit 1

echo "Compilant app..."
$CXX -std=gnu++17 -Wall -O1 $STATIC_FLAG \
    -I"$ROOT" \
    -I"$QPCPP/include" \
    -I"$QPCPP/src" \
    -I"$QPCPP/ports/$QP_PORT" \
    "$ROOT/main.cpp" \
    "$ROOT/ControlRemot/ControlRemot.cpp" \
    "$ROOT/ControlHorari/ControlHorari.cpp" \
    "$ROOT/HttpServer/HttpServer.cpp" \
    "$QPCPP/src/qf/qep_hsm.cpp" \
    "$QPCPP/src/qf/qep_msm.cpp" \
    "$QPCPP/src/qf/qf_act.cpp" \
    "$QPCPP/src/qf/qf_actq.cpp" \
    "$QPCPP/src/qf/qf_defer.cpp" \
    "$QPCPP/src/qf/qf_dyn.cpp" \
    "$QPCPP/src/qf/qf_mem.cpp" \
    "$QPCPP/src/qf/qf_ps.cpp" \
    "$QPCPP/src/qf/qf_qact.cpp" \
    "$QPCPP/src/qf/qf_qeq.cpp" \
    "$QPCPP/src/qf/qf_qmact.cpp" \
    "$QPCPP/src/qf/qf_time.cpp" \
    "$QPCPP/ports/$QP_PORT/qf_port.cpp" \
    "$OUT/mongoose.o" \
    -o "$OUT/app.exe" \
    -lpthread \
    $([ "$QP_PORT" = "win32-qv" ] && echo "-lwinmm -lws2_32")

if [ $? -eq 0 ]; then
    echo "OK — build/app.exe"
else
    echo "ERROR — revisa els missatges de dalt"
    exit 1
fi
