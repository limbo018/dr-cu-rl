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
    double route(vector<float > rank_score);
    int prepare();
    enum Feature_idx{ROUTED=0, SIZE, DEGREE, NUM_RIP_UP, FEA_DIM};
private:
    int iter=0;
    vector<db::RouteStatus> allNetStatus;
    vector<SingleNetRouter> _routers;
    vector<int> _nets_to_route;
    vector<vector<int>> _feature;

    void ripup(const vector<int>& netsToRoute);
    void updateCost(const vector<int>& netsToRoute);
    void route(const vector<int>& netsToRoute);
    void finish();
    void unfinish();

    void pre_route(const vector<int>& netsToRoute);
    double printStat(bool major = false);
    double get_score();
};
