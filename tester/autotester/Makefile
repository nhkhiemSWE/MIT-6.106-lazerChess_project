.phony: all
all: lauto.jar

OPTS=-encoding US-ASCII -O

lauto.jar: Book.class Counter.class CVerifier.class Harness.class Pattach.class Player.class PlayGame.class Verifier.class main.txt
	jar cmf main.txt lauto.jar $^

%.class : %.java
	javac ${OPTS} $<

.PHONY: clean
clean :
	rm -rf *.class *~ *.o lauto.jar
