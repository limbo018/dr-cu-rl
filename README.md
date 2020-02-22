# Building
## Install Dependencies
* `Libtorch`

```bash
wget https://download.pytorch.org/libtorch/cu101/libtorch-shared-with-deps-1.4.0.zip
unzip libtorch*.zip
```

You can also click [here](https://pytorch.org/get-started/locally) to download other versions based on your gcc and cuda.

* `benchmakrs` (The benchmarks can be downloaded from the hompage of [ISPD'18 Contest](http://www.ispd.cc/contests/18/#benchmarks))
* `Boost` (I am using version 1.2, other versions have not been tested)
* `gcc`
* `cmake`
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
cmake -DCMAKE_PREFIX_PATH=/path/to/libtorch
make
```
## How to run
```
./dr-cu-rl -lef path/to/lef -def path/to/lef/file -guide /path/to/def -output output_file_name -threads 8 -tat 200000
```
### Example
```bash
cd build
./dr-cu-rl -lef ../toys/ispd18_sample/ispd18_sample.input.lef -def ../toys/ispd18_sample/ispd18_sample.input.def -guide ../toys/ispd18_sample/ispd18_sample.input.guide -output ispd18_sample.solution.def -threads 8 -tat 200000
```

