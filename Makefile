NAME = ircserv
CC = c++

FLAGS = -Wall -Wextra -Werror -std=c++98 -g -fsanitize=address

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
	@$(RM) $(NAME)
	@echo "$(BYELLOW)$(NAME) $(BRED)REMOVED$(DEFAULT)"

re: fclean all

.PHONY: all clean
