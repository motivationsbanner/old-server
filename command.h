#include <SFML/Network.hpp>

enum Command_Type : sf::Uint8 {
	null, player_join, player_leave, player_damage, login, kick
};

class Command { 
	public:
		sf::Uint16 id;
		virtual sf::Packet& send(sf::Packet& packet) {}
};

class Join_Command : public Command {
	public:
		sf::Uint16 x;
		sf::Uint16 y;
		sf::Uint16 player_id;

		Join_Command(sf::Uint16 id, sf::Uint16 player_id, sf::Uint16 x, sf::Uint16 y); 

		sf::Packet& send(sf::Packet& packet); 
};

class Leave_Command : public Command {
	public:
		sf::Uint16 player_id;

		Leave_Command(sf::Uint16 id, sf::Uint16 player_id);

		sf::Packet& send(sf::Packet& packet); 
};

class Kick_Command : public Command {
	public:
		Kick_Command(sf::Uint16 id);

		sf::Packet& send(sf::Packet& packet); 
};

sf::Packet& operator <<(sf::Packet& packet, Command& command);
