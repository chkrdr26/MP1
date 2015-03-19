all: CentralServer

CentralServer: CentralServer.o 
	g++ -std=c++11 CentralServer.o -o CentralServer #remember to add -std=c++11 because you used c++11 thread in the program
CentralServer.o: CentralServer.cpp
	g++ -std=c++11 -c CentralServer.cpp    #why not header files?

clean:
	rm *.o
