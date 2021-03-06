#pragma once
#include "vector"
#include "Drcu.h"

#define NUM_ENVS 1

class Envs {
public:
    struct Res{
        vector<vector<vector<double>>> feature{NUM_ENVS};
        vector<bool> done;
        vector<float> reward;
    };
    Res reset();
    vector<vector<double>> reset(int env_idx);
    Res step(const std::vector<std::vector<double>>& actions);
    Res init(int argc, char* short_format_argv[]);
    std::array<double, 4> get_all_vio() const;
private:
    std::vector<Drcu> _envs{NUM_ENVS};


};
