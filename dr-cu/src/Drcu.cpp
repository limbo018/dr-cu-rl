#include "Drcu.h"

void Drcu::init(int argc, char *short_format_argv[]) {
    Rsyn::Session::init();
    _argc = argc;
    _short_format_argv = short_format_argv;
    feed_argv(_argc, _short_format_argv);
    db::setting.makeItSilent();
    init_ispd_flow();
    database.init();
    db::setting.adapt();
    _router.init();
    prepare();
}

void Drcu::reset() {
    close();
    Rsyn::Session::checkInitialized(false, true);
    _router.reset();
    _step_cnt = 0;
    _rank_score.clear();
    init(_argc, _short_format_argv);
}

int Drcu::feed_argv(int argc, char *short_format_argv[]) {
    _long_format_argv[0] = short_format_argv[0];
    if (argc > 2) convert_argv_format(short_format_argv);

    char *argv[13];
    for (int i = 0; i < 13; ++i) {
        argv[i] = const_cast<char *>(_long_format_argv[i].c_str());
    }
    if (db::setting.dbVerbose >= +db::VerboseLevelT::HIGH) {
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
            return 0;
        }
        for (const auto &option : desc.options()) {
            if (db::setting.dbVerbose >= +db::VerboseLevelT::MIDDLE) {
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
    } catch (const boost::program_options::error &e) {
        printlog(e.what());
        return 1;
    }
    return 0;
}

void Drcu::convert_argv_format(char *short_format_argv[]) {
    std::string year = short_format_argv[1];
    std::string test_id = short_format_argv[2];
    std::string input_file_name =
        "../dr-cu/toys/ispd" + year + "_test" + test_id + "/ispd" + year + "_test" + test_id + ".input";

    _long_format_argv[0] = short_format_argv[0];
    _long_format_argv[1] = "-lef";
    _long_format_argv[2] = input_file_name + ".lef";
    _long_format_argv[3] = "-def";
    _long_format_argv[4] = input_file_name + ".def";
    _long_format_argv[5] = "-guide";
    _long_format_argv[6] = input_file_name + ".guide";
    _long_format_argv[7] = "-output";
    _long_format_argv[8] = "ispd" + year + "_test" + test_id + ".solution.def";
    _long_format_argv[9] = "-threads";
    _long_format_argv[10] = "8";
    _long_format_argv[11] = "-tat";
    _long_format_argv[12] = "2000000000";
}

void Drcu::init_ispd_flow() {
    //    db::setting.makeItSilent();

    Rsyn::Session session;

    // Parse options
    // required
    std::string lefFile = _vm.at("lef").as<std::string>();
    std::string defFile = _vm.at("def").as<std::string>();
    std::string guideFile = _vm.at("guide").as<std::string>();
    db::setting.numThreads = _vm.at("threads").as<int>();
    db::setting.tat = _vm.at("tat").as<int>();
    db::setting.outputFile = _vm.at("output").as<std::string>();
    // optional
    // multi_net
    if (_vm.count("multiNetVerbose")) {
        db::setting.multiNetVerbose =
            db::VerboseLevelT::_from_string(_vm.at("multiNetVerbose").as<std::string>().c_str());
    }
    if (_vm.count("multiNetScheduleSortAll")) {
        db::setting.multiNetScheduleSortAll = _vm.at("multiNetScheduleSortAll").as<bool>();
    }
    if (_vm.count("multiNetScheduleReverse")) {
        db::setting.multiNetScheduleReverse = _vm.at("multiNetScheduleReverse").as<bool>();
    }
    if (_vm.count("multiNetScheduleSort")) {
        db::setting.multiNetScheduleSort = _vm.at("multiNetScheduleSort").as<bool>();
    }
    if (_vm.count("rrrIters")) {
        db::setting.rrrIterLimit = _vm.at("rrrIters").as<int>();
    }
    if (_vm.count("rrrWriteEachIter")) {
        db::setting.rrrWriteEachIter = _vm.at("rrrWriteEachIter").as<bool>();
    }
    if (_vm.count("rrrInitVioCostDiscount")) {
        db::setting.rrrInitVioCostDiscount = _vm.at("rrrInitVioCostDiscount").as<double>();
    }
    if (_vm.count("rrrFadeCoeff")) {
        db::setting.rrrFadeCoeff = _vm.at("rrrFadeCoeff").as<double>();
    }
    // single_net
    if (_vm.count("defaultGuideExpand")) {
        db::setting.defaultGuideExpand = _vm.at("defaultGuideExpand").as<int>();
    }
    if (_vm.count("diffLayerGuideVioThres")) {
        db::setting.diffLayerGuideVioThres = _vm.at("diffLayerGuideVioThres").as<int>();
    }
    if (_vm.count("wrongWayPointDensity")) {
        db::setting.wrongWayPointDensity = _vm.at("wrongWayPointDensity").as<double>();
    }
    if (_vm.count("wrongWayPenaltyCoeff")) {
        db::setting.wrongWayPenaltyCoeff = _vm.at("wrongWayPenaltyCoeff").as<double>();
    }
    if (_vm.count("fixOpenBySST")) {
        db::setting.fixOpenBySST = _vm.at("fixOpenBySST").as<bool>();
    }
    // db
    if (_vm.count("dbVerbose")) {
        db::setting.dbVerbose = db::VerboseLevelT::_from_string(_vm.at("dbVerbose").as<std::string>().c_str());
    }
    if (_vm.count("dbUsePoorViaMapThres")) {
        db::setting.dbUsePoorViaMapThres = _vm.at("dbUsePoorViaMapThres").as<int>();
    }
    if (_vm.count("dbPoorWirePenaltyCoeff")) {
        db::setting.dbPoorWirePenaltyCoeff = _vm.at("dbPoorWirePenaltyCoeff").as<double>();
    }
    if (_vm.count("dbPoorViaPenaltyCoeff")) {
        db::setting.dbPoorViaPenaltyCoeff = _vm.at("dbPoorViaPenaltyCoeff").as<double>();
    }
    if (_vm.count("dbNondefaultViaPenaltyCoeff")) {
        db::setting.dbNondefaultViaPenaltyCoeff = _vm.at("dbNondefaultViaPenaltyCoeff").as<double>();
    }
    if (_vm.count("dbInitHistUsageForPinAccess")) {
        db::setting.dbInitHistUsageForPinAccess = _vm.at("dbInitHistUsageForPinAccess").as<double>();
    }

    // Read benchmarks
    Rsyn::ISPD2018Reader reader;
    const Rsyn::Json params = {
        {"lefFile", lefFile},
        {"defFile", defFile},
        {"guideFile", guideFile},
    };

    if (db::setting.dbVerbose >= +db::VerboseLevelT::HIGH) {
        log() << std::endl;
        log() << "################################################################" << std::endl;
        log() << "Start reading benchmarks" << std::endl;
    }
    reader.load(params);
    if (db::setting.dbVerbose >= +db::VerboseLevelT::HIGH) {
        log() << "Finish reading benchmarks" << std::endl;
        log() << "MEM: cur=" << utils::mem_use::get_current() << "MB, peak=" << utils::mem_use::get_peak() << "MB"
              << std::endl;
        log() << std::endl;
    }
}

void Drcu::test(int argc, char *short_format_argv[]) {
    float action{0};
    Res res;
    init(argc, short_format_argv);

    res.feature = get_the_1st_observation();

    while (!res.done) {
        res = step(res.feature.at(0));
    }
    //    route();

    reset();
}

void Drcu::close() {
    // database.writeNetTopo(db::setting.outputFile + ".topo");
    database.clear();
    // database.writeDEF(db::setting.outputFile);
    if (db::setting.multiNetVerbose >= +db::VerboseLevelT::MIDDLE) {
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
    res = _router.prepare();
    if (res) return res;

    _features = _router.get_nets_feature();

    std::array<int, 2> min{_features.at(0).at(0), _features.at(0).at(1)};
    std::array<int, 2> max{_features.at(0).at(0), _features.at(0).at(1)};
    for (auto feature : _features) {
        for (int i = 0; i < 2; ++i) {
            if (max.at(i) < feature.at(i)) max.at(i) = feature.at(i);
            if (min.at(i) > feature.at(i)) min.at(i) = feature.at(i);
        }
    }
    _features_norm.resize(_features.size());
    for (int i = 0; i < _features.size(); ++i) {
        _features_norm.at(i).resize(2);
        for (int j = 0; j < 2; ++j) {
            float val = _features.at(i).at(j);
            val = (val - min.at(j)) / static_cast<float>(max.at(j) - min.at(j));
            val = val * 2 - 1;
            _features_norm.at(i).at(j) = val;
        }
    }

    return 0;
}

Drcu::Res Drcu::step(const vector<float>& rank_score) {
    Res res;
//    vector<float> size;
//    for (auto e: _features_norm) {
//        size.emplace_back(e.at(0));
//    }
    if (_step_cnt < IRR_LIMIT) {
        res.reward = - _router.route(rank_score);
        _step_cnt++;
        if (prepare()) {
            res.done = true;
        } else {
            res.done = false;
            res.feature = _features_norm;
        }

    } else {
        res.reward = - _router.route(rank_score);
        res.done = true;
    }
    return res;
}

vector<vector<float>> Drcu::get_the_1st_observation() {
    _step_cnt++;
    return vector<vector<float>>(_features_norm);
}