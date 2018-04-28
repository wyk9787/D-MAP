# D-MAP
A distributed version of map procedure that distributes task to multiple
threads running on multiple machines

## Usage

* Run `make` in the root directory, it will compile each program inside of each
directory.

* Run `make` in each directory, it will compile the program in that directory

# Changelog
## 04-24-2018
## ADDED
  - A server program that accepts connection from worker programs and makes a thread to talk to each of them.
  - A worker program that conencts to the server.  
  


