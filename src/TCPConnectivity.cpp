#include "TCPConnectivity.hpp"

void TCPSession::SendMessageCopy(shared_message& message)
{
	_send_message(message);	
}

void TCPSession::RequestMessageResend(int seq_num)
{
	std::cout << "Resend message request for seq_num " << seq_num << '\n';
	shared_message message{ .type = message_type::query_resend_message, .seq_num = seq_num };
	_send_message(message);
}

void TCPSession::_process_specific_message(shared_message& message)
{

}

void TCPSession::_restore_missing_values()
{
	_bad_seq_num_list.sort([](shared_message& a, shared_message& b) { return a.seq_num < b.seq_num; });
	std::vector<std::list<shared_message>::iterator> to_remove;
	for (auto iter = _bad_seq_num_list.begin(); iter != _bad_seq_num_list.end(); ++iter)
	{
		if (iter->seq_num - _seq_num == 1 && (iter->type == message_type::query_push_position || iter->type == message_type::response_history))
		{
			++_seq_num;
			_process_specific_message(*iter);
			to_remove.push_back(iter);
			_history.push_back(*iter);
		}
		else if (iter->type != message_type::query_push_position && iter->type != message_type::response_history)
		{
			_process_specific_message(*iter);
			to_remove.push_back(iter);
		}
		else if (iter->seq_num <= _seq_num)
		{
			to_remove.push_back(iter);
		}
		else
		{
			RequestMessageResend(_seq_num + 1);
			return;
		}
	}
	for (auto iter : to_remove)
	{
		_bad_seq_num_list.erase(iter);
	}
}

void TCPSession::_process_message(shared_message& message)
{
	if (message.type == message_type::ack)
	{
		//std::lock_guard<std::mutex> lock(_ack_mut);
		auto result = std::find_if(_pending_ack.begin(), _pending_ack.end(), [&](shared_message& m) {return m.seq_num == message.seq_num; });
		if (result != _pending_ack.end())
			_pending_ack.erase(result);
		return;
	}
	
	shared_message ack_mes{ .type = message_type::ack, .seq_num = message.seq_num };
	_send_message(ack_mes);

	std::unique_lock lock(_mut);
	_bad_seq_num_list.push_back(message);
	_restore_missing_values();
}

void TCPSession::_process_ack_messages()
{
	while (_is_running)
	{
		std::this_thread::sleep_for(500ms);
		std::lock_guard<std::mutex> lock(_ack_mut);
		for (auto message : _pending_ack)
		{
			_send_message(message, false);
		}
	}
}

void TCPSession::_listen()
{
	while (_is_running)
	{
		_wait_for_request();
	}
}

void TCPSession::_send_message(shared_message& message, bool is_ack_needed)
{
	boost::asio::streambuf buf;
	std::ostream os(&buf);
	boost::archive::binary_oarchive oa(os);
	oa << message;
	boost::system::error_code ec;
	std::this_thread::sleep_for(1ms);
	auto result = boost::asio::write(_socket, buf, ec);

	if (ec)
		return;

	/*if (is_ack_needed)
	{
		std::lock_guard<std::mutex> lock(_ack_mut);
		_pending_ack.push_back(message);
	}*/
}

void TCPSession::_wait_for_request()
{
	boost::asio::streambuf buffer;
	boost::system::error_code ec;
	boost::asio::read_until(_socket, buffer, "\0", ec);
	if (ec || buffer.size() == 0)
	{
		this->Stop();
		return;
	}

	shared_message msg;
	try
	{
		std::istream is(&buffer);
		boost::archive::binary_iarchive ia(is);
		ia >> msg;
//		std::cout << "Received message: seq_num: " << msg.seq_num << " type: " << msg.type << '\n';
	}
	catch (std::exception& ex)
	{
		std::cout << "Error while reading data: " << ex.what() << '\n';
		return;
	}
	_process_message(msg);
}

void TCPSession::_resend_message(int seq_num)
{
	//std::shared_lock lock(_mut);
	auto result = std::find_if(_history.begin(), _history.end(), [&](shared_message& message) { return message.seq_num == seq_num; });
	if (result != _history.end())
		_send_message(*result);
}

void TCPServer::RemoveSession(int id)
{
	std::lock_guard<std::mutex> lock(local_mutex);
	auto to_remove = _sessions.end();
	for (auto iter = _sessions.begin(); iter != _sessions.end(); ++iter)
	{
		if ((*iter)->GetId() == id)
		{
			to_remove = iter;
			break;
		}
	}
	if (to_remove != _sessions.end())
		_sessions.erase(to_remove);
}

void TCPServer::Stop()
{
	_is_running = false;
	for (auto session : _sessions)
	{
		session->Stop();
	}
}


void TCPServer::Broadcast(shared_message& message, int id = std::numeric_limits<int>().max())
{
	for (auto session : _sessions)
	{
		if (session->GetId() != id)
		{
			session->SendMessageCopy(message);
		}
	}
}

void TCPServer::_run()
{
	while (_is_running)
	{
		_accept();
	}
}

void TCPServer::_accept() {
	try
	{
		auto socket = _acceptor.accept();
		_output << "creating session on: "
			<< socket.remote_endpoint().address().to_string()
			<< ":" << socket.remote_endpoint().port() << '\n';
		auto ptr = std::make_shared<TCPSessionServerSide>(_last_session_id++, std::move(socket), this, _history, _seq_num, _mut);
		ptr->AddSubscriber(_positionManager);
		_positionManager->AddSubscriber(ptr);
		_sessions.push_back(ptr);
	}
	catch (std::exception& ex)
	{
		//std::cout << "Can't accept connection: " << ex.what() << '\n';
	}
}

void TCPServer::TCPSessionServerSide::_process_specific_message(shared_message& message)
{
	switch (message.type)
	{
		case (message_type::query_resend_message):
			_resend_message(message.seq_num);
			break;
		case (message_type::query_get_history):
			_process_history_request();
			break;
		case (message_type::query_push_position):
			_server->Broadcast(message, _id);
			SharePosition(message.position);
			break;
	}
}

void  TCPServer::TCPSessionServerSide::_process_history_request()
{
	int seq_num = 0;
	int batch_size = 100;
	int counter = 1;
	for (auto message : _history)
	{
//		if (counter % batch_size == 0)
//			std::this_thread::sleep_for(500ms);
		++counter;
		shared_message new_message = message;
		new_message.type = message_type::response_history;
		_send_message(new_message, true);
		seq_num = new_message.seq_num;
	}
	shared_message new_message{ .type = message_type::response_history_last, .seq_num = seq_num + 1 };
	_send_message(new_message, true);
}


void TCPClient::_run()
{
	_connect_next_server();
}

void TCPClient::_connect_next_server()
{
	if (_request_history) 
		_seq_num = 0;
	_session = std::make_shared<TCPSessionClientSide>(0, this, _history, _seq_num, _mut);
	while (!_session->Connect(_servers[_curr_server % _servers.size()].first, _servers[_curr_server % _servers.size()].second, _request_history) && _is_running)
	{
		++_curr_server;
	}
	_request_history = false;
	_session->AddSubscriber(_positionManager);
	_positionManager->AddSubscriber(_session);
}

void TCPClient::_send_to_own_server(shared_message& message)
{
	if (_own_server != nullptr)
		_own_server->Broadcast(message, -1);
}

bool TCPClient::TCPSessionClientSide::Connect(std::string& ip, int port, bool request_history)
{
	try
	{
		std::cout << "Connecting " << ip << ":" << port << '\n';
		auto result = boost::asio::connect(_socket, _resolver.resolve(ip, std::to_string(port)));
		_listener_thread = std::make_shared<std::thread>(&TCPSessionClientSide::_listen, this);
		_listener_thread->detach();
		if (request_history) 
			_request_history();
	}
	catch (std::exception ex)
	{
		std::cout << "Session Connection Error: " + ip + ":" + std::to_string(port) + "\n";
		return false;
	}
	return true;
}

void TCPClient::TCPSessionClientSide::SendPosition(symbol_pos& position)
{
	std::shared_lock lock(_mut);
	shared_message message{ .type = message_type::query_push_position, .seq_num = _seq_num + 1, .position = position };
	_send_message(message);
	_history.push_back(message);
	_client->_send_to_own_server(message);
	++_seq_num;
}

void TCPClient::TCPSessionClientSide::_request_history()
{
	std::unique_lock lock(_mut);
	std::cout << "Sending history request\n";
	_history.clear();
	shared_message message{ .type = message_type::query_get_history };
	_send_message(message);
}

void TCPClient::TCPSessionClientSide::_process_history_message(shared_message& message)
{
	if (message.type == message_type::response_history)
	{
		SharePosition(message.position);
		_history.push_back(message);
	}
}

void TCPClient::TCPSessionClientSide::_process_specific_message(shared_message& message)
{
	switch (message.type)
	{
		case (message_type::query_resend_message):
			_resend_message(message.seq_num);
			break;
		case (message_type::query_push_position):
			_client->_send_to_own_server(message);
			SharePosition(message.position);
			break;
		case (message_type::response_history):
			_process_history_message(message);
			break;
		case (message_type::response_history_last):
			_process_history_message(message);
			break;
	}
}
