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

/* TODO
 * Need to add virtual source node to graph and update the Parser tests to consider this newly added source node
 */

auto BenchParser::parse_bench(std::istream& is, TimingGraph& timing_graph) -> bool
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
        std::cout << line <<"\n";
        std::cout.flush();
        std::string_view view{line};
        if (view.starts_with('#') || view == "")
        {
            continue;
        }
        // auto print_view = static_cast<std::string>(view);
        // std::cout << print_view << std::endl;
        if (view.starts_with("INPUT"))
        {
            auto start = view.find('(');
            auto end = view.find(')');
            if (start < end && start != view.npos && end != view.npos)
            {
                auto net_id = static_cast<std::string>(view.substr(start + 1, end - 1 - start));
                if (net_id.empty())
                {
                    return true;
                }
                else
                {
                    auto valid_id = std::all_of(net_id.begin(), net_id.end(), [](unsigned char c){return std::isdigit(c);});
                    if (!valid_id)
                    {
                        return true;
                    }
                }
                auto new_pin_id = timing_graph.add_pin(PinType::PI);
                net_sources[net_id] = new_pin_id;
                std::cout << "INPUT " << "net " << net_id << " new pin id: "<< new_pin_id << "\n";
                timing_graph.update_input_pins(new_pin_id);
            }
        }
        else if (view.starts_with("OUTPUT"))
        {
            auto start = view.find('(');
            auto end = view.find(')');
            if (start < end && start != view.npos && end != view.npos)
            {
                auto net_id = static_cast<std::string>(view.substr(start + 1, end - 1 - start));
                if (net_id.empty())
                {
                    return true;
                }
                else
                {
                    auto valid_id = std::all_of(net_id.begin(), net_id.end(), [](unsigned char c){return std::isdigit(c);});
                    if (!valid_id)
                    {
                        return true;
                    }
                }
                auto new_pin_id = timing_graph.add_pin(PinType::PO);
                net_destinations[net_id].emplace_back(new_pin_id);
                std::cout << "OUTPUT " << "net " << net_id << " new pin id "<< new_pin_id << "\n";
            }
        }
        else
        {
            std::istringstream line_stream(line);
            std::string Y_net, equal_sign;
            line_stream >> Y_net >> equal_sign;
            char gate_ch;
            std::string gate_name;
            float gate_early_delay, gate_late_delay;
            while (line_stream >> gate_ch && gate_ch != '(')
            {
                gate_name += gate_ch;
            }
            if (gate_name.empty())
            {
                return true;
            }
            else if (gate_name == "INV")
            {
                gate_early_delay = 1;
                gate_late_delay = 1;
            }
            else if (gate_name == "AND")
            {
                gate_early_delay = 2;
                gate_late_delay = 2;
            }
            else if (gate_name == "OR")
            {
                gate_early_delay = 2;
                gate_late_delay = 2;
            }
            else if (gate_name == "XOR")
            {
                gate_early_delay = 2;
                gate_late_delay = 2;
            }
            else if (gate_name == "NAND")
            {
                gate_early_delay = 2;
                gate_late_delay = 2;
            }
            else if (gate_name == "NOR")
            {
                gate_early_delay = 2;
                gate_late_delay = 2;
            }
            else if (gate_name == "XNOR")
            {
                gate_early_delay = 2;
                gate_late_delay = 2;
            }
            else
            {
                return true;
            }

            std::string A_net, B_net;
            line_stream >> A_net >> B_net;
            A_net.pop_back();
            if (!B_net.empty())
            {
                B_net.pop_back();
            }

            if (A_net.empty() || Y_net.empty())
            {
                return true;
            }
            else
            {
                if (gate_name != "INV" && B_net.empty())
                {
                   return true;
                }
            }
            auto valid_id_A = std::all_of(A_net.begin(), A_net.end(), [](unsigned char c){return std::isdigit(c);});
            auto valid_id_B = std::all_of(B_net.begin(), B_net.end(), [](unsigned char c){return std::isdigit(c);});
            auto valid_id_Y = std::all_of(Y_net.begin(), Y_net.end(), [](unsigned char c){return std::isdigit(c);});

            if (!valid_id_A || !valid_id_Y)
            {
                return true;
            }
            else
            {
                if (gate_name != "INV" && !valid_id_B)
                {
                    return true;
                }
            }

            std::cout << "Driven net " << Y_net << " Gate " << gate_name << " Anet " << A_net << " B net " << B_net << "\n";

            auto new_A_pin = timing_graph.add_pin(PinType::INTERNAL);
            auto new_Y_pin = timing_graph.add_pin(PinType::INTERNAL);
            timing_graph.add_arc(new_A_pin, new_Y_pin, ArcType::GATE,
                gate_early_delay, gate_late_delay);
            net_destinations[A_net].emplace_back(new_A_pin);
            net_sources[Y_net] = new_Y_pin;
            if (gate_name != "INV")
            {
                auto new_B_pin = timing_graph.add_pin(PinType::INTERNAL);
                timing_graph.add_arc(new_B_pin, new_Y_pin, ArcType::GATE,
                    gate_early_delay, gate_late_delay);
                net_destinations[B_net].emplace_back(new_B_pin);
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

    return false;
}
