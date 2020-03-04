#include "Drcu.h"

void Drcu::init(std::vector<std::string> const& argv) {
    _session.init();
    _argc = argv.size();
    _short_format_argv = argv; 
    feed_argv(_short_format_argv);
    _database.setting().makeItSilent();
    init_ispd_flow();
    _database.init(_session);
    _database.setting().adapt(_database);
    _router.init(_database);
    for (auto &net: _database.nets) {
        net.stash();
    }
    _database.stash();
    prepare();
    if (_database.nets.size() < 10000) {
        ++irr_limit;
    }
    else if (_database.nets.size() > 800000) {
        --irr_limit;
    }
}

void Drcu::reset() {
    _router.reset(_database);
    _step_cnt = 0;
    _rank_score.clear();
    _router.reset(_database);
    for (auto &net: _database.nets) {
        net.reset();
    }
    _database.reset();
    prepare();
}

int Drcu::feed_argv(std::vector<std::string> const& short_format_argv) {
    int argc = short_format_argv.size();
    char *argv[13];
    _long_format_argv[0] = short_format_argv.at(0);
    if (argc == 2) {
        for (int i = 0; i < 13; ++i) {
            // Yibo: need to copy from string 
            argv[i] = new char [_long_format_argv.at(i).size() + 1];
            strncpy(argv[i], _long_format_argv.at(i).c_str(), _long_format_argv.at(i).size());
            argv[i][_long_format_argv.at(i).size()] = '\0';
        }
    } else if (argc == 3) {
        convert_argv_format(short_format_argv);
        for (int i = 0; i < 13; ++i) {
            // Yibo: need to copy from string 
            argv[i] = new char [_long_format_argv.at(i).size() + 1];
            strncpy(argv[i], _long_format_argv.at(i).c_str(), _long_format_argv.at(i).size());
            argv[i][_long_format_argv.at(i).size()] = '\0';
        }
    } else if (argc == 13){
        for (int i = 0; i < 13; ++i) {
            // Yibo: need to copy from string 
            argv[i] = new char [short_format_argv.at(i).size() + 1];
            strncpy(argv[i], short_format_argv.at(i).c_str(), short_format_argv.at(i).size());
            argv[i][short_format_argv.at(i).size()] = '\0';
        }
    } else {
        for (int i = 0; i < 13; ++i) {
            argv[i] = nullptr; 
        }
    }

    if (_database.setting().dbVerbose >= +db::VerboseLevelT::HIGH) {
        printlog("------------------------------------------------------------------------------");
        printlog("                    ISPD 2019 - Detailed Routing Contest                      ");
        printlog("                             Team number : 15                                 ");
        printlog("                             Team name: Dr. CU                                ");
        printlog("        Members: Gengjie Chen, Haocheng Li, Bentian Jiang, Jingsong Chen,     ");
        printlog("                 Evangeline F.Y. Young                                        ");
        printlog("        Affiliation: The Chinese University of Hong Kong                      ");
        printlog("------------------------------------------------------------------------------");
    }
    std::cout << std::boolalpha;  // set std::boolalpha to std::cout

    int ret = 0; 
    try {
        using namespace boost::program_options;
        options_description desc{"Options"};
        // clang-format off
        desc.add_options()
                ("help", "Help message.")
                ("lef", value<std::string>()->required())
                ("def", value<std::string>()->required())
                ("guide", value<std::string>()->required())
                ("threads", value<int>()->required())
                ("tat", value<int>()->required())
                ("output", value<std::string>()->required())
                // optional
                ("multiNetVerbose", value<std::string>())
                ("multiNetScheduleSortAll", value<bool>())
                ("multiNetScheduleReverse", value<bool>())
                ("multiNetScheduleSort", value<bool>())
                ("rrrIters", value<int>())
                ("rrrWriteEachIter", value<bool>())
                ("rrrInitVioCostDiscount", value<double>())
                ("rrrFadeCoeff", value<double>())
                ("defaultGuideExpand", value<int>())
                ("diffLayerGuideVioThres", value<int>())
                ("wrongWayPointDensity", value<double>())
                ("wrongWayPenaltyCoeff", value<double>())
                ("fixOpenBySST", value<bool>())
                ("dbVerbose", value<std::string>())
                ("dbUsePoorViaMapThres", value<int>())
                ("dbPoorWirePenaltyCoeff", value<double>())
                ("dbPoorViaPenaltyCoeff", value<double>())
                ("dbNondefaultViaPenaltyCoeff", value<double>())
                ("dbInitHistUsageForPinAccess", value<double>());
        // clang-format on
        store(command_line_parser(13, argv)
                  .options(desc)
                  .style(command_line_style::unix_style | command_line_style::allow_long_disguise)
                  .run(),
              _vm);
        notify(_vm);

        if (_vm.count("help")) {
            std::cout << desc << std::endl;
            ret = 0; 
        } else {
            for (const auto &option : desc.options()) {
                if (_database.setting().dbVerbose >= +db::VerboseLevelT::MIDDLE) {
                    if (_vm.count(option->long_name())) {
                        std::string name = option->description().empty() ? option->long_name() : option->description();
                        log() << std::left << std::setw(18) << name << ": ";
                        const auto &value = _vm.at(option->long_name()).value();
                        if (auto v = boost::any_cast<double>(&value)) {
                            std::cout << *v;
                        } else if (auto v = boost::any_cast<int>(&value)) {
                            std::cout << *v;
                        } else if (auto v = boost::any_cast<std::string>(&value)) {
                            std::cout << *v;
                        } else if (auto v = boost::any_cast<bool>(&value)) {
                            std::cout << *v;
                        } else {
                            std::cout << "unresolved type";
                        }
                        std::cout << std::endl;
                    }
                }
            }
        }
    } catch (const boost::program_options::error &e) {
        printlog(e.what());
        ret = 1; 
    }

    // Yibo: recycle argv 
    for (int i = 0; i < 13; ++i) {
        if (argv[i]) {
            delete [] argv[i];
        }
    }

    return ret;
}

void Drcu::convert_argv_format(std::vector<std::string> const& short_format_argv) {
    std::string year = short_format_argv.at(1);
    std::string test_id = short_format_argv.at(2);
    std::string input_file_name =
        "../dr-cu/toys/ispd" + year + "_test" + test_id + "/ispd" + year + "_test" + test_id + ".input";

    _long_format_argv.at(0) = short_format_argv.at(0);
    _long_format_argv.at(1) = "-lef";
    _long_format_argv.at(2) = input_file_name + ".lef";
    _long_format_argv.at(3) = "-def";
    _long_format_argv.at(4) = input_file_name + ".def";
    _long_format_argv.at(5) = "-guide";
    _long_format_argv.at(6) = input_file_name + ".guide";
    _long_format_argv.at(7) = "-output";
    _long_format_argv.at(8) = "ispd" + year + "_test" + test_id + ".solution.def";
    _long_format_argv.at(9) = "-threads";
    _long_format_argv.at(10) = "8";
    _long_format_argv.at(11) = "-tat";
    _long_format_argv.at(12) = "2000000000";
}

void Drcu::init_ispd_flow() {
    //    _database.setting().makeItSilent();

    // Parse options
    // required
    std::string lefFile = _vm.at("lef").as<std::string>();
    std::string defFile = _vm.at("def").as<std::string>();
    std::string guideFile = _vm.at("guide").as<std::string>();
    _database.setting().numThreads = _vm.at("threads").as<int>();
    _database.setting().tat = _vm.at("tat").as<int>();
    _database.setting().outputFile = _vm.at("output").as<std::string>();
    // optional
    // multi_net
    if (_vm.count("multiNetVerbose")) {
        _database.setting().multiNetVerbose =
            db::VerboseLevelT::_from_string(_vm.at("multiNetVerbose").as<std::string>().c_str());
    }
    if (_vm.count("multiNetScheduleSortAll")) {
        _database.setting().multiNetScheduleSortAll = _vm.at("multiNetScheduleSortAll").as<bool>();
    }
    if (_vm.count("multiNetScheduleReverse")) {
        _database.setting().multiNetScheduleReverse = _vm.at("multiNetScheduleReverse").as<bool>();
    }
    if (_vm.count("multiNetScheduleSort")) {
        _database.setting().multiNetScheduleSort = _vm.at("multiNetScheduleSort").as<bool>();
    }
    if (_vm.count("rrrIters")) {
        _database.setting().rrrIterLimit = _vm.at("rrrIters").as<int>();
    }
    if (_vm.count("rrrWriteEachIter")) {
        _database.setting().rrrWriteEachIter = _vm.at("rrrWriteEachIter").as<bool>();
    }
    if (_vm.count("rrrInitVioCostDiscount")) {
        _database.setting().rrrInitVioCostDiscount = _vm.at("rrrInitVioCostDiscount").as<double>();
    }
    if (_vm.count("rrrFadeCoeff")) {
        _database.setting().rrrFadeCoeff = _vm.at("rrrFadeCoeff").as<double>();
    }
    // single_net
    if (_vm.count("defaultGuideExpand")) {
        _database.setting().defaultGuideExpand = _vm.at("defaultGuideExpand").as<int>();
    }
    if (_vm.count("diffLayerGuideVioThres")) {
        _database.setting().diffLayerGuideVioThres = _vm.at("diffLayerGuideVioThres").as<int>();
    }
    if (_vm.count("wrongWayPointDensity")) {
        _database.setting().wrongWayPointDensity = _vm.at("wrongWayPointDensity").as<double>();
    }
    if (_vm.count("wrongWayPenaltyCoeff")) {
        _database.setting().wrongWayPenaltyCoeff = _vm.at("wrongWayPenaltyCoeff").as<double>();
    }
    if (_vm.count("fixOpenBySST")) {
        _database.setting().fixOpenBySST = _vm.at("fixOpenBySST").as<bool>();
    }
    // db
    if (_vm.count("dbVerbose")) {
        _database.setting().dbVerbose = db::VerboseLevelT::_from_string(_vm.at("dbVerbose").as<std::string>().c_str());
    }
    if (_vm.count("dbUsePoorViaMapThres")) {
        _database.setting().dbUsePoorViaMapThres = _vm.at("dbUsePoorViaMapThres").as<int>();
    }
    if (_vm.count("dbPoorWirePenaltyCoeff")) {
        _database.setting().dbPoorWirePenaltyCoeff = _vm.at("dbPoorWirePenaltyCoeff").as<double>();
    }
    if (_vm.count("dbPoorViaPenaltyCoeff")) {
        _database.setting().dbPoorViaPenaltyCoeff = _vm.at("dbPoorViaPenaltyCoeff").as<double>();
    }
    if (_vm.count("dbNondefaultViaPenaltyCoeff")) {
        _database.setting().dbNondefaultViaPenaltyCoeff = _vm.at("dbNondefaultViaPenaltyCoeff").as<double>();
    }
    if (_vm.count("dbInitHistUsageForPinAccess")) {
        _database.setting().dbInitHistUsageForPinAccess = _vm.at("dbInitHistUsageForPinAccess").as<double>();
    }

    // Read benchmarks
    Rsyn::ISPD2018Reader reader;
    const Rsyn::Json params = {
        {"lefFile", lefFile},
        {"defFile", defFile},
        {"guideFile", guideFile},
    };

    if (_database.setting().dbVerbose >= +db::VerboseLevelT::HIGH) {
        log() << std::endl;
        log() << "################################################################" << std::endl;
        log() << "Start reading benchmarks" << std::endl;
    }
    reader.load(&_session, params);
    if (_database.setting().dbVerbose >= +db::VerboseLevelT::HIGH) {
        log() << "Finish reading benchmarks" << std::endl;
        log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
              << std::endl;
        log() << std::endl;
    }
}

void Drcu::test(std::vector<std::string> const& argv) {
    float action{0};
    Res res;
    init(argv);

    res.feature = get_the_1st_observation();

    while (!res.done) {
        res = step(res.feature.at(0));
    }
    //    route();

    reset();
}

void Drcu::close() {
    // _database.writeNetTopo(_database.setting().outputFile + ".topo");
    _database.clear();
    // _database.writeDEF(_database.setting().outputFile);
    if (_database.setting().multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
        log() << "Finish writing def" << std::endl;
        log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
              << std::endl;
        log() << std::endl;

        printlog("---------------------------------------------------------------------------");
        printlog("                               Terminated...                               ");
        printlog("---------------------------------------------------------------------------");
    }
}

int Drcu::prepare() {
    int res = 0;
    res = _router.prepare(_database);
    if (res) return res;

    _features = _router.get_nets_feature(_database);
    _features_norm.resize(_features.size());
    for (int i = 0; i < _features.size(); ++i) {
        _features_norm.at(i).resize(Router::Feature_idx::FEA_DIM);
    }

    auto feature_mt = runJobsMT(_database, Router::Feature_idx::FEA_DIM, [&](int fea_id) {
        int min{_features.at(0).at(fea_id)};
        int max{_features.at(0).at(fea_id)};

        for (int i = 1; i < _features.size(); ++i) {
            min = std::min(min, _features.at(i).at(fea_id));
            max = std::max(max, _features.at(i).at(fea_id));
        }

        for (int i = 0; i < _features.size(); ++i) {
            if (min != max) {
                double norm = static_cast<double>(_features.at(i).at(fea_id) - min ) / static_cast<double>(max - min);
                _features_norm.at(i).at(fea_id) = norm;
            }
            else {
                _features_norm.at(i).at(fea_id) = 0;
            }
        }

    });

    return 0;
}

Drcu::Res Drcu::step(const vector<double>& action) {
    Res res;
    vector<double> rank_score;
    for (int i = 0; i < _features.size(); ++i) {
        if (_features.at(i).at(0) == 1) {
            rank_score.emplace_back(action.at(i));
        }
    }
    res.reward = - _router.route(_database, rank_score) + 366.5;
    // res.reward /= 1000.0;
    if (_step_cnt < irr_limit) {
        _step_cnt++;
        if (prepare()) {
            res.done = true;
        } else {
            res.done = false;
            res.feature = _features_norm;
        }

    } else {
        res.done = true;
    }
    return res;
}

vector<vector<double>> Drcu::get_the_1st_observation() {
    _step_cnt++;
    return vector<vector<double>>(_features_norm);
}

std::array<double, 4> Drcu::get_all_vio() const {
    auto ret = std::array<double, 4>(_database.get_all_vio());
    //for (auto v : ret) {
    //    std::cout << v << " ";
    //}
    //std::cout << "\n";
    return ret; 
}
