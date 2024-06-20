#include <iostream> 
#include <thread>
#include <memory>
#include <atomic>
#include <chrono>
#include <unordered_set>

#include <boost/asio.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include "PositionManager.hpp"

using namespace boost::asio::ip;
using namespace std::chrono_literals;

class TCPSession : public Subscriber, public Broadcaster
{
public:
    TCPSession(int id, std::vector<shared_message>& history, std::atomic<int>& seq_num, std::shared_mutex& mut)
        : _socket(_io_context), _resolver(_io_context), _id(id), _history(history), _seq_num(seq_num), _mut(mut)
    {
        //_listener_thread = std::make_shared<std::jthread>(&TCPSession::_listen, this);
    }

    TCPSession(int id, tcp::socket socket, std::vector<shared_message>& history, std::atomic<int>& seq_num, std::shared_mutex& mut)
        : _socket(std::move(socket)), _resolver(_io_context),
        _id(id), _history(history),
        _seq_num(seq_num), _mut(mut),
        _ack_processing_thread(&TCPSession::_process_ack_messages, this)
    {
        _listener_thread = std::make_shared<std::thread>(&TCPSession::_listen, this);
        _listener_thread->detach();
    }

    int GetId()
    {
        return _id;
    }

    virtual void Stop()
    {
        _is_running = false;
    }

    void SendMessageCopy(shared_message& message);
    void RequestMessageResend(int seq_num);
    
protected:
    void _restore_missing_values();
    virtual void _process_message(shared_message& message);
    void _process_ack_messages();
    void _listen();
    void _send_message(shared_message& message, bool is_ack_needed = false);
    void _wait_for_request();
    void _resend_message(int seq_num);
    virtual void _process_specific_message(shared_message& message);

protected:
    std::mutex _ack_mut;
    std::vector<shared_message> _pending_ack;
    std::shared_ptr<std::thread> _listener_thread;
    std::thread _ack_processing_thread;
    std::vector<shared_message>& _history;
    std::list<shared_message> _bad_seq_num_list;
    std::shared_mutex& _mut;
    boost::asio::io_context _io_context;
    tcp::resolver _resolver;
    tcp::socket _socket;
    boost::asio::streambuf _buffer;

    std::atomic<int> _id = 0;
    std::atomic<int>& _seq_num;
    std::atomic<bool> _is_running = true;
};

class TCPServer
{
    class TCPSessionServerSide : public TCPSession
    {
    public:
        TCPSessionServerSide(int id, tcp::socket socket, TCPServer* server, 
            std::vector<shared_message>& history,
            std::atomic<int>& seq_num, std::shared_mutex& mut) :
            TCPSession(id, std::move(socket), history, seq_num, mut), _server(server)
        {
            _listener_thread = std::make_shared<std::thread>(&TCPSessionServerSide::_listen, this);
            _listener_thread->detach();
        }

        void Stop() final
        {
            TCPSession::Stop();
            _server->RemoveSession(_id);
        }

    private:
        void _process_specific_message(shared_message& message) final;
        void _process_history_request();

    private:
        TCPServer* _server;
    };

public:
    TCPServer(short port, std::vector<shared_message>& history, 
        std::shared_mutex& mut, std::shared_ptr<PositionManager> positionManager) :
        _port(port), _output(std::cout),
        _acceptor(_io_context, tcp::endpoint(tcp::v4(), port)),
        _history(history), _mut(mut),
        _positionManager(positionManager)
    {
        _listener_thread = std::make_shared<std::jthread>(&TCPServer::_run, this);
    }

    void RemoveSession(int id);
    void Stop();
    void Broadcast(shared_message& message, int id);

private:
    void _run();
    void _accept();

private:
    int _last_session_id = 0;
    boost::asio::io_context _io_context;
    std::vector<shared_message>& _history;
    std::atomic<bool> _is_running = true;
    std::list<std::shared_ptr<TCPSession>> _sessions;
    std::shared_ptr<PositionManager> _positionManager;
    std::vector<tcp::socket> _sockets;
    std::shared_ptr<std::jthread> _listener_thread;
    std::atomic<int> _seq_num;
    tcp::acceptor _acceptor;
    std::ostream& _output;
    std::shared_mutex& _mut;
    std::mutex local_mutex;
    int _port;
};

class TCPClient
{
    class TCPSessionClientSide : public TCPSession
    {
    public:
        TCPSessionClientSide(int id, TCPClient* client, std::vector<shared_message>& history, 
            std::atomic<int>& seq_num, std::shared_mutex& mut) :
            TCPSession(0, history, seq_num, mut), _client(client)
        {}

        bool Connect(std::string& ip, int port, bool request_history);
        void SendPosition(symbol_pos& position);
        
        void Stop() final
        {
            TCPSession::Stop();
            _client->_connect_next_server();
        }

        void OnNewPosition(symbol_pos position) override
        {
            SendPosition(position);
        }

    private:
        void _request_history();
        void _process_history_message(shared_message& message);
        void _process_specific_message(shared_message& message) final;

    private:
        TCPClient* _client;
    };

public:
    TCPClient(std::vector<std::pair<std::string, int>>& servers, 
        std::vector<shared_message>& history, std::shared_mutex& mut,
        std::shared_ptr<PositionManager> positionManager, std::shared_ptr<TCPServer> own_server) :
        _resolver(_io_context), _servers(servers), _history(history), 
        _mut(mut), _positionManager(positionManager), _own_server(own_server)
    {
        _run();
    }

    void SendPosition(symbol_pos position)
    {
        _session->SendPosition(position);
    }

    void Stop()
    {
        _session->Stop();
        _is_running = false;
    }

private:
    void _run();
    void _connect_next_server();
    void _send_to_own_server(shared_message& message);

private:
    boost::asio::io_context _io_context;
    std::vector<shared_message>& _history;
    std::vector<std::pair<std::string, int>> _servers;
    std::atomic<int> _seq_num = 0;
    std::shared_ptr<PositionManager> _positionManager = nullptr;
    std::shared_ptr<TCPSessionClientSide> _session = nullptr;
    std::shared_mutex& _mut;
    tcp::resolver _resolver;

    std::shared_ptr<TCPServer> _own_server = nullptr;
    std::atomic<int> _curr_server = 0;
    std::atomic<bool> _is_running = true;

    bool _request_history = true;
};

