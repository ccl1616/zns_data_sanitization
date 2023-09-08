# sample Makefile, feel free to make any modification.
RES = main.cpp
EXE = main
all:
	g++ -std=c++11 $(RES) -o $(EXE)
clean:
	rm $(EXE)