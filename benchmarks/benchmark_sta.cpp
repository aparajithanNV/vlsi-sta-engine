//
// Created by aparajithannv on 08/10/26.
//

#include <fstream>
#include <benchmark/benchmark.h>
#include "sta/TimingAnalyzer.h"
#include "sta/BenchParser.h"

static void BENCHMARK_STA_SEQUENTIAL(benchmark::State& state, const std::string& circuit)
{
    TimingGraph timing_graph;
    BenchParser parser;
    STAState sta_state;
    // std::ifstream file("../../iscas85/c7552.bench");
    std::string circuit_path = "../../iscas85/" + circuit + ".bench";
    std::ifstream file(circuit_path);

    if (!file.is_open())
    {
        state.SkipWithError("Could not open file");
        return;
    }

    parser.parse_bench(file, timing_graph);
    timing_graph.populate_pin_neighbors();
    auto success = timing_graph.find_topological_levels();
    // std::cout << circuit << " -> Pins: " << timing_graph.get_number_of_pins() << ", Topo Levels: " << timing_graph.get_number_of_topological_levels() << "\n";

    sta_state.circuit_RAT = 5.5;
    auto num_pins = timing_graph.get_number_of_pins();
    sta_state.actual_arrival_times = std::vector<float>(num_pins, 0.0);
    sta_state.required_arrival_times = std::vector<float>(num_pins, 0.0);
    sta_state.slack = std::vector<float>(num_pins, 0.0);

    for (auto _ : state)
    {
        calculate_slack_full_pass_late(timing_graph, sta_state);
        benchmark::DoNotOptimize(sta_state);
    }
}

// static void BENCHMARK_STA_SEQUENTIAL_C7552(benchmark::State& state)
// {
//     TimingGraph timing_graph;
//     BenchParser parser;
//     STAState sta_state;
//     std::ifstream file("../../iscas85/c7552.bench");
//
//     if (!file.is_open())
//     {
//         state.SkipWithError("Could not open file");
//         return;
//     }
//
//     parser.parse_bench(file, timing_graph);
//     timing_graph.populate_pin_neighbors();
//     auto success = timing_graph.find_topological_levels();
//
//     sta_state.circuit_RAT = 5.5;
//     auto num_pins = timing_graph.get_number_of_pins();
//     sta_state.actual_arrival_times = std::vector<float>(num_pins, 0.0);
//     sta_state.required_arrival_times = std::vector<float>(num_pins, 0.0);
//     sta_state.slack = std::vector<float>(num_pins, 0.0);
//
//     for (auto _ : state)
//     {
//         calculate_slack_full_pass_late(timing_graph, sta_state);
//         benchmark::DoNotOptimize(sta_state);
//     }
// }

static void BENCHMARK_PARALLEL_STA_C7552(benchmark::State& state)
{
    TimingGraph timing_graph;
    BenchParser parser;
    STAState sta_state;
    std::ifstream file("../../iscas85/c7552.bench");

    if (!file.is_open())
    {
        state.SkipWithError("Could not open file");
        return;
    }

    parser.parse_bench(file, timing_graph);
    timing_graph.populate_pin_neighbors();
    auto success = timing_graph.find_topological_levels();

    sta_state.circuit_RAT = 5.5;
    auto num_pins = timing_graph.get_number_of_pins();
    sta_state.actual_arrival_times = std::vector<float>(num_pins, 0.0);
    sta_state.required_arrival_times = std::vector<float>(num_pins, 0.0);
    sta_state.slack = std::vector<float>(num_pins, 0.0);

    tf::Taskflow task_graph;
    auto mode = Analysis::Late;
    auto AAT_terminal_task = build_AAT_tasks(task_graph, timing_graph, sta_state, mode);
    auto RAT_terminal_task = build_RAT_tasks(task_graph, timing_graph, sta_state, mode);
    auto slack_task = build_slack_tasks(task_graph, timing_graph, sta_state, mode);
    AAT_terminal_task.precede(slack_task);
    RAT_terminal_task.precede(slack_task);
    int num_threads = state.range(0);
    tf::Executor executor(num_threads);

    for (auto _ : state)
    {
        // calculate_slack_full_pass_parallel(timing_graph, sta_state, Analysis::Late, executor);
        executor.run(task_graph).wait();
        auto wns = *std::min_element(sta_state.slack.begin(), sta_state.slack.end());
        sta_state.wns = wns;
        benchmark::DoNotOptimize(sta_state);
    }
}

//Sequential single-core baseline
BENCHMARK_CAPTURE(BENCHMARK_STA_SEQUENTIAL, c17, std::string("c17"))->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(BENCHMARK_STA_SEQUENTIAL, c432, std::string("c432"))->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(BENCHMARK_STA_SEQUENTIAL, c880, std::string("c880"))->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(BENCHMARK_STA_SEQUENTIAL, c3540, std::string("c3540"))->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(BENCHMARK_STA_SEQUENTIAL, c6288, std::string("c6288"))->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(BENCHMARK_STA_SEQUENTIAL, c7552, std::string("c7552"))->Unit(benchmark::kMicrosecond);
BENCHMARK(BENCHMARK_PARALLEL_STA_C7552)
    ->Args({1})
    ->Args({2})
    ->Args({4})
    ->Args({6})
    ->Args({8})
    ->Args({12})
    ->Args({16})
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_MAIN();