CXX = clang++
CXXFLAGS = -Wall -Wextra -O3 -std=c++17 -pthread -mavx2 -Rpass=loop-vectorize -Rpass-missed=loop-vectorize -Rpass-analysis=loop-vectorize 

all: 
	${CXX} ${CXXFLAGS} -o pi.out pi.cpp