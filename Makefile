OBJECTS = MiscUtil.o Database.o Packet.o main.o
DEFINES = -D_REENTRANT -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

inno_monitor : $(OBJECTS)
	g++ -Wall -m64 -g -o inno_monitor $(DEFINES) $(OBJECTS) -lpthread -lmysqlclient_r -L/usr/lib64 -L/usr/lib64/mysql -L/usr/local/lib
	rm $(OBJECTS)

MiscUtil.o : MiscUtil.cpp
	g++ -Wall -m64 -g -c $(DEFINES) MiscUtil.cpp

Database.o : Database.cpp
	g++ -Wall -m64 -g -c $(DEFINES) -I/usr/include/mysql/ Database.cpp

Packet.o : Packet.cpp
	g++ -Wall -m64 -g -c $(DEFINES) Packet.cpp

main.o : main.cpp main_monitorserver.cpp main_clean.cpp 
	g++ -Wall -m64 -g -c $(DEFINES) -I/usr/include/mysql/ main.cpp

clean:
	rm $(OBJECTS)

cscope:
	cscope -bRu -I /usr/include -I /usr/include/mysql
