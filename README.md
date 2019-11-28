# Building
## Get the source
```bash
git clone git@github.com:tyleryy/dr-cu-rl.git
git submodule sync
git submodule update --init --recursive
```
## Build
```bash
cd dr-cu-rl
mkdir build && cd build
cmake ..
make
```
