#!/bin/bash
#
# Compilacion cruzada de ZEsarUX para Windows (64 bits) desde Linux o WSL, usando MinGW-w64
# Windows .exe keresztfordítása Linuxról vagy WSL-ből, MinGW-w64-gyel
#
# Uso / Használat:
#   sudo apt install gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64 zip curl
#   ./build-windows-cross.sh
#
# El resultado queda en / Az eredmény helye:  ~/zesarux-windows/ZEsarUX_windows-<rama>.zip
#
# No modifica la compilacion de Linux: compila sobre una copia del codigo fuente
# A Linuxos fordítást nem érinti: a forrás egy másolatán dolgozik

set -e

SDL_VER=2.30.9
HOST=x86_64-w64-mingw32

SRCDIR="$(cd "$(dirname "$0")" && pwd)"
WORK="${XDG_CACHE_HOME:-$HOME/.cache}/zesarux-windows-build"
OUTDIR="${ZESARUX_WINDOWS_OUT:-$HOME/zesarux-windows}"

RAMA=$(git -C "$SRCDIR" rev-parse --abbrev-ref HEAD 2>/dev/null || echo build)
NOMBRE="ZEsarUX_windows-$RAMA"

for programa in $HOST-gcc $HOST-windres zip curl; do
    if ! command -v $programa >/dev/null 2>&1; then
        echo "Missing / Hiányzik: $programa"
        echo "sudo apt install gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64 zip curl"
        exit 1
    fi
done

mkdir -p "$WORK/bin" "$WORK/mingw"

# SDL2 para MinGW / SDL2 MinGW-hez
if [ ! -d "$WORK/SDL2-$SDL_VER" ]; then
    echo "Downloading SDL2 $SDL_VER for MinGW..."
    curl -fsSL "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-devel-$SDL_VER-mingw.tar.gz" | tar xz -C "$WORK"
fi
ln -sfn "$WORK/SDL2-$SDL_VER/$HOST" "$WORK/mingw/SDL2"

# configure detecta Windows con "uname -o" = Msys. Lo simulamos solo para esta compilacion
cat > "$WORK/bin/uname" <<'EOF'
#!/bin/sh
case "$1" in
    -o) echo Msys ;;
    -s) echo MINGW64_NT ;;
    -m) echo x86_64 ;;
    *) /usr/bin/uname "$@" ;;
esac
EOF
printf '#!/bin/sh\nexec %s-windres "$@"\n' "$HOST" > "$WORK/bin/windres"
chmod +x "$WORK/bin/uname" "$WORK/bin/windres"

# Copia limpia del codigo fuente / Tiszta másolat a forrásról
rm -rf "$WORK/src"
mkdir -p "$WORK/src"
( cd "$SRCDIR" && tar --exclude='*.o' --exclude='./zesarux' --exclude='./zesarux.exe' \
    --exclude='./ZEsarUX_windows-*' -cf - . ) | tar -xf - -C "$WORK/src"

cd "$WORK/src"

# configure ejecuta pequenos programas para medir tipos. No se pueden ejecutar binarios de Windows,
# asi que se compilan con el gcc nativo (los tamanyos son los mismos en x86_64).
# Ademas, se cambia la ruta fija /c/mingw/ por la nuestra
sed -e 's|`\./\$TEMPFILE\.o`|`gcc -x c $TEMPFILE -o $TEMPFILE.native \&\& ./$TEMPFILE.native; rm -f $TEMPFILE.native`|' \
    -e "s|/c/mingw/|$WORK/mingw/|g" \
    configure > configure.cross
chmod +x configure.cross

export PATH="$WORK/bin:$PATH"

CFLAGS="-DMAX_PATH=260" ./configure.cross --c-compiler $HOST-gcc --enable-sdl2 \
    --disable-pulse --disable-alsa --disable-xwindows --disable-curses --disable-cursesw \
    --disable-fbdev --disable-dsp --disable-onebitspeaker --disable-linuxrealjoystick \
    --disable-aa --disable-caca --disable-sndfile

make -j"$(nproc)"

# Paquete / Csomag
cp "$WORK/mingw/SDL2/bin/SDL2.dll" .
cp "/usr/$HOST/lib/libwinpthread-1.dll" .

rm -rf "$NOMBRE"
mkdir "$NOMBRE"
cp -a ACKNOWLEDGEMENTS LICENSE LICENSES_info Changelog Cambios TODO* README HISTORY FEATURES FEATURES_es \
    EXCLUSIVEFEATURES INSTALL INSTALLWINDOWS IN_MEMORIAM* ALTERNATEROMS INCLUDEDTAPES DONATE DONORS FAQ \
    *.odt mantransfev3.bin *.rom zxuno.flash zxmmcplus.flash tbblue.mmc pcw_8x_boot*dsk zesarux.mp3 \
    zesarux.xcf editionnamegame.* keyboards alternate_roms z88_shortcuts.bmp zesarux.pdf docs licenses \
    zesarux.exe SDL2.dll libwinpthread-1.dll "$NOMBRE"/

mkdir -p "$OUTDIR"
rm -rf "$OUTDIR/$NOMBRE" "$OUTDIR/$NOMBRE.zip"
cp -a "$NOMBRE" "$OUTDIR/"
zip -qr "$OUTDIR/$NOMBRE.zip" "$NOMBRE"

echo
echo "Windows build ready / Elkészült:"
echo "  $OUTDIR/$NOMBRE/zesarux.exe"
echo "  $OUTDIR/$NOMBRE.zip"
