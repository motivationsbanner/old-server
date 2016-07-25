#include <iostream>
#include <cstdio>
#include <cassert>
#include <map>
#include <thread>
#include <mutex>
#include <deque>
#include <SFML/Network.hpp>

#define DEBUG

#ifdef DEBUG
	#define DEBUG_PRINT printf // there has to be a better way
	#define TICK_TIME 1
#else
	#define DEBUG_PRINT
	#define TICK_TIME 0.02f
#endif

enum Direction : sf::Uint8 {
	none, up, down, left, right	
};

enum Command_Type : sf::Uint8 {
	null, player_join, player_leave, player_damage, message
};

std::mutex mutex;
sf::UdpSocket socket;
sf::Uint16 newest_id = 0; // the id of the newest player

class Command { // (i think) this could (and probably should) be in the Client class so it can access the ids 
	public:
		sf::Uint16 id;
		virtual sf::Packet& send(sf::Packet& packet) {}
};

class Join_Command : public Command {
	public:
		sf::Uint16 x;
		sf::Uint16 y;
		sf::Uint16 player_id;

		Join_Command(sf::Uint16 id, sf::Uint16 player_id, sf::Uint16 x, sf::Uint16 y) {
			this->id = id;
			this->player_id = player_id;
			this->x = x;
			this->y = y;
		
			DEBUG_PRINT("new join command (id: %i)\n", id);
		}

		sf::Packet& send(sf::Packet& packet) {
			DEBUG_PRINT("sending join command (id: %i)\n", id);

			assert(packet << Command_Type::player_join << this->id << this->player_id << this->x << this->y); 
			return packet;
		}
};

class Leave_Command : public Command {
	public:
		sf::Uint16 player_id;

		Leave_Command(sf::Uint16 id, sf::Uint16 player_id) {
			this->id = id;
			this->player_id = player_id;

			DEBUG_PRINT("new leave command (id: %i)\n", id);
		}
		
		sf::Packet& send(sf::Packet& packet) {
			DEBUG_PRINT("sending leave command (id: %i)\n", id);

			assert(packet << Command_Type::player_leave << this->id << this->player_id); 
			return packet;
		}
};

sf::Packet& operator <<(sf::Packet& packet, Command& command) {
	return command.send(packet);
}

struct Client {
	sf::Uint8 action;
	int last_packet = 0; // the amount of ticks ago the last packet has been received
	std::deque<Command *> command_queue;
	sf::Uint16 id = ++newest_id;
	sf::Uint16 last_sent_command = 0; // its id
	sf::Uint16 x = 0;
	sf::Uint16 y = 0;
};

std::map<std::pair<sf::IpAddress, int>, Client> clients;
std::deque<Client *> new_clients;
std::deque<sf::Uint16> disconnected_clients;

void receive() {
	sf::Packet packet;
	sf::IpAddress sender;
	unsigned short port;
	sf::Uint8 action;
	sf::Uint16 last_received_command;

	while (true) {
		assert(socket.receive(packet, sender, port) == sf::Socket::Done);
		assert(packet >> last_received_command >> action);

		DEBUG_PRINT("recieved packet (last_received_command: %i, action %i)\n", last_received_command, action);
	
		mutex.lock();

		std::pair<sf::IpAddress, int> key(sender, port);

		if (clients.find(key) == clients.end()) {
			Client client;
			client.action = action;

			clients.insert(
				std::pair<std::pair<sf::IpAddress, int>, Client>(key, client)
			);
			
			std::cout << "new client " << sender <<  " " << port << " " << client.id << std::endl;

			new_clients.push_back(&clients[key]);
		} else {
			Client &client = clients[key];

			if (client.last_packet == 0) { // got multiple packets during the same tick
				std::cerr << "Ignoring packet" << std::endl;
			}

			while(client.command_queue.size() &&
				client.command_queue.front()->id <= last_received_command) {
				DEBUG_PRINT("deleting command (id: %i)\n", client.command_queue.front()->id);
				delete client.command_queue.front();
				client.command_queue.pop_front();
			}

			client.last_packet = 0;
			client.action = action;
		}
		
		mutex.unlock();
	}
}

void gameloop() {
	sf::Packet packet;
	sf::Clock clock;

	while (true) {
		clock.restart();
		mutex.lock();

		for (auto &client : clients) {
			switch (client.second.action) {
				case Direction::up:
					client.second.y --;
					break;

				case Direction::down:
					client.second.y ++;
					break;

				case Direction::left:
					client.second.x --;
					break;

				case Direction::right:
					client.second.x ++;
					break;
			}
		}

		for (auto &client : clients) {
			packet.clear();

			if (client.second.last_sent_command == 0) {
				// No commands have been sent yet
				// Send player_join commands
				for (auto &client2 : clients) {
					client.second.command_queue.push_back(
						new Join_Command(
							++ client.second.last_sent_command,
							client2.second.id,
							client2.second.x,
							client2.second.y
						)
					);
				}
			} else {
				for (auto &client2 : new_clients) {
					client.second.command_queue.push_back(
						new Join_Command(
							++ client.second.last_sent_command,
							client2->id,
							client2->x,
							client2->y
						)
					);
				}

				for (auto &client2 : disconnected_clients) {
					client.second.command_queue.push_back(
						new Leave_Command(
							++ client.second.last_sent_command,
							client2
						)
					);
				}
			}
		
			for (auto &command : client.second.command_queue) {
				DEBUG_PRINT("seinding command (id: %i)\n", command->id);
				packet << *command;
			}
			
			packet << Command_Type::null;

			for (auto &client2 : clients) {
				packet << client2.second.id << client2.second.x << client2.second.y;
			}

			assert(socket.send(packet, client.first.first, client.first.second) == sf::Socket::Done);
		}
		new_clients.clear();
		disconnected_clients.clear();

		for (auto &client : clients) {
			if (client.second.last_packet >= 1) {
				std::cerr << "Packet lost" << std::endl;
			}

			if (client.second.last_packet > 5) {
				std::cout << "client left " << client.first.first << " " << client.first.second << std::endl;
				disconnected_clients.push_back(client.second.id);

				while(client.second.command_queue.size()) {
					delete client.second.command_queue.front();
					client.second.command_queue.pop_front();
				}
				clients.erase(client.first);

			} else {
				client.second.last_packet ++;
			}
		}

		mutex.unlock();
		sf::sleep(sf::seconds(TICK_TIME) - clock.restart());
	}
}

int main() {
	std::cout << "Starting mmorpg-server v0.0.1 by Motivationsbanner (c) 2016" << std::endl;

	assert(socket.bind(4499) == sf::Socket::Done);

	std::thread recieve_thread(receive);
	std::thread gameloop_thread(gameloop);

	recieve_thread.join();
	gameloop_thread.join();

	return 0;
}
