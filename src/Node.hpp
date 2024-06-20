#include "TCPConnectivity.hpp"

class Node
{
public:
	Node(std::vector<std::pair<std::string, int>> servers, int port, std::shared_ptr<PositionManager> positionManager) :
		
		_positionManager(positionManager)
	{
		_tcpServer = std::make_shared<TCPServer>(port, _history, _mut, _positionManager);
		_tcpClient = std::make_shared<TCPClient>(servers, _history, _mut, _positionManager, _tcpServer);
	}

	void PushPosition(symbol_pos position)
	{
		_positionManager->PushAndBroadcastPosition(position);
	}

	void ShowPositions()
	{
		const std::unordered_map<std::string, symbol_pos>& positions = _positionManager->GetPositionsMap();
		std::cout << "Positions:\n";
		for (auto record : positions)
		{
			std::cout << record.first << ": " << record.second.net_position << "\n";
		}
	}

	void Stop()
	{
		_tcpServer->Stop();
		_tcpClient->Stop();
	}

private:
	boost::asio::io_context _io_context;

	std::vector<shared_message> _history;
	std::shared_mutex _mut;
	std::shared_ptr<PositionManager> _positionManager;
	std::shared_ptr<TCPClient> _tcpClient;
	std::shared_ptr<TCPServer> _tcpServer;
	
};
