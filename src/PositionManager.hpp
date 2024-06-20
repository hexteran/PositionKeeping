#include <string> 
#include <shared_mutex>
#include <unordered_map>
#include <list>
#include <mutex>
#include "Broadcaster.hpp"

class PositionManager: public Broadcaster, public Subscriber
{
public:
	PositionManager()
	{} 

	void OnNewPosition(symbol_pos position) final
	{
		PushPositionLocal(position);
	}

	void PushPositionLocal(const symbol_pos& position)
	{
		std::unique_lock lock(_mut);
		_positionsMap[position.symbol] = position;
	}

	void PushAndBroadcastPosition(const symbol_pos& position)
	{
		PushPositionLocal(position);
		SharePosition(position);
	}

	const symbol_pos& GetPosition(const std::string& symbol)
	{
		std::shared_lock lock(_mut);
		if (!_positionsMap.contains(symbol))
			_positionsMap[symbol] = symbol_pos{ .symbol = symbol, .net_position = 0 };
		return _positionsMap[symbol];
	}

	const std::unordered_map<std::string, symbol_pos>& GetPositionsMap()
	{
		return _positionsMap;
	}

private:
	std::unordered_map<std::string, symbol_pos> _positionsMap;
	std::shared_mutex _mut;
};
