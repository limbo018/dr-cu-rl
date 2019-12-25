#pragma once
#include "db/Database.h"
#include "global.h"
#include "multi_net/Router.h"
#include "multi_net/Scheduler.h"

class Drcu {
public:
    void init(int argc, char* short_format_argv[]);
    void reset();
    void test(int argc, char* short_format_argv[]);
    struct Res{
        vector<vector<float>> feature;
        bool done = false;
        float reward = 0;
    };
    Res step(const vector<float>& action);
    vector<vector<float>> get_the_1st_observation();
private:
    std::string _long_format_argv[13] = {"argv[0]",
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
    vector<vector<float>> _features_norm;
    vector<float> _rank_score;
    int _argc{0};
    char** _short_format_argv{nullptr};
    int _step_cnt{0};
    const int IRR_LIMIT{4};


    int feed_argv(int argc, char* short_format_argv[]);
    void convert_argv_format(char* short_format_argv[]);
    void init_ispd_flow();
    void close();
    int prepare();
};
