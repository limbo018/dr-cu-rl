#pragma once

#include "LocalNet.h"

class PostMazeRoute {
public:
    PostMazeRoute(db::NetBase& netBase) : net(netBase) {}
    void run(db::Database& database);
    void run2(db::Database& database);  // rerun extendMinAreaWires

private:
    db::NetBase& net;

    // Remove track switch which goes to another layer and back with via spacing violation
    void removeTrackSwitchWithVio(db::Database& database);

    // Extend wires to resolve min area violations
    void extendMinAreaWires(db::Database& database);
    void getExtendWireRects(db::Database& database, const std::vector<std::shared_ptr<db::GridSteiner>>& candEdge) const;
};
