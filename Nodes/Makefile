all: Node		

Node: Node.o 
	g++ -std=c++11 Node.o -o Node 
Node.o: Node.cpp
	g++ -std=c++11 -c Node.cpp

clean:
	rm *.o
