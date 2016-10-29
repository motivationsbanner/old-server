#include "command.h"
#include <cassert>

Join_Command::Join_Command(sf::Uint16 id, sf::Uint16 player_id, sf::Uint16 x, sf::Uint16 y) {
	this->id = id;
	this->player_id = player_id;
	this->x = x;
	this->y = y;
}

sf::Packet& Join_Command::send(sf::Packet& packet) {
	assert(packet << (sf::Uint8) Command_Type::player_join << (sf::Uint16) this->id << (sf::Uint16) this->player_id << (sf::Uint16) this->x << (sf::Uint16)this->y); 
	return packet;
}

Leave_Command::Leave_Command(sf::Uint16 id, sf::Uint16 player_id) {
	this->id = id;
	this->player_id = player_id;
}

sf::Packet& Leave_Command::send(sf::Packet& packet) {
	assert(packet << (sf::Uint8) Command_Type::player_leave << (sf::Uint16) this->id << (sf::Uint16) this->player_id); 
	return packet;
}

Kick_Command::Kick_Command(sf::Uint16 id) {
	this->id = id;
}

sf::Packet& Kick_Command::send(sf::Packet& packet) {
	assert(packet << (sf::Uint8) Command_Type::kick << (sf::Uint16) this->id); 
	return packet;
}

sf::Packet& operator <<(sf::Packet& packet, Command& command) {
	return command.send(packet);
}
