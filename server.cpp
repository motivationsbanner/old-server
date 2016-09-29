#include <iostream>
#include <cstdio> // TODO: remove
#include <cassert>
#include <map>
#include <thread>
#include <mutex>
#include <deque>
#include <SFML/Network.hpp>
#include <getopt.h>

float interval = 0.02;
int print_commands = 0;
int print_packet_loss = 0;

enum Direction : sf::Uint8 {
	none, up, down, left, right
};

enum Command_Type : sf::Uint8 {
	null, player_join, player_leave, player_damage
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

			if(print_commands) printf("new join command (id: %i)\n", id);
		}

		sf::Packet& send(sf::Packet& packet) {
			assert(packet << (sf::Uint8) Command_Type::player_join << (sf::Uint16) this->id << (sf::Uint16) this->player_id << (sf::Uint16) this->x << (sf::Uint16)this->y); 
			return packet;
		}
};

class Leave_Command : public Command {
	public:
		sf::Uint16 player_id;

		Leave_Command(sf::Uint16 id, sf::Uint16 player_id) {
			this->id = id;
			this->player_id = player_id;

			if(print_commands) printf("new leave command (id: %i)\n", id);
		}

		sf::Packet& send(sf::Packet& packet) {
			assert(packet << (sf::Uint8) Command_Type::player_leave << (sf::Uint16) this->id << (sf::Uint16) this->player_id); 
			return packet;
		}
};

sf::Packet& operator <<(sf::Packet& packet, Command& command) {
	return command.send(packet);
}

struct Client {
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
	unsigned short port; // port of the client
	sf::Uint16 last_received_command, x, y;

	while (true) {
		assert(socket.receive(packet, sender, port) == sf::Socket::Done);
		assert(packet >> last_received_command >> x >> y);

		mutex.lock();

		std::pair<sf::IpAddress, int> key(sender, port);

		if (clients.find(key) == clients.end()) {
			Client client;

			clients.insert(
				std::pair<std::pair<sf::IpAddress, int>, Client>(key, client)
			);

			std::cout << "new client " << sender <<  " " << port << " " << client.id << std::endl;
			
			client.x = x;
			client.y = y;

			new_clients.push_back(&clients[key]);
		} else {
			Client &client = clients[key];

			if (client.last_packet == 0) { // got multiple packets during the same tick
				if(print_packet_loss) std::cerr << "Ignoring packet" << std::endl;
			}

			while(client.command_queue.size() &&
				client.command_queue.front()->id <= last_received_command) {
				delete client.command_queue.front();
				client.command_queue.pop_front();
			}

			client.last_packet = 0;
			client.x = x;
			client.y = y;
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
				packet << *command;
			}

			packet << (sf::Uint8) Command_Type::null;

			for (auto &client2 : clients) {
				packet << (sf::Uint16) client2.second.id << (sf::Uint16) client2.second.x << (sf::Uint16) client2.second.y;
			}

			assert(socket.send(packet, client.first.first, client.first.second) == sf::Socket::Done);
		}
		new_clients.clear();
		disconnected_clients.clear();

		for (auto &client : clients) {
			if (client.second.last_packet >= 1) {
				if(print_packet_loss) std::cerr << "Packet lost" << std::endl;
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
		sf::sleep(sf::seconds(interval) - clock.restart());
	}
}

int main(int argc, char **argv) {
	struct option options[] = {
		{"interval", required_argument, 0, 'i'},
		{"port", required_argument, 0, 'p'},
		{"print-commands", no_argument, &print_commands, 1},
		{"print-packet-loss", no_argument, &print_packet_loss, 1},
		{0, 0, 0, 0}
	};

	int option_index = 0;
	int port = 4499;
	int c = 0;

	while(c != -1) {
		c = getopt_long(argc, argv, "i:", options, &option_index);
		switch(c) {
			case 'i':
				interval = atof(optarg);
				break;

			case 'p':
				port = atoi(optarg);
				break;

			case '?':
				return -1;
		}
	}

	// Invalid value or 0
	if(interval == 0) {
		interval = 0.02f;
	}

	std::cout << "Starting Server localhost:" << port << std::endl;
	std::cout << "Interval: " << interval << std::endl;
	std::cout << "Print command creation: " << (print_commands ? "true" : "false") << std::endl;
	std::cout << "Print packet loss: " << (print_packet_loss ? "true" : "false") << std::endl;

	assert(socket.bind(port) == sf::Socket::Done);

	std::thread recieve_thread(receive);
	std::thread gameloop_thread(gameloop);

	recieve_thread.join();
	gameloop_thread.join();

	return 0;
}
