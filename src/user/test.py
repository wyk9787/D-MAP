import subprocess
import sys

SERVER_ADDR = 'localhost'
INPUT_FILE = './input-6.in'

if len(sys.argv) > 1:
    SERVER_ADDR = sys.argv[1];

cmd = ['time', './user', SERVER_ADDR, '<', INPUT_FILE]
process = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
out, err = process.communicate()
print (out)
