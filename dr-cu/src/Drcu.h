#pragma once
#include "db/Database.h"
#include "global.h"
#include "multi_net/Router.h"
#include "multi_net/Scheduler.h"

class Drcu {
public:
    void init(std::vector<std::string> const& argv);
    void reset();
    void test(std::vector<std::string> const& argv);
    struct Res{
        vector<vector<double>> feature;
        bool done = false;
        float reward = 0;
    };
    Res step(const vector<double>& action);
    vector<vector<double>> get_the_1st_observation();
    std::array<double, 4> get_all_vio() const;
private:
    std::vector<std::string> _long_format_argv = {"argv[0]",
                                        "-lef",
                                        "../dr-cu/toys/ispd18_sample/ispd18_sample.input.lef",
                                        "-def",
                                        "../dr-cu/toys/ispd18_sample/ispd18_sample.input.def",
                                        "-guide",
                                        "../dr-cu/toys/ispd18_sample/ispd18_sample.input.guide",
                                        "-output",
                                        "ispd18_sample.solution.def",
                                        "-threads",
                                        "8",
                                        "-tat",
                                        "2000000000"};
    boost::program_options::variables_map _vm;
    Router _router;
//    vector<int> _nets_to_route;
    vector<vector<int>> _features;
    vector<vector<double>> _features_norm;
    vector<double> _rank_score;
    int _argc{0};
    std::vector<std::string> _short_format_argv; 
    int _step_cnt{0};
    int irr_limit{4};
    db::Database _database; 

    int feed_argv(std::vector<std::string> const& short_format_argv);
    void convert_argv_format(std::vector<std::string> const& short_format_argv);
    void init_ispd_flow();
    void close();
    int prepare();
};
