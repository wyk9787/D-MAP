# D-MAP
A distributed version of map procedure that distributes task to multiple
threads running on multiple machines

## Usage

* Run `make` in the root directory, it will compile each program inside of each
directory.

### User

0. To use this system, you need to compile your program into a shared object (.so file) and implement two functions:
   - `bool has_next();` return true of false to indicate if there is more work that needs to be done
   - `char* get_next();` return the next chunk of inputs to the entrance function
   
   See `src/password_6char/` and `src/password_7char` for examples.
1. `cd src/user/`
2. `./user <server_adress>`
3. Answering the prompts by entering the request information. You can find example input file in `src/user/input-6.in` and `src/user/input-7.in`
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

## Example

Below is a simple walkthrough of the password cracker program:

1. Compile the system by running `make`.

2. Server needs to start first: `./src/server/server`

3. For each available worker machine to join the server: 

```
cd ./src/worker/
./worker <SERVER_ADDRESS>
```

4. Then user joins the system and sends the password cracker program (**You need to change the input file (e.g. input-6.in and input-7.in to your own path**):

```
cd ./src/user/
./user <SERVER_ADDRESS> < input-6.in
```

5. Wait for workers to complete the result and send output back to the server then from server to the user

6. Expected output(Order may differ): 

```
vostinar cached
stone chmcls
kington divest
curtsinger evlprf
rebelsky glimmr
wolz oliver
walker robots
osera snthtc
weinman vizwiz
klinge xaqznl
```

