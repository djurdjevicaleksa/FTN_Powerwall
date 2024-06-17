gcc -fPIC -shared ina219/lecaina219.c -o build/liblecaina219.so -lm
gcc -Wall senzorUredjaj.c -o senzor  -I./ina219 -L./build -llecaina219 -lmosquitto -pthread -lm
