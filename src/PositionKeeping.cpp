#include <iostream>
#include "Node.hpp"

using namespace std;

struct Params
{
    int port = 0;
    std::vector<std::pair<std::string, int>> servers;
};

vector<string> split(string& a, char del)
{
    stringstream ss(a);
    vector<string> result;
    string token;
    while (std::getline(ss, token, del))
    {
        result.push_back(token);
    }
    return result;
}

Params parse_command_line(int argc, char* argv[])
{
    Params result;
    string server_ip = "";
    int server_port = 0, own_port = 0;
    try
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string command = argv[i];
            if (command[0] != '-')
                throw std::invalid_argument("invalid command");

            if (command == "-server")
            {
                ++i;
                std::pair<std::string, int> server;
                string ip_port = argv[i];
                auto address = split(ip_port, ':');
                server.first = address[0];
                server.second = stoi(address[1]);
                result.servers.push_back(server);
            }
            else if (command == "-port")
            {
                ++i;
                string port = argv[i];
                result.port = stoi(port);
            }
        }
        if (result.port == 0 || result.servers.empty())
        {
            throw invalid_argument("invalid parmeters");
        }
    }
    catch (...)
    {
        throw invalid_argument("invalid parmeters");
    }
    
    return result;
}
int main(int argc, char* argv[])
{
    auto params = parse_command_line(argc, argv);
  
    auto positionManager = std::make_shared<PositionManager>();
    Node node(params.servers, params.port, positionManager);

    while (true)
    {
        try
        {
            string command;
            std::cin >> command;

            if (command == "new_position")
            {
                symbol_pos pos;
                std::cin >> pos.symbol;
                std::cin >> pos.net_position;
                node.PushPosition(pos);
            }
            else if (command == "show_positions")
            {
                node.ShowPositions();
            }
            else if (command == "generate")
            {
                using namespace std::chrono_literals;
                int num;
                int ms;
                std::string test_name;
                std::cin >> test_name;
                std::cin >> num;
                std::cin >> ms;
                std::cout << "Generating transactions...\n";
                for (; num > 0; --num)
                {
                    std::this_thread::sleep_for(ms * 1ms);
                    symbol_pos pos{ .symbol = test_name + "." + std::to_string(num), .net_position = double(num) };
                    node.PushPosition(pos);
                }
                std::cout << "Generating is done\n";
            }

            else
            {
                std::cout << "Unknown command\n";               
            }
        }
        catch (...)
        {
            std::cout << "Error occured during execution of the command\n";
        }
    }
    return 0;
}
   
