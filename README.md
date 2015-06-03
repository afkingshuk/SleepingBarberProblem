Sleeping barber problem
=======

Implement of modified variant of sleeping barber problem using semaphores
and shared memory. Prevent to deadlock and starvation.

Info about Sleeping barber problem: http://en.wikipedia.org/wiki/Sleeping_barber_problem

Installation:
--------------
    $ git clone https://github.com/tomasvalek/SleepingBarberProblem.git

Requirement:
    gcc

Using:
-------------
./barber Q genc genb N F
Q - number of chairs in waiting room
genc - interval of generate customers [ms]
genb - interval of generate of time service [ms]
N - number of customers
F - output filename

Using valgrind:
--------------
    valgrind --tool=memcheck --leak-check=full --leak-resolution=high ./barber 10 10 10 100 output

