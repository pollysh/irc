NAME = ircserv
CC = c++

# Compiler flags
FLAGS = -Wall -Wextra -Werror -std=c++98 -g -fsanitize=address

# Linker flags
LDFLAGS =

# Define the source files
SOURCES = Server.cpp Commands.cpp ProcessCmd.cpp Message.cpp main.cpp

# Define the object files
OBJECTS = $(SOURCES:.cpp=.o)

# Define the executable file
EXECUTABLE = ircserv

# The first rule is the default when "make" is run without arguments
all: $(SOURCES) $(EXECUTABLE)

# Rule to create the executable
$(EXECUTABLE): $(OBJECTS) 
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

# Rule to create object files
.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

fclean: clean
	@$(RM) $(NAME)
	@echo "$(BYELLOW)$(NAME) $(BRED)REMOVED$(DEFAULT)"

re: fclean all

# Phony targets
.PHONY: all clean
