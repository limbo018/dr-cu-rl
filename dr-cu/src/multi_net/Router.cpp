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

void Router::run(db::Database& database) {
    allNetStatus.resize(database.nets.size(), db::RouteStatus::FAIL_UNPROCESSED);
    for (iter = 0; iter < database.setting().rrrIterLimit; iter++) {
        log() << std::endl;
        log() << "################################################################" << std::endl;
        log() << "Start RRR iteration " << iter << std::endl;
        log() << std::endl;
        database.routeStat().clear();
        vector<int> netsToRoute = getNetsToRoute(database);
        if (netsToRoute.empty()) {
            if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
                log() << "No net is identified for this iteration of RRR." << std::endl;
                log() << std::endl;
            }
            break;
        }
        database.rrrIterSetting().update(database, iter);
        if (iter > 0) {
            // updateCost should before ripup, otherwise, violated nets have gone
            updateCost(database, netsToRoute);
            ripup(database, netsToRoute);
        }
        database.statHistCost(database);
        if (database.setting().rrrIterLimit > 1) {
            double step = (1.0 - database.setting().rrrInitVioCostDiscount) / (database.setting().rrrIterLimit - 1);
            database.setUnitVioCost(database, database.setting().rrrInitVioCostDiscount + step * iter);
        }
        if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            database.rrrIterSetting().print();
        }
        route(database, netsToRoute);
        log() << std::endl;
        log() << "Finish RRR iteration " << iter << std::endl;
        log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
              << std::endl;
        if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            printStat(database, database.setting().rrrWriteEachIter);
        }
        if (database.setting().rrrWriteEachIter) {
            std::string fn = "iter" + std::to_string(iter) + "_" + database.setting().outputFile;
            printlog("Write result of RRR iter", iter, "to", fn, "...");
            finish(database);
            database.writeDEF(fn);
            unfinish(database);
        }
    }
    finish(database);
    log() << std::endl;
    log() << "################################################################" << std::endl;
    log() << "Finish all RRR iterations and PostRoute" << std::endl;
    log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
          << std::endl;
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printStat(database, true);
    }
}

vector<int> Router::getNetsToRoute(db::Database& database) {
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
            if (UpdateDB::checkViolation(database, net)) {
                netsToRoute.push_back(net.idx);
                _nets_cost.emplace_back(static_cast<float>(UpdateDB::get_net_vio_cost(database, net)));
            }
        }
    }

    return netsToRoute;
}

void Router::ripup(db::Database& database, const vector<int> &netsToRoute) {
    for (auto netIdx : netsToRoute) {
        UpdateDB::clearRouteResult(database, database.nets[netIdx]);
        allNetStatus[netIdx] = db::RouteStatus::FAIL_UNPROCESSED;
    }
}

void Router::updateCost(db::Database& database, const vector<int> &netsToRoute) {
    database.addHistCost(database);
    database.fadeHistCost(database, netsToRoute);
}

void Router::route(db::Database& database, const vector<int> &netsToRoute) {
    // init SingleNetRouters
    vector<SingleNetRouter> routers;
    routers.reserve(netsToRoute.size());
    for (int netIdx : netsToRoute) {
        routers.emplace_back(database.nets[netIdx]);
    }

    // pre route
    auto preMT = runJobsMT(database, netsToRoute.size(), [&](int netIdx) { routers[netIdx].preRoute(database); });
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("preMT", preMT);
        printStat(database);
    }

    // schedule
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Start multi-thread scheduling. There are " << netsToRoute.size() << " nets to route." << std::endl;
    }
    Scheduler scheduler(routers);
    const vector<vector<int>> &batches = scheduler.schedule(database);
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Finish multi-thread scheduling" << ((database.setting().numThreads == 0) ? " using simple mode" : "")
              << ". There will be " << batches.size() << " batches." << std::endl;
        log() << std::endl;
    }

    // maze route and commit DB by batch
    int iBatch = 0;
    MTStat allMazeMT, allCommitMT, allGetViaTypesMT, allCommitViaTypesMT;
    for (const vector<int> &batch : batches) {
        // 1 maze route
        auto mazeMT = runJobsMT(database, batch.size(), [&](int jobIdx) {
            auto &router = routers[batch[jobIdx]];
            router.mazeRoute(database);
            allNetStatus[router.dbNet.idx] = router.status;
        });
        allMazeMT += mazeMT;
        // 2 commit nets to DB
        auto commitMT = runJobsMT(database, batch.size(), [&](int jobIdx) {
            auto &router = routers[batch[jobIdx]];
            if (!db::isSucc(router.status)) return;
            router.commitNetToDB(database);
        });
        allCommitMT += commitMT;
        // 3 get via types
        allGetViaTypesMT += runJobsMT(database, batch.size(), [&](int jobIdx) {
            auto &router = routers[batch[jobIdx]];
            if (!db::isSucc(router.status)) return;
            PostRoute postRoute(router.dbNet);
            postRoute.getViaTypes(database);
        });
        allCommitViaTypesMT += runJobsMT(database, batch.size(), [&](int jobIdx) {
            auto &router = routers[batch[jobIdx]];
            if (!db::isSucc(router.status)) return;
            UpdateDB::commitViaTypes(database, router.dbNet);
        });
        // 4 stat
        if (database.setting().multiNetVerbose >= +db::VerboseLevelT::HIGH && database.setting().numThreads != 0) {
            int maxNumVertices = 0;
            for (int i : batch) maxNumVertices = std::max(maxNumVertices, routers[i].localNet.estimatedNumOfVertices);
            log() << "Batch " << iBatch << " done: size=" << batch.size() << ", mazeMT " << mazeMT << ", commitMT "
                  << commitMT << ", peakM=" << utils::mem_use::get_peak() << ", maxV=" << maxNumVertices << std::endl;
        }
        iBatch++;
    }
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("allMazeMT", allMazeMT);
        printlog("allCommitMT", allCommitMT);
        printlog("allGetViaTypesMT", allGetViaTypesMT);
        printlog("allCommitViaTypesMT", allCommitViaTypesMT);
    }
}

void Router::finish(db::Database& database) {
    PostScheduler postScheduler(database.nets);
    const vector<vector<int>> &batches = postScheduler.schedule(database);
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("There will be", batches.size(), "batches for getting via types.");
    }
    // 1. redo min area handling
    MTStat allPostMaze2MT;
    for (const vector<int> &batch : batches) {
        runJobsMT(database, batch.size(), [&](int jobIdx) {
            int netIdx = batch[jobIdx];
            if (!db::isSucc(allNetStatus[netIdx])) return;
            UpdateDB::clearMinAreaRouteResult(database, database.nets[netIdx]);
        });
        allPostMaze2MT += runJobsMT(database, batch.size(), [&](int jobIdx) {
            int netIdx = batch[jobIdx];
            if (!db::isSucc(allNetStatus[netIdx])) return;
            PostMazeRoute(database.nets[netIdx]).run2(database);
        });
        runJobsMT(database, batch.size(), [&](int jobIdx) {
            int netIdx = batch[jobIdx];
            if (!db::isSucc(allNetStatus[netIdx])) return;
            UpdateDB::commitMinAreaRouteResult(database, database.nets[netIdx]);
        });
    }
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("allPostMaze2MT", allPostMaze2MT);
    }
    // 2. get via types again
    for (int iter = 0; iter < database.setting().multiNetSelectViaTypesIter; iter++) {
        MTStat allGetViaTypesMT, allCommitViaTypesMT;
        for (const vector<int> &batch : batches) {
            allGetViaTypesMT += runJobsMT(database, batch.size(), [&](int jobIdx) {
                int netIdx = batch[jobIdx];
                if (!db::isSucc(allNetStatus[netIdx])) return;
                PostRoute postRoute(database.nets[netIdx]);
                if (iter == 0) postRoute.considerViaViaVio = false;
                postRoute.getViaTypes(database);
            });
            allCommitViaTypesMT += runJobsMT(database, batch.size(), [&](int jobIdx) {
                int netIdx = batch[jobIdx];
                if (!db::isSucc(allNetStatus[netIdx])) return;
                UpdateDB::commitViaTypes(database, database.nets[netIdx]);
            });
        }
        if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            printlog("allGetViaTypesMT", allGetViaTypesMT);
            printlog("allCommitViaTypesMT", allCommitViaTypesMT);
        }
    }
    // 3. post route
    auto postMT = runJobsMT(database, database.nets.size(), [&](int netIdx) {
        if (!db::isSucc(allNetStatus[netIdx])) return;
        PostRoute postRoute(database.nets[netIdx]);
        postRoute.run(database);
    });
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("postMT", postMT);
    }
    // final open fix
    if (database.setting().fixOpenBySST) {
        int count = 0;
        for (auto &net : database.nets) {
            if (net.defWireSegments.empty() && net.numOfPins() > 1) {
                connectBySTT(database, net);
                count++;
            }
        }
        if (count > 0) log() << "#nets connected by STT: " << count << std::endl;
    }
}

void Router::unfinish(db::Database& database) {
    runJobsMT(database, database.nets.size(), [&](int netIdx) { database.nets[netIdx].clearPostRouteResult(); });
}

double Router::printStat(db::Database const& database, bool major) {
    double total_score = 0;
    log() << std::endl;
    log() << "----------------------------------------------------------------" << std::endl;
    database.routeStat().print();
    if (major) {
        total_score = database.printAllUsageAndVio(database);
    }
    log() << "----------------------------------------------------------------" << std::endl;
    log() << std::endl;
    return total_score;
}

int Router::step(db::Database& database) {
    if (iter < database.setting().rrrIterLimit) {
        if (database.setting().dbVerbose >= +db::VerboseLevelT::MIDDLE) {
            log() << std::endl;
            log() << "################################################################" << std::endl;
            log() << "Start RRR iteration " << iter << std::endl;
            log() << std::endl;
        }
        database.routeStat().clear();
        vector<int> netsToRoute = getNetsToRoute(database);
        if (netsToRoute.empty()) {
            if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
                log() << "No net is identified for this iteration of RRR." << std::endl;
                log() << std::endl;
            }
            return -1;
        }
        database.rrrIterSetting().update(database, iter);
        if (iter > 0) {
            // updateCost should before ripup, otherwise, violated nets have gone
            updateCost(database, netsToRoute);
//            ripup(database, netsToRoute);
        }
        ripup(database, netsToRoute);
        database.statHistCost(database);
        if (database.setting().rrrIterLimit > 1) {
            double step = (1.0 - database.setting().rrrInitVioCostDiscount) / (database.setting().rrrIterLimit - 1);
            database.setUnitVioCost(database, database.setting().rrrInitVioCostDiscount + step * iter);
        }
        if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            database.rrrIterSetting().print();
        }
        route(database, netsToRoute);
        if (database.setting().dbVerbose >= +db::VerboseLevelT::MIDDLE) {
            log() << std::endl;
            log() << "Finish RRR iteration " << iter << std::endl;
            log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
                  << std::endl;
        }
        if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            printStat(database, database.setting().rrrWriteEachIter);
        }
        if (database.setting().rrrWriteEachIter) {
            std::string fn = "iter" + std::to_string(iter) + "_" + database.setting().outputFile;
            printlog("Write result of RRR iter", iter, "to", fn, "...");
            finish(database);
            database.writeDEF(fn);
            unfinish(database);
        }
        iter++;
        return 0;
    } else {
        finish(database);
        if (database.setting().dbVerbose >= +db::VerboseLevelT::MIDDLE) {
            log() << std::endl;
            log() << "################################################################" << std::endl;
            log() << "Finish all RRR iterations and PostRoute" << std::endl;
            log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
                  << std::endl;
        }
        if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            printStat(database, true);
        }
        return 1;
    }
}

void Router::reset(db::Database& database) {
    for (auto &e: _feature) {
        e.assign(Router::Feature_idx::FEA_DIM, 0);
    }
    iter = 0;
    vector<int> all_nets;
    for (int i = 0; i < database.nets.size(); ++i)
        all_nets.emplace_back(i);
    ripup(database, all_nets);
}

void Router::init(db::Database& database) {
    allNetStatus.resize(database.nets.size(), db::RouteStatus::FAIL_UNPROCESSED);
    _feature.resize(database.nets.size());
    for (auto &e: _feature) {
        e.resize(Router::Feature_idx::FEA_DIM, 0);
    }
    iter = 0;
}

void Router::pre_route(db::Database& database, const vector<int> &netsToRoute) {
    // init SingleNetRouters
    _routers.clear();
    _routers.reserve(netsToRoute.size());
    for (int netIdx : netsToRoute) {
        _routers.emplace_back(database.nets[netIdx]);
    }

    // pre route
    auto preMT = runJobsMT(database, netsToRoute.size(), [&](int netIdx) { _routers[netIdx].preRoute(database); });
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("preMT", preMT);
        printStat(database);
    }
}

vector<vector<int>> Router::get_nets_feature(db::Database& database) {
    Scheduler scheduler(_routers);
    vector<int> degree = scheduler.get_net_degree(database);

    auto reset_mt = runJobsMT(database, _feature.size(), [&](int i) {
        _feature.at(i).at(ROUTED) = 0;
    });

    auto feature_mt = runJobsMT(database, _routers.size(), [&](int i) {
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

int Router::prepare(db::Database& database) {
    if (!iter)
        allNetStatus.resize(database.nets.size(), db::RouteStatus::FAIL_UNPROCESSED);
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << std::endl;
        log() << "################################################################" << std::endl;
        log() << "Start RRR iteration " << iter << std::endl;
        log() << std::endl;
    }
    database.routeStat().clear();
    _nets_to_route = getNetsToRoute(database);
    if (_nets_to_route.empty()) {
        if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
            log() << "No net is identified for this iteration of RRR." << std::endl;
            log() << std::endl;
        }
        return -1;
    }
    database.rrrIterSetting().update(database, iter);
    if (iter > 0) {
        // updateCost should before ripup, otherwise, violated nets have gone
        updateCost(database, _nets_to_route);
        _via_usage.clear();
        _wire_usage_length.clear();
        _layer_usage.clear();
        database.get_net_wire_vio_usage(_via_usage, _wire_usage_length, _layer_usage);
        ripup(database, _nets_to_route);
    }
    database.statHistCost(database);
    if (database.setting().rrrIterLimit > 1) {
        double step = (1.0 - database.setting().rrrInitVioCostDiscount) / (database.setting().rrrIterLimit - 1);
        database.setUnitVioCost(database, database.setting().rrrInitVioCostDiscount + step * iter);
    }
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        database.rrrIterSetting().print();
    }
    pre_route(database, _nets_to_route);
    // schedule
    return 0;
}

double Router::route(db::Database& database, vector<double> rank_score) {
    // schedule
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Start multi-thread scheduling. There are " << _nets_to_route.size() << " nets to route." << std::endl;
    }
    Scheduler scheduler(_routers);
    const vector<vector<int>> &batches = scheduler.schedule(database, std::move(rank_score));
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Finish multi-thread scheduling" << ((database.setting().numThreads == 0) ? " using simple mode" : "")
              << ". There will be " << batches.size() << " batches." << std::endl;
        log() << std::endl;
    }

    // maze route and commit DB by batch
    int iBatch = 0;
    MTStat allMazeMT, allCommitMT, allGetViaTypesMT, allCommitViaTypesMT;
    for (const vector<int> &batch : batches) {
        // 1 maze route
        auto mazeMT = runJobsMT(database, batch.size(), [&](int jobIdx) {
            auto &router = _routers[batch[jobIdx]];
            router.mazeRoute(database);
            allNetStatus[router.dbNet.idx] = router.status;
        });
        allMazeMT += mazeMT;
        // 2 commit nets to DB
        auto commitMT = runJobsMT(database, batch.size(), [&](int jobIdx) {
            auto &router = _routers[batch[jobIdx]];
            if (!db::isSucc(router.status)) return;
            router.commitNetToDB(database);
        });
        allCommitMT += commitMT;
        // 3 get via types
        allGetViaTypesMT += runJobsMT(database, batch.size(), [&](int jobIdx) {
            auto &router = _routers[batch[jobIdx]];
            if (!db::isSucc(router.status)) return;
            PostRoute postRoute(router.dbNet);
            postRoute.getViaTypes(database);
        });
        allCommitViaTypesMT += runJobsMT(database, batch.size(), [&](int jobIdx) {
            auto &router = _routers[batch[jobIdx]];
            if (!db::isSucc(router.status)) return;
            UpdateDB::commitViaTypes(database, router.dbNet);
        });
        // 4 stat
        if (database.setting().multiNetVerbose >= +db::VerboseLevelT::HIGH && database.setting().numThreads != 0) {
            int maxNumVertices = 0;
            for (int i : batch) maxNumVertices = std::max(maxNumVertices, _routers[i].localNet.estimatedNumOfVertices);
            log() << "Batch " << iBatch << " done: size=" << batch.size() << ", mazeMT " << mazeMT << ", commitMT "
                  << commitMT << ", peakM=" << utils::mem_use::get_peak() << ", maxV=" << maxNumVertices << std::endl;
        }
        iBatch++;
    }
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        printlog("allMazeMT", allMazeMT);
        printlog("allCommitMT", allCommitMT);
        printlog("allGetViaTypesMT", allGetViaTypesMT);
        printlog("allCommitViaTypesMT", allCommitViaTypesMT);
    }
    if (database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << std::endl;
        log() << "Finish RRR iteration " << iter << std::endl;
        log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
              << std::endl;
    }
    double total_score = 0;
    total_score = get_score(database);

    if (database.setting().rrrWriteEachIter) {
        std::string fn = "iter" + std::to_string(iter) + "_" + database.setting().outputFile;
        printlog("Write result of RRR iter", iter, "to", fn, "...");
        finish(database);
        database.writeDEF(fn);
        unfinish(database);
    }
    iter ++;
    return total_score;
}

double Router::get_score(db::Database& database) {
    return database.get_score(database);
}
