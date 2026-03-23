# BYTEFALL
![Bytefall preview](preview.jpg)
Renders any file as pixels and plays it as audio
# Build
### Windows
> Requires MSYS2 + UCRT64.
```bash
pacman -S mingw-w64-ucrt-x86_64-SDL2
```
```bash
gcc bytefall.c -o bytefall.exe -I/ucrt64/include/SDL2 -L/ucrt64/lib -lmingw32 -lSDL2main -Wl,-Bstatic -lSDL2 -Wl,-Bdynamic -lm -mwindows -lole32 -loleaut32 -limm32 -lwinmm -lversion -lsetupapi -lcfgmgr32
```
### Linux
> Requires SDL2
```bash
sudo apt install libsdl2-dev
```
```bash
gcc bytefall.c -o bytefall $(sdl2-config --cflags --libs) -lm
```
### macOS
> Requires SDL2 via Homebrew
```bash
brew install sdl2
```
```bash
gcc bytefall.c -o bytefall $(sdl2-config --cflags --libs) -lm
```

# Usage
Drag and drop any file onto the window or pass it as an argument:
    ./bytefall myfile.bin

# Pixel modes
| Mode | Bytes per pixel |
| :---:| :---: |
| 8G | 1 - grayscale | 
| RGB | 3 - color (default) | 
| RGBA | 4 - color with alpha | 
| 16G | 2 - 16-bit grayscale | 

# Controls
| Key | Action |
| :---: | :---: |
| `Space` | Play / Pause |
| `R` | Restart |
| `M` | Mute |
| `↑` / `↓` | Speed |
| `?` | Help |
| Mouse wheel | Scroll |
