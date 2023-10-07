
CC = arch -arm64 clang
CCP = arch -arm64 clang++
AR = arch -arm64 ar
ARFLAGS = rcs
MYFLAGSOPT = -Ofast
MYFLAGS = -g 
CPPFLAGS = -std=c++14  
CFLAGS =  $(MYFLAGS) -I/opt/homebrew/include
PROFILE = -Ofast 

LIBS = -L/opt/homebrew/lib -lprotobuf

OBJFILES =  Object.o Integer.o PersistentList.o PersistentVector.o PersistentVectorNode.o String.o Double.o Nil.o Boolean.o Symbol.o ConcurrentHashMap.o Interface.o Keyword.o Function.o PersistentArrayMap.o

default: all

all: 
	$(MAKE) runtime
runtime-tests: $(OBJFILES) runtime-tests.o
	$(CCP) -o runtime-tests $(PROFILE) $(LIBS) runtime-tests.o  $(OBJFILES) 
runtime: $(OBJFILES)
	$(AR) $(ARFLAGS) runtime.a  $(OBJFILES) 
clean:
	rm -f *.o
	rm runtime.a
Object.o: Object.c Object.h  
	$(CC) -c $(CFLAGS) Object.c
runtime-tests.o: runtime-tests.cpp
	$(CCP) -c $(CFLAGS) $(CPPFLAGS) runtime-tests.cpp
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
Nil.o: Nil.c Nil.h
	$(CC) -c $(CFLAGS) Nil.c
Boolean.o: Boolean.c Boolean.h  
	$(CC) -c $(CFLAGS) Boolean.c
Symbol.o: Symbol.c Symbol.h  
	$(CC) -c $(CFLAGS) Symbol.c
ConcurrentHashMap.o: ConcurrentHashMap.c ConcurrentHashMap.h  
	$(CC) -c $(CFLAGS) ConcurrentHashMap.c
Interface.o: Interface.c Interface.h  
	$(CC) -c $(CFLAGS) Interface.c
Keyword.o: Keyword.c Keyword.h  
	$(CC) -c $(CFLAGS) Keyword.c
Function.o: Function.c Function.h  
	$(CC) -c $(CFLAGS) Function.c
PersistentArrayMap.o: PersistentArrayMap.c PersistentArrayMap.h  
	$(CC) -c $(CFLAGS) PersistentArrayMap.c

