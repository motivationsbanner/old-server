#include <iostream>
#include <cstdio> // TODO: remove
#include <cassert>
#include <map>
#include <thread>
#include <mutex>
#include <deque>
#include <string>
#include <SFML/Network.hpp>
#include <getopt.h>

#include "database_connection.h"
#include "command.h"

// TODO: clean this mess up

float interval = 0.05;
int print_packet_loss = 0;

enum Direction : sf::Uint8 {
	none, up, down, left, right
};

std::mutex mutex;
sf::UdpSocket socket;
sf::Uint16 newest_id = 0; // the id of the newest player
DatabaseConnection con;

struct Client {
	int last_packet = 0; // the amount of ticks ago the last packet has been received
	std::deque<Command *> command_queue;
	sf::Uint16 id = ++newest_id;
	sf::Uint16 last_sent_command = 0; // its id
	sf::Uint16 x = 0;
	sf::Uint16 y = 0;
	bool verified = false;
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
		mutex.lock();

		assert(socket.receive(packet, sender, port) == sf::Socket::Done);
		assert(packet >> last_received_command >> x >> y);

		std::pair<sf::IpAddress, int> key(sender, port);

		if (clients.find(key) == clients.end()) {
			Client client;

			clients.insert(
				std::pair<std::pair<sf::IpAddress, int>, Client>(key, client)
			);
			
			client.x = x;
			client.y = y;

			sf::Uint8 command_type;
			std::string name;
			std::string password;
			assert(packet >> command_type >> name >> password);

			std::cout << name << " (" << sender << ":" << port << ") is trying to log in" << std::endl;
			std::cout << "new client " << sender <<  " " << port << " " << client.id << std::endl;

			if (con.test_user(name, password)) {
				std::cout << "login information is correct" << std::endl;
				new_clients.push_back(&clients[key]);
				client.verified = true;
			} else {
				std::cout << "login information is not correct " << std::endl;
		
				client.command_queue.push_back(new Kick_Command(++ client.last_sent_command));
			}
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

			// unverified clients only get the kick command
			if (client.second.verified) {
				if (client.second.last_sent_command == 0) {
					// No commands have been sent yet
					// Send player_join commands
					for (auto &client2 : clients) {
						if (client2.second.verified && client2.second.id != client.second.id) {
							client.second.command_queue.push_back(
								new Join_Command(
									++ client.second.last_sent_command,
									client2.second.id,
									client2.second.x,
									client2.second.y
								)
							);
						}
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
			}

			for (auto &command : client.second.command_queue) {
				packet << *command;
			}

			packet << (sf::Uint8) Command_Type::null;

			packet << (sf::Uint16) 0 << (sf::Uint16) client.second.x << (sf::Uint16) client.second.y;

			for (auto &client2 : clients) {
				if (client2.second.id != client.second.id) {
					packet << (sf::Uint16) client2.second.id << (sf::Uint16) client2.second.x << (sf::Uint16) client2.second.y;
				}
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
				
				if (client.second.verified) {	
					disconnected_clients.push_back(client.second.id);
				}

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
		interval = 0.05f;
	}

	std::cout << "Starting Server localhost:" << port << std::endl;
	std::cout << "Interval: " << interval << std::endl;
	std::cout << "Print packet loss: " << (print_packet_loss ? "true" : "false") << std::endl;

	assert(socket.bind(port) == sf::Socket::Done);

	std::thread recieve_thread(receive);
	std::thread gameloop_thread(gameloop);

	recieve_thread.join();
	gameloop_thread.join();

	return 0;
}
