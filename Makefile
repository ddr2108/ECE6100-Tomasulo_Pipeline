CXXFLAGS := -g -Wall -std=c++0x -lm
#CXXFLAGS := -g -Wall -lm
CXX=g++
SRC=procsim.cpp procsim_driver.cpp
PROCSIM=./procsim
R=8
J=1
K=2
L=3
F=4
M=2

build:
	$(CXX) $(CXXFLAGS) $(SRC) -o procsim

run:
	$(PROCSIM) -r$R -f$F -m$M -j$J -k$K -l$L < traces/gcc.100k.trace 

clean:
	rm -f procsim *.o
