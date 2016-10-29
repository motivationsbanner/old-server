#include <string>
#include <mysql_connection.h>
#include <cppconn/driver.h>

class DatabaseConnection {
	public:
		DatabaseConnection();
		~DatabaseConnection();
		bool test_user(std::string name, std::string password);

	private:
		sql::Driver *driver;
		sql::Connection *con;
};
