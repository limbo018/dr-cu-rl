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
vector<vector<float>> Envs::reset(int env_idx) {
     _envs[env_idx].reset();
    return _envs[env_idx].get_the_1st_observation();
}
Envs::Res Envs::step(const std::vector<std::vector<float>> &actions) {
    Res res;
    Drcu::Res drcu_res;
    for (int i = 0; i < NUM_ENVS; i++) {
        drcu_res = _envs[i].step(actions.at(i));
        res.feature.at(i) = drcu_res.feature;
        if (res.feature.at(i).size() < NET_NUM) {
            vector<vector<float>> feature_fill(NET_NUM - res.feature.at(i).size(), {0, 0});
            res.feature.at(i).insert(res.feature.at(i).end(), feature_fill.begin(), feature_fill.end());
        }
        res.reward.emplace_back(drcu_res.reward);
        res.done.emplace_back(drcu_res.done);
    }
    return res;
}
Envs::Res Envs::init(int argc, char **short_format_argv) {
    Res res;
    for (int i = 0; i < NUM_ENVS; i++) {
        _envs[i].init(argc, short_format_argv);
//        for(auto e: _envs.at(i).get_the_1st_observation()){
//            res.feature.at(i).emplace_back(static_cast<float>(e));
//        }
        res.feature.at(i) = _envs.at(i).get_the_1st_observation();
        res.reward.emplace_back(0);
        res.done.emplace_back(false);
    }
    return res;
}
