This is me understanding the C Language hope you enjoy and please let me know where I can improve or something I overlooked.
The different packages languages provides is how I dissect a new language
stdio.h 
Functions:
  - printf() -  Prints to screen- These are the handy string operators I used 
  - scanf() - Reads input 
  - fprintf() - Prints output to files or streams
  - fgets() - Reads strings from a file or input

![image](https://github.com/user-attachments/assets/0a0a64f3-d825-44cc-8693-6f0ccb43e97f)


stdlib.h ( Standard Library where all the Memory Fun happens)
Functions:
  malloc() / free() : Memory Allocation to dynamically allocate large blocks of memory and to "free"  deallocate it.
  exit() - Being able to exit out safely with status code.
  atoi() - This one took some getting used to and was where I banged my head against a wall, Math and I have our 
            differences at times. It took a while that the user input as strings had to be converted to integers to 
            be able to properly compare when being processed. Coming across this function saved my head from more 
            bangs against the wall.
  reaalloc() - resizes the memory so that you can dynamically handle ports and the target list. 

string.h
  Functions:
    strdup() - Allocated memory and duplicated a string this was useful for threading
    strtok() - Used to token and parse the CLI args from the user.
    strcmp() - Compares 2 strings 
    strlen() - Length

unistd.h  
  I use everything in Linux so getting aquainted with C POSIX Library was very useful getting to the 
  different system call wrappers for fork, pipe, I/O primitives, 
  Functions:
    getopt() - Parses CLI args
    close() - Close Network Sockets for file descriptors
    read() / write(): Basic I/O 

netdb.h 
  getaddrinfo() : This one so useful and very nice to know how well kept the C language is, that there was already 
                  an ipv6 and/or ipv4 option. To those reading this thinking "duh dumby! hello! Hey! hey! hey !...
                  none of that I'm just dumb self taught programmer leave me alone.

  freeaddrinfo() - By this time of discovering the amount of different methods you have to remember on 
                    simply how to deallocate memory gives me great sympathy and gratitude to the devs
                    that have constructed higher programming languages that allow us now to have to continuously 
                    worrying about memory. JS Devs who live in a perfect universe I'm not sure if i envy you yet still. 
                    PHP guys... ya I envy you guys. I'm ok with someimtes having a garbage collector but perhaps thats just the gopher in me.

arpa/inet 
  inet_pton() - This handy guy converts IP addresses from text to binary before we send off our request
  inet_ntop() - This would simply do the reverse. binary => string

sys/socket.h
  socket() - These I recognized from Python and got to see where the Lynus made his decisions of on the syntax of python. 
  Here I would say Python certainly has it easier. This guy creates a new socket

  connect() - Establishes the remote socket connection.
  bind() - Binds socket with the port
  listen() - Listens for incoming traffic
  setsockopt() - Configures socket options that I used for threading as well
  getsockopt() - Retrieves the options that you called from the initial function.
  select() - Monitors multiple file descriptors, good for handling non-blocking connections which is what we want for port scanning


  This was a very educational process I put myself through. Getting to know the GNU Compiler Collection (GCC)
  I was pleased at how intuitive and verbose the debugging was. I am sure its come a long way since its previous
  predecessors. 
  Using GDB (GNU Debugger) was very powerful little tool to get to know that I am sure will come in handy in     
  future malware analysis, but I'm Ghidra will do the job. This tool allowed me to see that even though my code
  compiled successfully it still had memory allocation errors in which memory was being leaked at certain 
  functions, and the amount of times I forgot to deallocate memory was more than I'd like to admit.  
  


  Please let me know if I missed anything or something doesnt make sense, I am just self taught programmer just trying to learn and build safe code.
  


Cheers to all you keyboard warriors ! 
