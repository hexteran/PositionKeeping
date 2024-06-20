#include <string>

constexpr int buffer_capacity = 1024;

enum message_type
{
	query_push_position,
	query_get_history,
	query_resend_message,
	response_history,
	response_history_last,
	ack
};

struct symbol_pos {
	std::string symbol = "";
	double net_position = 0;
	template<typename archive> void serialize(archive& ar, const unsigned version) {
		ar& symbol;
		ar& net_position;
	}
};

struct shared_message
{
	message_type type;
	int seq_num = 0;
	symbol_pos position;
	template<typename archive> void serialize(archive& ar, const unsigned version) {
		ar& type;
		ar& seq_num;
		ar& position;
	}


};
