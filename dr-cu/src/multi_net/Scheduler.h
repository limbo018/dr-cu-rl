#pragma once

#include "db/Database.h"
#include "single_net/SingleNetRouter.h"

class Scheduler {
public:
    Scheduler(const vector<SingleNetRouter>& routersToExec) : routers(routersToExec){};
    vector<vector<int>>& schedule(db::Database const& database);
    vector<vector<int>>& schedule(db::Database const& database, vector<double> rank_score);
    vector<int> get_net_degree(db::Database const& database);

private:
    const vector<SingleNetRouter>& routers;
    vector<vector<int>> batches;

    // for conflict detect
    RTrees rtrees;
    virtual void initSet(db::Database const& database, vector<int> jobIdxes);
    virtual void updateSet(db::Database const& database, int jobIdx);
    virtual bool hasConflict(db::Database const& database, int jobIdx);
};

class PostScheduler {
public:
    PostScheduler(const vector<db::Net>& netsToExec) : dbNets(netsToExec){};
    vector<vector<int>>& schedule(db::Database const& database);

private:
    const vector<db::Net>& dbNets;
    vector<vector<int>> batches;

    // for conflict detect
    RTrees rtrees;
    boostBox getBoostBox(db::Database const& database, const db::GridPoint &gp);
    boostBox getBoostBox(db::Database const& database, const db::GridEdge &edge);
    vector<std::pair<boostBox, int>> getNetBoxes(db::Database const& database, const db::Net& dbNet);
    virtual void initSet(db::Database const& database, vector<int> jobIdxes);
    virtual void updateSet(db::Database const& database, int jobIdx);
    virtual bool hasConflict(db::Database const& database, int jobIdx);   
};
