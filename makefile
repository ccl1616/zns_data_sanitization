# sample Makefile, feel free to make any modification.
RES = main.cpp tree.cpp
EXE = RPK64
all:
	g++ -std=c++11 $(RES) -o $(EXE)
# clean:
# 	rm $(EXE)