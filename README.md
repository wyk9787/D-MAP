# D-MAP
A distributed version of map procedure that distributes task to multiple
threads running on multiple machines

## Usage

* Run `make` in the root directory, it will compile each program inside of each
directory.

### User

1. `cd src/user/`
2. `./user <server_adress>`
3. Answering the prompts by entering the request information.
4. The user program will send information to server and will wait for output of the
   program sent back.

### Server

1. `cd src/server`
2. `./server`
3. The server will then start listening for connection from both workers and
   user

### Worker

1. `cd src/worker`
2. `./worker <server_address>`
3. The worker will then connect to the server and then wait for jobs


