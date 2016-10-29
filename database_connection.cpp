#include <stdlib.h>
#include <iostream>

#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include "hidden/password.h"
#include "database_connection.h"

DatabaseConnection::DatabaseConnection() {
	driver = get_driver_instance();
	con = driver->connect("tcp://127.0.0.1:3306", "overkeggly_user", MYSQL_PASSWORD);
	con->setSchema("Overkeggly");
}


DatabaseConnection::~DatabaseConnection() {
	delete con;
}

bool DatabaseConnection::test_user(std::string name, std::string password) {
	sql::Statement *stmt;
	sql::ResultSet *res;

	try {
		stmt = con->createStatement();
		res = stmt->executeQuery("SELECT COALESCE(SHA2('"+ password +"', 256) = (SELECT password FROM players WHERE name = '"+ name +"'), 0) 'match'");
		res->next();
		return res->getBoolean("match");
		
		delete res;
		delete stmt;

	} catch (sql::SQLException &e) {
		// TODO: log
		return 0;
	}
}
