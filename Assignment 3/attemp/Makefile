all:agent router
agent:agent.o
	g++ -g -o agent agent.o
	rm agent.o
router:router.o
	g++ -g -o router router.o
	rm router.o
agent.o:agent.cpp
	g++ -g -c agent.cpp
router.o:router.cpp
	g++ -g -c router.cpp
clean:all
	rm all