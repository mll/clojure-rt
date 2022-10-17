
CC = arch -arm64 clang
CCP = arch -arm64 clang++

MYFLAGSOPT = -Wall -Ofast  -I/opt/homebrew/include
MYFLAGS = -Wall -g -I/opt/homebrew/include
CPPFLAGS = -std=c++17  
CFLAGS =  $(MYFLAGSOPT) 
PROFILE = -Ofast 

LIBS = -L/opt/homebrew/lib -lprotobuf
OBJFILES =  Object.o Integer.o PersistentList.o PersistentVector.o PersistentVectorNode.o String.o Double.o sds.o pool.o runtime.o libtcmalloc_and_profiler.a

default: all

all: 
	$(MAKE) runtime
runtime : $(OBJFILES)
	$(CCP) -o runtime $(PROFILE) $(LIBS)  $(OBJFILES) 
clean:
	rm -f *.o
Object.o: Object.c Object.h  
	$(CC) -c $(CFLAGS) Object.c
runtime.o: runtime.cpp
	$(CC) -c $(CFLAGS) $(CPPFLAGS) runtime.cpp
Integer.o: Integer.c Integer.h
	$(CC) -c $(CFLAGS) Integer.c
PersistentList.o: PersistentList.c PersistentList.h
	$(CC) -c $(CFLAGS) PersistentList.c
PersistentVector.o: PersistentVector.c PersistentVector.h
	$(CC) -c $(CFLAGS) PersistentVector.c
PersistentVectorNode.o: PersistentVectorNode.c PersistentVectorNode.h
	$(CC) -c $(CFLAGS) PersistentVectorNode.c
String.o: String.c String.h  
	$(CC) -c $(CFLAGS) String.c
Double.o: Double.c Double.h  
	$(CC) -c $(CFLAGS) Double.c
sds.o: sds/sds.c sds/sds.h
	$(CC) -c $(CFLAGS) sds/sds.c
pool.o: pool/pool.c pool/pool.h
	$(CC) -c $(CFLAGS) pool/pool.c