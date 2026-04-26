
.PHONY: all examples example_csv

CXX=c++
CXX_FLAG= -Wall -Wextra -Werror -Wconversion -std=c++98 -pedantic-errors

all:
	$(CXX) $(CXX_FLAG) main.cpp

examples:
	$(CXX) $(CXX_FLAG) examples.cpp

example_csv:
	$(CXX) $(CXX_FLAG) example_csv.cpp
