#pragma once

#include "LocalNet.h"

class PreRoute {
public:
    PreRoute(LocalNet& localNetData) : localNet(localNetData) {}

    db::RouteStatus runIterative(db::Database& database);

private:
    LocalNet& localNet;

    db::RouteStatus run(db::Database& database, int numPitchForGuideExpand);
    void expandGuidesToMargin(db::Database const& database);
    db::RouteStatus expandGuidesToCoverPins(db::Database& database);

    bool checkGuideConnTrack(db::Database const& database) const;
};
