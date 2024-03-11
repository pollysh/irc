NAME = ircserv
CXX = c++

CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -fsanitize=address
LDFLAGS = -fsanitize=address

SOURCES = Server.cpp Commands.cpp ProcessCmd.cpp Message.cpp main.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = ircserv

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

fclean: clean
	rm -f $(NAME)
	@echo "Executable removed"

re: fclean all

.PHONY: all clean fclean re
