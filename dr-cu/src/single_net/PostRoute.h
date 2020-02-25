#pragma once

#include "LocalNet.h"

class PostRoute {
public:
    PostRoute(db::Net& databaseNet) : dbNet(databaseNet) {}
    
    db::RouteStatus run(db::Database& database);
    void getViaTypes(db::Database& database);
    bool considerViaViaVio = true;

private:
    db::Net& dbNet;
    vector<std::shared_ptr<db::GridSteiner>> pinTaps;
    std::unordered_map<std::shared_ptr<db::GridSteiner>, vector<utils::SegmentT<DBU>>> linkToPins;
    std::unordered_map<std::shared_ptr<db::GridSteiner>, std::pair<int, utils::PointT<DBU>>> linkViaToPins;

    // 1. Get pin taps
    void getPinTapPoints();

    // 2. Connect taps to pins by L-shape links
    db::RouteStatus connectPins(db::Database& database);

    // 3. Get final topo (with metal fill)
    void getTopo(db::Database& database);
    bool hasNeighPin(std::shared_ptr<db::GridSteiner> node, std::shared_ptr<db::GridSteiner> neighNode) const {
        return (neighNode->pinIdx >= 0 && neighNode->layerIdx == node->layerIdx);
    }
    utils::BoxT<DBU> getEdgeLayerMetal(db::Database const& database, const db::GridEdge& edge,
                                       const db::ViaType* viaType);  // metal on edge.u.layerIdx

    void processAllSameLayerSameNetBoxes(db::Database const& database, 
                                         const vector<db::BoxOnLayer>& pinAccessBoxes,
                                         int layerIdx,
                                         const vector<utils::SegmentT<DBU>>& linkToPin,
                                         const std::function<void(const utils::BoxT<DBU>&)>& handle);
    const db::ViaType* getViaType(db::Database const& database, std::shared_ptr<db::GridSteiner> u, std::shared_ptr<db::GridSteiner> v, int pinIdx);
    utils::BoxT<DBU> getWireSegmentMetal(db::Database const& database, const db::GridEdge& edge);
};

class MetalFiller {
public:
    MetalFiller(const vector<utils::BoxT<DBU>>& targetMetalRects,
                int layerIndex,
                db::AggrParaRunSpace aggressiveSpacing = db::AggrParaRunSpace::DEFAULT)
        : targetMetals(targetMetalRects), layerIdx(layerIndex), aggrSpace(aggressiveSpacing) {}

    void run(db::Database const& database);
    vector<utils::BoxT<DBU>> fillMetals;
    db::AggrParaRunSpace aggrSpace;

private:
    void getFillRect(db::Database const& database, utils::BoxT<DBU> targetMetal);  // by value as the vector may be reallocated
    void iterateAllMetals(const std::function<void(const utils::BoxT<DBU>&)> handle);  // target & fill
    vector<utils::BoxT<DBU>> targetMetals;
    int layerIdx;
};

void connectBySTT(db::Database& database, db::Net& net);
