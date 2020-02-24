#include "Envs.h"
Envs::Res Envs::reset() {
    Res res;
    for (int i = 0; i < NUM_ENVS; i++) {
        _envs[i].reset();
//        for(auto e: _envs.at(i).get_the_1st_observation()){
//            res.feature.at(i).emplace_back(static_cast<float>(e));
//        }
        res.feature.at(i) = _envs.at(i).get_the_1st_observation();
        res.reward.emplace_back(0);
        res.done.emplace_back(false);
    }
    return res;
}
vector<vector<double>> Envs::reset(int env_idx) {
     _envs[env_idx].reset();
    return _envs[env_idx].get_the_1st_observation();
}
Envs::Res Envs::step(const std::vector<std::vector<double>> &actions) {
    Res res;
    Drcu::Res drcu_res;
    for (int i = 0; i < NUM_ENVS; i++) {
        drcu_res = _envs[i].step(actions.at(i));
        res.feature.at(i) = drcu_res.feature;
        res.reward.emplace_back(drcu_res.reward);
        res.done.emplace_back(drcu_res.done);
    }
    return res;
}
Envs::Res Envs::init(int argc, char **short_format_argv) {
    Res res;
    std::vector<std::string> argv (argc); 
    for (int i = 0; i < argc; ++i) {
        argv[i] = short_format_argv[i]; 
    }
    for (int i = 0; i < NUM_ENVS; i++) {
        _envs[i].init(argv);
//        for(auto e: _envs.at(i).get_the_1st_observation()){
//            res.feature.at(i).emplace_back(static_cast<float>(e));
//        }
        res.feature.at(i) = _envs.at(i).get_the_1st_observation();
        res.reward.emplace_back(0);
        res.done.emplace_back(false);
    }
    return res;
}

std::array<double, 4> Envs::get_all_vio() const {
    return _envs[0].get_all_vio();
}
