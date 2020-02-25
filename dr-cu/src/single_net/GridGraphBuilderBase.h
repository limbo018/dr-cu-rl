#pragma once

#include "GridGraph.h"
#include "LocalNet.h"

class GridGraphBuilderBase {
public:
    GridGraphBuilderBase(db::Database const& database, LocalNet &localNetData, GridGraph &gridGraph)
        : localNet(localNetData),
          graph(gridGraph),
          vertexToGridPoint(graph.vertexToGridPoint),
          minAreaFixable(graph.minAreaFixable) {
        graph.pinToVertex.resize(localNetData.numOfPins());

        outOfPinWireLengthPenalty = database.setting().weightWrongWayWirelength / database.setting().weightWirelength + 1;
    }

    virtual void run(db::Database const& database) = 0;

protected:
    LocalNet &localNet;
    GridGraph &graph;

    // reference to GridGraph
    vector<db::GridPoint> &vertexToGridPoint;
    vector<bool> &minAreaFixable;

    vector<std::pair<int, int>> intervals;
    vector<vector<int>> pinToOriVertex;

    // Besides wrong-way wire cost itself, discourage out-of-pin taps slightly more
    // Because violations between link and via/wire are out of control now
    double outOfPinWireLengthPenalty;

    // TODO: replace getPinPointCost() by PinTapConnector
    double getPinPointCost(db::Database const& database, const vector<db::BoxOnLayer> &accessBoxes, const db::GridPoint &grid);
    void updatePinVertex(db::Database const& database, int pinIdx, int vertexIdx, bool fakePin = false);
    void addOutofPinPenalty(db::Database const& database);
    virtual void setMinAreaFlags(db::Database const& database) = 0;
    void fixDisconnectedPin();
};
