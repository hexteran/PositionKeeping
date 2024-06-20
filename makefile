all: src/* 
	@mkdir -p bin 
	@g++ -lpthread -std=c++20 -g -o bin/node src/* ./lib/libboost_serialization.a

