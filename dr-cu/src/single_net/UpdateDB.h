#pragma once

#include "LocalNet.h"

class UpdateDB {
public:
    // Note: after commitRouteResult, localNet should not be used (as move())
    static void commitRouteResult(db::Database& database, LocalNet& localNet, db::Net& dbNet);
    static void clearRouteResult(db::Database& database, db::Net& dbNet);
    static void commitMinAreaRouteResult(db::Database& database, db::Net& dbNet);
    static void clearMinAreaRouteResult(db::Database& database, db::Net& dbNet);
    static void commitViaTypes(db::Database& database, db::Net& dbNet);
    static bool checkViolation(db::Database const& database, db::Net& dbNet);
    static double get_net_vio_cost(db::Database const& database, db::Net &dbNet);
};
