/** CSC 501 PA2 Additional task 2: sync issue **/

semaphore resource = 1;
semaphore rmutex = 1;
readcount = 0;
MAX_STRING = 1000;
char buffer[MAX_STRING];

// Function for readers
reader()
{
    wait(rmutex);
    readcount++;
    if (readcount == 1)
        wait(resource);
    signal(rmutex);
    char bufstr[MAX_STRING];
    bufstr = read(buffer); // Read from the buffer
    wait(rmutex);
    readcount--;
    if (readcount == 0)
        signal(resource);
    signal(rmutex);
}

// Function for writers
writer()
{
    wait(resource);
    char bufstr[MAX_STRING];
    bufstr = pid_to_str(); // Get the string representation of its pid
    write(bufstr, buffer); // Clear the buffer then write to the buffer
    update_global_now();
    signal(resource);
}
