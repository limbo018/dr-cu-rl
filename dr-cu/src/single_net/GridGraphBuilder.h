#pragma once

#include "GridGraphBuilderBase.h"

class GridGraphBuilder : public GridGraphBuilderBase {
public:
    using GridGraphBuilderBase::GridGraphBuilderBase;

    void run(db::Database const& database);

private:
    void connectGuide(db::Database const& database, int guideIdx);
    void addRegWrongWayConn(db::Database const& database, int guideIdx);
    void addPinWrongWayConn(db::Database const& database);
    void addAdjGuideWrongWayConn(db::Database const& database);
    void addWrongWayConn(db::Database const& database);
    void connectTwoGuides(db::Database const& database, int guideIdx1, int guideIdx2);

    void setMinAreaFlags(db::Database const& database);

    int guideToVertex(int gIdx, int trackIdx, int cpIdx) const;
    int boxToVertex(const db::GridBoxOnLayer& box, int pointBias, int trackIdx, int cpIdx) const;
};
