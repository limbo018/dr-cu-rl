#pragma once
#include "vector"
#include "Drcu.h"

#define NUM_ENVS 1

class Envs {
public:
    struct Res{
        vector<vector<float>> feature{NUM_ENVS};
        vector<bool> done;
        vector<float> reward;
    };
    Res reset();
    vector<int> reset(int env_idx);
    Res step(const std::vector<std::vector<float >>& actions);
    Res init(int argc, char* short_format_argv[]);
private:
    std::vector<Drcu> _envs{NUM_ENVS};


};
