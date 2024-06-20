#include "DTO.h"
#include <vector>
#include <memory>

class Subscriber
{
public:
	virtual void OnNewPosition(symbol_pos position) {};
};

class Broadcaster
{
public:
	void AddSubscriber(std::shared_ptr<Subscriber> subscriber)
	{
		_subscribers.push_back(subscriber);
	}

	void SharePosition(symbol_pos position)
	{
		for (auto subscriber : _subscribers)
			subscriber->OnNewPosition(position);
	}
private:
	std::vector<std::shared_ptr<Subscriber>> _subscribers;
};
