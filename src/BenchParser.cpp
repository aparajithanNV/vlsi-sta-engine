//
// Created by aparajithannv on 08/10/26.
//

#include "sta/BenchParser.h"

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <cctype>

auto is_valid_net = [](std::string_view s) -> bool {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isdigit(c);
    });
};

auto BenchParser::parse_input(std::string_view line, TimingGraph &timing_graph, std::unordered_map<std::string, PinID>& net_sources) -> int
{
    // auto trim = [](std::string_view& s)
    // {
    //     while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    //     while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    // };
    auto start = line.find('(');
    auto end = line.find(')');
    if (start < end && start != line.npos && end != line.npos)
    {
        auto net_id = static_cast<std::string>(line.substr(start + 1, end - 1 - start));
        if (net_id.empty())
        {
            return 1;
        }
        else
        {
            // auto valid_id = std::all_of(net_id.begin(), net_id.end(), [](unsigned char c){return std::isdigit(c);});
            auto valid_id = is_valid_net(net_id);
            if (!valid_id)
            {
                return 1;
            }
        }
        auto new_pin_id = timing_graph.add_pin(PinType::PI);
        net_sources[net_id] = new_pin_id;
        timing_graph.update_input_pins(new_pin_id);
    }
    return 0;
}

auto BenchParser::parse_output(std::string_view line, TimingGraph &timing_graph, std::unordered_map<std::string, std::vector<PinID>>& net_destinations) -> int
{
    // auto trim = [](std::string_view& s)
    // {
    //     while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    //     while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    // };
    auto start = line.find('(');
    auto end = line.find(')');
    if (start < end && start != line.npos && end != line.npos)
    {
        auto net_id = static_cast<std::string>(line.substr(start + 1, end - 1 - start));
        if (net_id.empty())
        {
            return 1;
        }
        else
        {
            // auto valid_id = std::all_of(net_id.begin(), net_id.end(), [](unsigned char c){return std::isdigit(c);});
            auto valid_id = is_valid_net(net_id);
            if (!valid_id)
            {
                return 1;
            }
        }
        auto new_pin_id = timing_graph.add_pin(PinType::PO);
        net_destinations[net_id].emplace_back(new_pin_id);
        timing_graph.update_output_pins(new_pin_id);
    }
    return 0;
}

auto BenchParser::parse_gate(std::string_view line, TimingGraph &timing_graph, std::unordered_map<std::string,PinID>& net_sources,
    std::unordered_map<std::string, std::vector<PinID>>& net_destinations) -> int
{
    auto trim = [](std::string_view& s)
    {
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    };
    auto equals_loc = line.find('=');
    auto LHS = line.substr(0, equals_loc);
    trim(LHS);
    auto status_0 = is_valid_net(LHS);
    if (!status_0){return 1;}
    auto Y_pin = timing_graph.add_pin(PinType::INTERNAL);
    net_sources[static_cast<std::string>(LHS)] = Y_pin;
    auto first_parenthesis = line.find('(');
    auto second_parenthesis = line.find(')');
    auto gate_name = line.substr(equals_loc + 1, first_parenthesis - equals_loc - 1);
    trim(gate_name);
    float early_delay, late_delay;
    if (gate_name == "NOT" || gate_name == "INV" || gate_name == "BUF" || gate_name == "BUFF")
    {
        early_delay = 1;
        late_delay = 1;
    }
    else if (gate_name == "AND" || gate_name == "NAND" || gate_name == "OR" || gate_name == "NOR" || gate_name == "XOR" || gate_name == "XNOR")
    {
        early_delay = 2;
        late_delay = 2;
    }
    else
    {
        return 1;
    }
    auto gate_inputs = line.substr(first_parenthesis + 1, second_parenthesis - first_parenthesis - 1);
    size_t start = 0;
    bool has_more_inputs = true;
    while (has_more_inputs)
    {
        auto next = gate_inputs.find(',');
        auto net_id = gate_inputs.substr(start, next - start);
        trim(net_id);
        auto status_1 = is_valid_net(net_id);
        if (!status_1)
        {
            return 1;
        }
        auto in_pin = timing_graph.add_pin(PinType::INTERNAL);
        net_destinations[static_cast<std::string>(net_id)].emplace_back(in_pin);
        timing_graph.add_arc(in_pin, Y_pin, ArcType::GATE, early_delay, late_delay);
        if (next == gate_inputs.npos)
        {
            has_more_inputs = false;
            break;
        }
        gate_inputs = gate_inputs.substr(next + 1, line.size() - next - 1);
        trim(gate_inputs);
        start = 0;
    }
    return 0;
}

auto BenchParser::parse_bench(std::istream& is, TimingGraph& timing_graph) -> int
{
    std::string line;
    std::unordered_map<std::string, std::vector<PinID>> net_destinations;
    std::unordered_map<std::string, PinID> net_sources;

    while (std::getline(is, line))
    {
        if (!line.empty() && line.back()=='\r')
        {
            line.pop_back();
        }
        std::string_view line_view{line};
        if (line_view.starts_with('#') || line_view == "")
        {
            continue;
        }
        // auto print_view = static_cast<std::string>(view);
        // std::cout << print_view << std::endl;
        if (line_view.starts_with("INPUT"))
        {
            auto status = parse_input(line_view, timing_graph, net_sources);
            if (status == 1){return 1;}
        }
        else if (line_view.starts_with("OUTPUT"))
        {
            auto status = parse_output(line_view, timing_graph, net_destinations);
            if (status == 1){return 1;}
        }
        else
        {
            auto status = parse_gate(line_view, timing_graph, net_sources, net_destinations);
            if (status == 1)
            {
                return 1;
            }
        }
    }

    // Create the wire arcs
    for (auto& net_data : net_sources)
    {
        auto [net_name, source_pin_id] = net_data;
        auto destination_pin_ids = net_destinations[net_name];
        for (auto destination_pin_id : destination_pin_ids)
        {
            timing_graph.add_arc(source_pin_id, destination_pin_id, ArcType::WIRE,
                0.0, 1.0);
        }
    }

    return 0;
}

