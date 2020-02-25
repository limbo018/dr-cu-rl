#pragma once

#include "db/Database.h"
#include "single_net/SingleNetRouter.h"

class Router {
public:
    void run(db::Database& database);
    int step(db::Database& database);
    void reset(db::Database& database);
    void init(db::Database& database);
    vector<int> getNetsToRoute(db::Database& database);

    vector<vector<int>> get_nets_feature(db::Database& database);
    double route(db::Database& database, vector<double> rank_score);
    int prepare(db::Database& database);
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

    void ripup(db::Database& database, const vector<int>& netsToRoute);
    void updateCost(db::Database& database, const vector<int>& netsToRoute);
    void route(db::Database& database, const vector<int>& netsToRoute);
    void finish(db::Database& database);
    void unfinish(db::Database& database);

    void pre_route(db::Database& database, const vector<int>& netsToRoute);
    double printStat(db::Database const& database, bool major = false);
    double get_score(db::Database& database);
};
