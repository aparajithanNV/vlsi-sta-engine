//
// Created by aparajithannv on 08/10/26.
//

#ifndef STA_ENGINE_STANALYZER_H
#define STA_ENGINE_STANALYZER_H

#include <limits>
#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/for_each.hpp>
#include "TimingGraph.h"

inline constexpr float NEG_INF = -std::numeric_limits<float>::infinity();
inline constexpr float POS_INF = std::numeric_limits<float>::infinity();

enum class STAMode : uint8_t
{
   FullPassLate = 0,
   FullPassEarly = 1,
   IncrementalLate = 2,
   IncrementalEarly = 3,
};

enum class Analysis : uint8_t
{
   Late = 0,
   Early = 1
};

struct STAState
{
   std::vector<float> actual_arrival_times;
   std::vector<float> required_arrival_times;
   std::vector<float> slack;

   float circuit_RAT; // constraint set by user
   float wns; // worst-case negative slack
   std::vector<PinID> critical_path;
   std::vector<ArcID> critical_path_arcs;
};

struct ArcUpdates
{
   std::vector<ArcID> arcs;
   std::vector<float> early_delays;
   std::vector<float> late_delays;
};

auto analyze(TimingGraph& timing_graph, STAMode sta_mode, STAState& sta_state, ArcUpdates& arc_data) -> bool;
auto calculate_slack_full_pass_late(TimingGraph& timing_graph, STAState& sta_state) -> void;
auto calculate_slack_full_pass_early(TimingGraph& timing_graph, STAState& sta_state) -> void;
auto calculate_slack_incremental_late(TimingGraph& timing_graph, STAState& sta_state, ArcUpdates& arc_data) -> void;
auto calculate_slack_incremental_early(TimingGraph& timing_graph, STAState& sta_state, ArcUpdates& arc_data) -> void;
auto calculate_slack(TimingGraph& timing_graph, STAState& sta_state, Analysis mode) -> void;
auto calculate_AAT(TimingGraph& timing_graph, STAState& sta_state, Analysis mode) -> void;
auto calculate_RAT(TimingGraph& timing_graph, STAState& sta_state, Analysis mode) -> void;
auto calculate_incremental_AAT(TimingGraph& timing_graph, STAState& sta_state, ArcUpdates& arc_data, Analysis mode) -> void;
auto calculate_incremental_RAT(TimingGraph& timing_graph, STAState& sta_state, ArcUpdates& arc_data, Analysis mode) -> void;

auto build_AAT_tasks(tf::Taskflow& task_graph, TimingGraph& timing_graph, STAState& sta_state, Analysis mode) -> tf::Task;
auto build_RAT_tasks(tf::Taskflow& task_graph, TimingGraph& timing_graph, STAState& sta_state, Analysis mode) -> tf::Task;
auto build_slack_tasks(tf::Taskflow& task_graph, TimingGraph& timing_graph, STAState& sta_state, Analysis mode) -> tf::Task;
auto calculate_slack_full_pass_parallel(TimingGraph& timing_graph, STAState& sta_state, Analysis mode, tf::Executor& executor) -> void;

auto find_critical_path(TimingGraph& timing_graph, STAState& sta_state, Analysis mode) -> void;
auto report_timing(TimingGraph& timing_graph, STAState& sta_state, Analysis mode) -> void;
#endif //STA_ENGINE_STANALYZER_H
