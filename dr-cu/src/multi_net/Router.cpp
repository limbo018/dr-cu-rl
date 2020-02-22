#include "Router.h"

#include <utility>
#include "Scheduler.h"

const MTStat &MTStat::operator+=(const MTStat &rhs) {
    auto dur = rhs.durations;
    std::sort(dur.begin(), dur.end());
    if (durations.size() < dur.size()) {
        durations.resize(dur.size(), 0.0);
    }
    for (int i = 0; i < dur.size(); ++i) {
        durations[i] += dur[i];
    }
    return *this;
}

ostream &operator<<(ostream &os, const MTStat mtStat) {
    double minDur = std::numeric_limits<double>::max(), maxDur = 0.0, avgDur = 0.0;
    for (double dur : mtStat.durations) {
        minDur = min(minDur, dur);
        maxDur = max(maxDur, dur);
        avgDur += dur;
    }
    avgDur /= mtStat.durations.size();
    os << "#threads=" << mtStat.durations.size() << " (dur: min=" << minDur << ", max=" << maxDur << ", avg=" << avgDur
       << ")";
    return os;
}

void Router::run() {
    allNetStatus.resize(database.nets.size(), db::RouteStatus::FAIL_UNPROCESSED);
    for (iter = 0; iter < db::setting.rrrIterLimit; iter++) {
        log() << std::endl;
        log() << "################################################################" << std::endl;
        log() << "Start RRR iteration " << iter << std::endl;
        log() << std::endl;
        db::routeStat.clear();
        vector<int> netsToRoute = getNetsToRoute();
        if (netsToRoute.empty()) {
            if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
                log() << "No net is identified for this iteration of RRR." << std::endl;
                log() << std::endl;
            }
            break;
        }
        db::rrrIterSetting.update(iter);
        if (iter > 0) {
            // updateCost should before ripup, otherwise, violated nets have gone
            updateCost(netsToRoute);
            ripup(netsToRoute);
        }
        database.statHistCost();
        if (db::setting.rrrIterLimit > 1) {
            double step = (1.0 - db::setting.rrrInitVioCostDiscount) / (db::setting.rrrIterLimit - 1);
            database.setUnitVioCost(db::setting.rrrInitVioCostDiscount + step * iter);
        }
        if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            db::rrrIterSetting.print();
        }
        route(netsToRoute);
        log() << std::endl;
        log() << "Finish RRR iteration " << iter << std::endl;
        log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
              << std::endl;
        if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            printStat(db::setting.rrrWriteEachIter);
        }
        if (db::setting.rrrWriteEachIter) {
            std::string fn = "iter" + std::to_string(iter) + "_" + db::setting.outputFile;
            printlog("Write result of RRR iter", iter, "to", fn, "...");
            finish();
            database.writeDEF(fn);
            unfinish();
        }
    }
    finish();
    log() << std::endl;
    log() << "################################################################" << std::endl;
    log() << "Finish all RRR iterations and PostRoute" << std::endl;
    log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
          << std::endl;
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printStat(true);
    }
}

vector<int> Router::getNetsToRoute() {
    vector<int> netsToRoute;
    _nets_cost.clear();
    if (iter == 0) {
        auto nets_size = database.nets.size();
//        if(nets_size > 1000)
//            nets_size = 1000;
        for (int i = 0; i < nets_size; i++) {
            // if (database.nets[i].getName() == "net8984") netsToRoute.push_back(i);
            netsToRoute.push_back(i);
            _nets_cost.emplace_back(0);
        }
    } else {
        for (auto &net : database.nets) {
            if (UpdateDB::checkViolation(net)) {
                netsToRoute.push_back(net.idx);
                _nets_cost.emplace_back(static_cast<float>(UpdateDB::get_net_vio_cost(net)));
            }
        }
    }

    return netsToRoute;
}

void Router::ripup(const vector<int> &netsToRoute) {
    for (auto netIdx : netsToRoute) {
        UpdateDB::clearRouteResult(database.nets[netIdx]);
        allNetStatus[netIdx] = db::RouteStatus::FAIL_UNPROCESSED;
    }
}

void Router::updateCost(const vector<int> &netsToRoute) {
    database.addHistCost();
    database.fadeHistCost(netsToRoute);
}

void Router::route(const vector<int> &netsToRoute) {
    // init SingleNetRouters
    vector<SingleNetRouter> routers;
    routers.reserve(netsToRoute.size());
    for (int netIdx : netsToRoute) {
        routers.emplace_back(database.nets[netIdx]);
    }

    // pre route
    auto preMT = runJobsMT(netsToRoute.size(), [&](int netIdx) { routers[netIdx].preRoute(); });
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("preMT", preMT);
        printStat();
    }

    // schedule
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Start multi-thread scheduling. There are " << netsToRoute.size() << " nets to route." << std::endl;
    }
    Scheduler scheduler(routers);
    const vector<vector<int>> &batches = scheduler.schedule();
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Finish multi-thread scheduling" << ((db::setting.numThreads == 0) ? " using simple mode" : "")
              << ". There will be " << batches.size() << " batches." << std::endl;
        log() << std::endl;
    }

    // maze route and commit DB by batch
    int iBatch = 0;
    MTStat allMazeMT, allCommitMT, allGetViaTypesMT, allCommitViaTypesMT;
    for (const vector<int> &batch : batches) {
        // 1 maze route
        auto mazeMT = runJobsMT(batch.size(), [&](int jobIdx) {
            auto &router = routers[batch[jobIdx]];
            router.mazeRoute();
            allNetStatus[router.dbNet.idx] = router.status;
        });
        allMazeMT += mazeMT;
        // 2 commit nets to DB
        auto commitMT = runJobsMT(batch.size(), [&](int jobIdx) {
            auto &router = routers[batch[jobIdx]];
            if (!db::isSucc(router.status)) return;
            router.commitNetToDB();
        });
        allCommitMT += commitMT;
        // 3 get via types
        allGetViaTypesMT += runJobsMT(batch.size(), [&](int jobIdx) {
            auto &router = routers[batch[jobIdx]];
            if (!db::isSucc(router.status)) return;
            PostRoute postRoute(router.dbNet);
            postRoute.getViaTypes();
        });
        allCommitViaTypesMT += runJobsMT(batch.size(), [&](int jobIdx) {
            auto &router = routers[batch[jobIdx]];
            if (!db::isSucc(router.status)) return;
            UpdateDB::commitViaTypes(router.dbNet);
        });
        // 4 stat
        if (db::setting.multiNetVerbose >= +db::VerboseLevelT::HIGH && db::setting.numThreads != 0) {
            int maxNumVertices = 0;
            for (int i : batch) maxNumVertices = std::max(maxNumVertices, routers[i].localNet.estimatedNumOfVertices);
            log() << "Batch " << iBatch << " done: size=" << batch.size() << ", mazeMT " << mazeMT << ", commitMT "
                  << commitMT << ", peakM=" << utils::mem_use::get_peak() << ", maxV=" << maxNumVertices << std::endl;
        }
        iBatch++;
    }
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("allMazeMT", allMazeMT);
        printlog("allCommitMT", allCommitMT);
        printlog("allGetViaTypesMT", allGetViaTypesMT);
        printlog("allCommitViaTypesMT", allCommitViaTypesMT);
    }
}

void Router::finish() {
    PostScheduler postScheduler(database.nets);
    const vector<vector<int>> &batches = postScheduler.schedule();
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("There will be", batches.size(), "batches for getting via types.");
    }
    // 1. redo min area handling
    MTStat allPostMaze2MT;
    for (const vector<int> &batch : batches) {
        runJobsMT(batch.size(), [&](int jobIdx) {
            int netIdx = batch[jobIdx];
            if (!db::isSucc(allNetStatus[netIdx])) return;
            UpdateDB::clearMinAreaRouteResult(database.nets[netIdx]);
        });
        allPostMaze2MT += runJobsMT(batch.size(), [&](int jobIdx) {
            int netIdx = batch[jobIdx];
            if (!db::isSucc(allNetStatus[netIdx])) return;
            PostMazeRoute(database.nets[netIdx]).run2();
        });
        runJobsMT(batch.size(), [&](int jobIdx) {
            int netIdx = batch[jobIdx];
            if (!db::isSucc(allNetStatus[netIdx])) return;
            UpdateDB::commitMinAreaRouteResult(database.nets[netIdx]);
        });
    }
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("allPostMaze2MT", allPostMaze2MT);
    }
    // 2. get via types again
    for (int iter = 0; iter < db::setting.multiNetSelectViaTypesIter; iter++) {
        MTStat allGetViaTypesMT, allCommitViaTypesMT;
        for (const vector<int> &batch : batches) {
            allGetViaTypesMT += runJobsMT(batch.size(), [&](int jobIdx) {
                int netIdx = batch[jobIdx];
                if (!db::isSucc(allNetStatus[netIdx])) return;
                PostRoute postRoute(database.nets[netIdx]);
                if (iter == 0) postRoute.considerViaViaVio = false;
                postRoute.getViaTypes();
            });
            allCommitViaTypesMT += runJobsMT(batch.size(), [&](int jobIdx) {
                int netIdx = batch[jobIdx];
                if (!db::isSucc(allNetStatus[netIdx])) return;
                UpdateDB::commitViaTypes(database.nets[netIdx]);
            });
        }
        if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            printlog("allGetViaTypesMT", allGetViaTypesMT);
            printlog("allCommitViaTypesMT", allCommitViaTypesMT);
        }
    }
    // 3. post route
    auto postMT = runJobsMT(database.nets.size(), [&](int netIdx) {
        if (!db::isSucc(allNetStatus[netIdx])) return;
        PostRoute postRoute(database.nets[netIdx]);
        postRoute.run();
    });
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("postMT", postMT);
    }
    // final open fix
    if (db::setting.fixOpenBySST) {
        int count = 0;
        for (auto &net : database.nets) {
            if (net.defWireSegments.empty() && net.numOfPins() > 1) {
                connectBySTT(net);
                count++;
            }
        }
        if (count > 0) log() << "#nets connected by STT: " << count << std::endl;
    }
}

void Router::unfinish() {
    runJobsMT(database.nets.size(), [&](int netIdx) { database.nets[netIdx].clearPostRouteResult(); });
}

double Router::printStat(bool major) {
    double total_score = 0;
    log() << std::endl;
    log() << "----------------------------------------------------------------" << std::endl;
    db::routeStat.print();
    if (major) {
        total_score = database.printAllUsageAndVio();
    }
    log() << "----------------------------------------------------------------" << std::endl;
    log() << std::endl;
    return total_score;
}

int Router::step() {
    if (iter < db::setting.rrrIterLimit) {
        if (db::setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
            log() << std::endl;
            log() << "################################################################" << std::endl;
            log() << "Start RRR iteration " << iter << std::endl;
            log() << std::endl;
        }
        db::routeStat.clear();
        vector<int> netsToRoute = getNetsToRoute();
        if (netsToRoute.empty()) {
            if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
                log() << "No net is identified for this iteration of RRR." << std::endl;
                log() << std::endl;
            }
            return -1;
        }
        db::rrrIterSetting.update(iter);
        if (iter > 0) {
            // updateCost should before ripup, otherwise, violated nets have gone
            updateCost(netsToRoute);
//            ripup(netsToRoute);
        }
        ripup(netsToRoute);
        database.statHistCost();
        if (db::setting.rrrIterLimit > 1) {
            double step = (1.0 - db::setting.rrrInitVioCostDiscount) / (db::setting.rrrIterLimit - 1);
            database.setUnitVioCost(db::setting.rrrInitVioCostDiscount + step * iter);
        }
        if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            db::rrrIterSetting.print();
        }
        route(netsToRoute);
        if (db::setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
            log() << std::endl;
            log() << "Finish RRR iteration " << iter << std::endl;
            log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
                  << std::endl;
        }
        if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            printStat(db::setting.rrrWriteEachIter);
        }
        if (db::setting.rrrWriteEachIter) {
            std::string fn = "iter" + std::to_string(iter) + "_" + db::setting.outputFile;
            printlog("Write result of RRR iter", iter, "to", fn, "...");
            finish();
            database.writeDEF(fn);
            unfinish();
        }
        iter++;
        return 0;
    } else {
        finish();
        if (db::setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
            log() << std::endl;
            log() << "################################################################" << std::endl;
            log() << "Finish all RRR iterations and PostRoute" << std::endl;
            log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
                  << std::endl;
        }
        if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            printStat(true);
        }
        return 1;
    }
}

void Router::reset() {
    for (auto &e: _feature) {
        e.assign(Router::Feature_idx::FEA_DIM, 0);
    }
    iter = 0;
    vector<int> all_nets;
    for (int i = 0; i < database.nets.size(); ++i)
        all_nets.emplace_back(i);
    ripup(all_nets);
}

void Router::init() {
    allNetStatus.resize(database.nets.size(), db::RouteStatus::FAIL_UNPROCESSED);
    _feature.resize(database.nets.size());
    for (auto &e: _feature) {
        e.resize(Router::Feature_idx::FEA_DIM, 0);
    }
    iter = 0;
}

void Router::pre_route(const vector<int> &netsToRoute) {
    // init SingleNetRouters
    _routers.clear();
    _routers.reserve(netsToRoute.size());
    for (int netIdx : netsToRoute) {
        _routers.emplace_back(database.nets[netIdx]);
    }

    // pre route
    auto preMT = runJobsMT(netsToRoute.size(), [&](int netIdx) { _routers[netIdx].preRoute(); });
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("preMT", preMT);
        printStat();
    }
}

vector<vector<int>> Router::get_nets_feature() {
    Scheduler scheduler(_routers);
    vector<int> degree = scheduler.get_net_degree();

    auto reset_mt = runJobsMT(_feature.size(), [&](int i) {
        _feature.at(i).at(ROUTED) = 0;
    });

    auto feature_mt = runJobsMT(_routers.size(), [&](int i) {
        auto net_id = _routers.at(i).dbNet.idx;
        _feature.at(net_id).at(ROUTED) = 1;
        _feature.at(net_id).at(SIZE) = _routers.at(i).localNet.estimatedNumOfVertices;
        _feature.at(net_id).at(DEGREE) = degree.at(i);
        _feature.at(net_id).at(NUM_RIP_UP) += 1;
        _feature.at(net_id).at(VIO_COST) = _nets_cost.at(i);
        if (iter > 0) {
            if (_wire_usage_length.count(net_id))
                _feature.at(net_id).at(WIRE_LENGTH) = _wire_usage_length[net_id];
            if (_via_usage.count(net_id))
                _feature.at(net_id).at(VIA) = _via_usage[net_id];
            if (_layer_usage.count(net_id)) {
                for (auto layer_idx: _layer_usage[net_id]) {
                    _feature.at(net_id).at(LAYER_BEGIN+layer_idx) = 1;
                }
            }
        }
    });
    return vector<vector<int>>(_feature);
}

int Router::prepare() {
    if (!iter)
        allNetStatus.resize(database.nets.size(), db::RouteStatus::FAIL_UNPROCESSED);
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << std::endl;
        log() << "################################################################" << std::endl;
        log() << "Start RRR iteration " << iter << std::endl;
        log() << std::endl;
    }
    db::routeStat.clear();
    _nets_to_route = getNetsToRoute();
    if (_nets_to_route.empty()) {
        if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            log() << "No net is identified for this iteration of RRR." << std::endl;
            log() << std::endl;
        }
        return -1;
    }
    db::rrrIterSetting.update(iter);
    if (iter > 0) {
        // updateCost should before ripup, otherwise, violated nets have gone
        updateCost(_nets_to_route);
        _via_usage.clear();
        _wire_usage_length.clear();
        _layer_usage.clear();
        database.get_net_wire_vio_usage(_via_usage, _wire_usage_length, _layer_usage);
        ripup(_nets_to_route);
    }
    database.statHistCost();
    if (db::setting.rrrIterLimit > 1) {
        double step = (1.0 - db::setting.rrrInitVioCostDiscount) / (db::setting.rrrIterLimit - 1);
        database.setUnitVioCost(db::setting.rrrInitVioCostDiscount + step * iter);
    }
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        db::rrrIterSetting.print();
    }
    pre_route(_nets_to_route);
    // schedule
    return 0;
}

double Router::route(vector<double> rank_score) {
    // schedule
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Start multi-thread scheduling. There are " << _nets_to_route.size() << " nets to route." << std::endl;
    }
    Scheduler scheduler(_routers);
    const vector<vector<int>> &batches = scheduler.schedule(std::move(rank_score));
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Finish multi-thread scheduling" << ((db::setting.numThreads == 0) ? " using simple mode" : "")
              << ". There will be " << batches.size() << " batches." << std::endl;
        log() << std::endl;
    }

    // maze route and commit DB by batch
    int iBatch = 0;
    MTStat allMazeMT, allCommitMT, allGetViaTypesMT, allCommitViaTypesMT;
    for (const vector<int> &batch : batches) {
        // 1 maze route
        auto mazeMT = runJobsMT(batch.size(), [&](int jobIdx) {
            auto &router = _routers[batch[jobIdx]];
            router.mazeRoute();
            allNetStatus[router.dbNet.idx] = router.status;
        });
        allMazeMT += mazeMT;
        // 2 commit nets to DB
        auto commitMT = runJobsMT(batch.size(), [&](int jobIdx) {
            auto &router = _routers[batch[jobIdx]];
            if (!db::isSucc(router.status)) return;
            router.commitNetToDB();
        });
        allCommitMT += commitMT;
        // 3 get via types
        allGetViaTypesMT += runJobsMT(batch.size(), [&](int jobIdx) {
            auto &router = _routers[batch[jobIdx]];
            if (!db::isSucc(router.status)) return;
            PostRoute postRoute(router.dbNet);
            postRoute.getViaTypes();
        });
        allCommitViaTypesMT += runJobsMT(batch.size(), [&](int jobIdx) {
            auto &router = _routers[batch[jobIdx]];
            if (!db::isSucc(router.status)) return;
            UpdateDB::commitViaTypes(router.dbNet);
        });
        // 4 stat
        if (db::setting.multiNetVerbose >= +db::VerboseLevelT::HIGH && db::setting.numThreads != 0) {
            int maxNumVertices = 0;
            for (int i : batch) maxNumVertices = std::max(maxNumVertices, _routers[i].localNet.estimatedNumOfVertices);
            log() << "Batch " << iBatch << " done: size=" << batch.size() << ", mazeMT " << mazeMT << ", commitMT "
                  << commitMT << ", peakM=" << utils::mem_use::get_peak() << ", maxV=" << maxNumVertices << std::endl;
        }
        iBatch++;
    }
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("allMazeMT", allMazeMT);
        printlog("allCommitMT", allCommitMT);
        printlog("allGetViaTypesMT", allGetViaTypesMT);
        printlog("allCommitViaTypesMT", allCommitViaTypesMT);
    }
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << std::endl;
        log() << "Finish RRR iteration " << iter << std::endl;
        log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
              << std::endl;
    }
    double total_score = 0;
    total_score = get_score();

    if (db::setting.rrrWriteEachIter) {
        std::string fn = "iter" + std::to_string(iter) + "_" + db::setting.outputFile;
        printlog("Write result of RRR iter", iter, "to", fn, "...");
        finish();
        database.writeDEF(fn);
        unfinish();
    }
    iter ++;
    return total_score;
}

double Router::get_score() {
    return database.get_score();
}
