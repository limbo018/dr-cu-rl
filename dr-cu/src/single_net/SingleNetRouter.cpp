#include "SingleNetRouter.h"
#include "PreRoute.h"
#include "MazeRoute.h"
#include "PostMazeRoute.h"
#include "UpdateDB.h"
#include "PostRoute.h"

SingleNetRouter::SingleNetRouter(db::Net& databaseNet)
    : localNet(databaseNet), dbNet(databaseNet), status(db::RouteStatus::SUCC_NORMAL) {}

void SingleNetRouter::preRoute(db::Database& database) {
    // Pre-route (obtain proper grid boxes)
    status &= PreRoute(localNet).runIterative(database);
}

void SingleNetRouter::mazeRoute(db::Database& database) {
    // Maze route (working on grid only)
    status &= MazeRoute(localNet).run(database);
    PostMazeRoute(localNet).run(database);
}

void SingleNetRouter::commitNetToDB(db::Database& database) {
    // Commit net to DB (commit result to DB)
    UpdateDB::commitRouteResult(database, localNet, dbNet);
}
