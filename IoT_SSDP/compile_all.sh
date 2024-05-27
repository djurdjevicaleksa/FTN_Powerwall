gcc -fPIC -shared ./ina219/lecaina219.c -o ./build/liblecaina219.so -I./ina219/ -lm

gcc -Wall ./src/centralniUredjaj.c -o centralni -I./include/ -pthread
gcc -Wall ./src/senzorUredjaj.c -o senzor -I./include/ -I./ina219 -L./build -llecaina219 -lmosquitto -pthread -lm
gcc -Wall ./src/aktuatorUredjaj.c -o aktuator -I./include/ -I./ina219 -L./build -llecaina219 -lmosquitto -pthread -lm
gcc -Wall ./src/kontrolerUredjaj.c -o kontroler -I./include/ -I./ina219 -L./build -llecaina219 -lmosquitto -pthread -lm
