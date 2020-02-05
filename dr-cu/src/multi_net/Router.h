#pragma once

#include "db/Database.h"
#include "single_net/SingleNetRouter.h"

class Router {
public:
    void run();
    int step();
    void reset();
    void init();
    vector<int> getNetsToRoute();

    vector<vector<int>> get_nets_feature();
    double route(vector<double> rank_score);
    int prepare();
    enum Feature_idx{ROUTED=0, SIZE, DEGREE, NUM_RIP_UP, VIO_COST, WIRE_LENGTH, VIA, LAYER_BEGIN,LAYER_END=LAYER_BEGIN+9-1, FEA_DIM};
private:
    int iter=0;
    vector<float> _nets_cost;
    vector<db::RouteStatus> allNetStatus;
    vector<SingleNetRouter> _routers;
    vector<int> _nets_to_route;
    vector<vector<int>> _feature;

    std::unordered_map<int, int> _via_usage;
    std::unordered_map<int, float> _wire_usage_length;
    std::unordered_map<int, std::set<int>> _layer_usage;

    void ripup(const vector<int>& netsToRoute);
    void updateCost(const vector<int>& netsToRoute);
    void route(const vector<int>& netsToRoute);
    void finish();
    void unfinish();

    void pre_route(const vector<int>& netsToRoute);
    double printStat(bool major = false);
    double get_score();
};
