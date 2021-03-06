#include <cstring>
#include <chrono>
#include <fstream>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/fmt/bundled/ostream.h>

#include <ATen/Parallel.h>

#include <cpprl/cpprl.h>

#include "Envs.h"

using namespace cpprl;

// Algorithm hyperparameters
const std::string algorithm = "A2C";
const float actor_loss_coef = 1.0;
const int batch_size = 5;
const float clip_param = 0.2;
const float discount_factor = 0.99;
const float entropy_coef = 1e-3;
const float gae = 0.9;
const float kl_target = 0.5;
const float learning_rate = 1e-4;
const int log_interval = 5;
const int num_updates = 1e+4;
const int num_epoch = 3;
const int num_mini_batch = 20;
const float reward_clip_value = 100;  // Post scaling
const bool use_gae = true;
const bool use_lr_decay = false;
const float value_loss_coef = 0.5;

// Environment hyperparameters
const int num_envs = 1;

// Model hyperparameters
const int hidden_size = 64;
const bool recurrent = false;
const std::string model_name_prefix = "se";
const bool save_model = false;
const bool load_model = false;

struct InfoResponse {
    std::string action_space_type;
    std::vector<int64_t> action_space_shape;
    std::string observation_space_type;
    std::vector<int64_t> observation_space_shape;
};

std::vector<float> flatten_vector(std::vector<float> const &input) { return input; }
std::vector<double> flatten_vector(std::vector<double> const &input) { return input; }

template <typename T>
std::vector<double> flatten_vector(std::vector<std::vector<T>> const &input) {
    std::vector<double> output;

    for (auto const &element : input) {
        auto sub_vector = flatten_vector(element);

        output.reserve(output.size() + sub_vector.size());
        output.insert(output.end(), sub_vector.cbegin(), sub_vector.cend());
    }

    return output;
}

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("%^[%T %7l] %v%$");

    at::set_num_threads(8);
    torch::manual_seed(0);

    torch::Device device = torch::cuda::is_available() ? torch::kCUDA : torch::kCPU;

    // environment information
    std::unique_ptr<InfoResponse> env_info = std::make_unique<InfoResponse>();
    env_info->action_space_type = "Box";
    env_info->observation_space_type = "Box";

    // reset
    spdlog::info("Resetting environment");
    Envs envs;
    Envs::Res res;
    res = envs.init(argc, argv);
    int net_num = res.feature.at(0).size();
    env_info->action_space_shape.emplace_back(1);
    env_info->observation_space_shape = {net_num, Router::Feature_idx::FEA_DIM};
    spdlog::info("Net num: {}, Feature dim: {}", net_num, Router::Feature_idx::FEA_DIM);
    spdlog::info("Action space: {} - [{}]", env_info->action_space_type, env_info->action_space_shape);
    spdlog::info("Observation space: {} - [{}]", env_info->observation_space_type, env_info->observation_space_shape);

    // observation
    auto observation_shape = env_info->observation_space_shape;
    observation_shape.insert(observation_shape.begin(), num_envs);
    torch::Tensor observation;
    std::vector<double> observation_vec;
    if (env_info->observation_space_shape.size() > 1) {
        observation_vec = flatten_vector(res.feature);
        observation = torch::from_blob(observation_vec.data(), observation_shape, torch::kDouble).to(device);
    } else {
        observation_vec = flatten_vector(res.feature);
        observation = torch::from_blob(observation_vec.data(), observation_shape, torch::kDouble).to(device);
    }

    // model
    std::shared_ptr<NNBase> base;
    base = std::make_shared<MlpBase>(env_info->observation_space_shape[1], recurrent, hidden_size);
    if (load_model) {
        auto file_name = "base_" + model_name_prefix + ".pt";
        std::ifstream fin(file_name);
        if (fin) {
            spdlog::info("loading base model");
            torch::load(base, fin);
        }
    }
    base->to(device);
    ActionSpace space{env_info->action_space_type, env_info->action_space_shape};
    Policy policy(nullptr);
    if (env_info->observation_space_shape.size() == 1) {
        // With observation normalization
        policy = Policy(space, base, false);
    } else {
        // Without observation normalization
        policy = Policy(space, base, false);
    }
    if (load_model) {
        auto file_name = "policy_" + model_name_prefix + ".pt";
        std::ifstream fin(file_name);
        if (fin) {
            spdlog::info("loading policy model");
            torch::load(policy, fin);
        }
    };
    policy->to(device);

    RolloutStorage storage(
        batch_size, num_envs, env_info->observation_space_shape, space, hidden_size, device, net_num);
    std::unique_ptr<Algorithm> algo;
    if (algorithm == "A2C") {
        algo = std::make_unique<A2C>(policy, actor_loss_coef, value_loss_coef, entropy_coef, learning_rate);
    } else if (algorithm == "PPO") {
        algo = std::make_unique<PPO>(policy,
                                     clip_param,
                                     num_epoch,
                                     num_mini_batch,
                                     actor_loss_coef,
                                     value_loss_coef,
                                     entropy_coef,
                                     learning_rate,
                                     1e-8,
                                     0.5,
                                     kl_target);
    }
    storage.set_first_observation(observation);

    std::vector<float> running_rewards(num_envs);
    int episode_count = 0;

    float reward_to_print = 0;
    RunningMeanStd returns_rms(1);
    auto returns = torch::zeros({num_envs});
    std::array<double, 4> vios{0, 0, 0, 0};

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int update = 0; update < num_updates; ++update) {
        auto batch_start_time = std::chrono::high_resolution_clock::now();
        for (int step = 0; step < batch_size; ++step) {
            std::vector<torch::Tensor> act_result;
            {
                torch::NoGradGuard no_grad;
                act_result = policy->act(
                    storage.get_observations()[step], storage.get_hidden_states()[step], storage.get_masks()[step]);
            }
            auto actions_tensor = act_result[1].cpu().to(torch::kDouble);
            double *actions_array = actions_tensor.data_ptr<double>();
            std::vector<std::vector<double>> actions(num_envs);
            for (int i = 0; i < num_envs; ++i) {
                if (space.type == "Discrete") {
                    actions[i] = {actions_array[i]};
                } else {
                    for (int j = 0; j < net_num; j++) {
                        actions[i].push_back(actions_array[i * env_info->action_space_shape[0] + j]);
                    }
                }
            }
            // step
            auto step_start_time = std::chrono::high_resolution_clock::now();
            res = envs.step(actions);
            auto step_run_time = std::chrono::high_resolution_clock::now() - step_start_time;
            spdlog::debug("take a step, took {:03.2f}s",
                          std::chrono::duration_cast<std::chrono::milliseconds>(step_run_time).count() / 1000.0);
            if (res.done.at(0)) {
                vios = envs.get_all_vio();
                spdlog::info("total score: {}, vios[{}, {}, {}, {}]", res.reward, vios[0], vios[1], vios[2], vios[3]);
                auto reset_start_time = std::chrono::high_resolution_clock::now();
                auto reset_res = envs.reset();
                res.feature = reset_res.feature;
                auto reset_run_time = std::chrono::high_resolution_clock::now() - reset_start_time;
                spdlog::debug("reset, took {:03.2f}s",
                              std::chrono::duration_cast<std::chrono::milliseconds>(reset_run_time).count() / 1000.0);
            }

            std::vector<float> rewards;
            std::vector<float> real_rewards;
            std::vector<std::vector<bool>> dones_vec{num_envs};
            if (env_info->observation_space_shape.size() > 1) {
                observation_vec = flatten_vector(res.feature);
                observation = torch::from_blob(observation_vec.data(), observation_shape, torch::kDouble).to(device);
                auto raw_reward_vec = flatten_vector(res.reward);
                auto reward_tensor = torch::from_blob(raw_reward_vec.data(), {num_envs}, torch::kFloat);
                returns = returns * discount_factor + reward_tensor;
                returns_rms->update(returns);
                rewards = std::vector<float>(reward_tensor.data_ptr<float>(),
                                             reward_tensor.data_ptr<float>() + reward_tensor.numel());
                real_rewards = flatten_vector(res.reward);
                for (int i = 0; i < num_envs; i++) {
                    dones_vec[i].push_back(res.done.at(i));
                }

            } else {
                observation_vec = flatten_vector(res.feature);
                observation = torch::from_blob(observation_vec.data(), observation_shape, torch::kDouble).to(device);
                auto raw_reward_vec = flatten_vector(res.reward);
                auto reward_tensor = torch::from_blob(raw_reward_vec.data(), {num_envs}, torch::kFloat);
                returns = returns * discount_factor + reward_tensor;
                returns_rms->update(returns);
                rewards = std::vector<float>(reward_tensor.data_ptr<float>(),
                                             reward_tensor.data_ptr<float>() + reward_tensor.numel());
                real_rewards = flatten_vector(res.reward);
                for (int i = 0; i < num_envs; i++) {
                    dones_vec[i].push_back(res.done.at(i));
                }
            }
            for (int i = 0; i < num_envs; ++i) {
                running_rewards[i] += real_rewards[i];
                if (dones_vec[i][0]) {
                    reward_to_print = real_rewards[i];
                    vios = envs.get_all_vio();
                    running_rewards[i] = 0;
                    returns[i] = 0;
                    episode_count++;
                }
            }
            auto dones = torch::zeros({num_envs, 1}, TensorOptions(device));
            for (int i = 0; i < num_envs; ++i) {
                dones[i][0] = static_cast<int>(dones_vec[i][0]);
            }

            storage.insert(observation,
                           act_result[3],
                           act_result[1],
                           act_result[2],
                           act_result[0],
                           torch::from_blob(rewards.data(), {num_envs, 1}).to(device),
                           1 - dones);
        }

        auto batch_run_time = std::chrono::high_resolution_clock::now() - batch_start_time;
        auto total_run_time = std::chrono::high_resolution_clock::now() - start_time;
        torch::Tensor next_value;
        {
            torch::NoGradGuard no_grad;
            next_value =
                policy
                    ->get_values(
                        storage.get_observations()[-1], storage.get_hidden_states()[-1], storage.get_masks()[-1])
                    .detach();
        }
        storage.compute_returns(next_value, use_gae, discount_factor, gae);

        float decay_level;
        if (use_lr_decay) {
            decay_level = 1. - static_cast<float>(update) / num_updates;
        } else {
            decay_level = 1;
        }
        auto update_data = algo->update(storage, decay_level);
        storage.after_update();

        spdlog::info("{}s: update: {}, runtime: {:03.2f}s, vios: [{}, {}, {}, {}], reward: {}",
                     std::chrono::duration_cast<std::chrono::seconds>(total_run_time).count(),
                     update,
                     std::chrono::duration_cast<std::chrono::milliseconds>(batch_run_time).count() / 1000.0,
                     vios.at(0),
                     vios.at(1),
                     vios.at(2),
                     vios.at(3),
                     reward_to_print);
        if (update % log_interval == 0 && update > 0) {
            //            for (const auto &datum : update_data) {
            //                spdlog::info("{}: {}", datum.name, datum.value);
            //            }
            if (save_model) {
                torch::save(base, "base_" + model_name_prefix + ".pt");
                torch::save(policy, "policy_" + model_name_prefix + ".pt");
            }
        }
    }
}
