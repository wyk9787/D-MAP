import subprocess
import sys

SERVER_ADDR = 'localhost'
INPUT_FILE = './src/user/input-6.in'

if len(sys.argv) > 1:
    SERVER_ADDR = sys.argv[1];

cmd = ['time', './src/user/user', SERVER_ADDR, '<', INPUT_FILE]
process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
out, err = process.communicate()
print (out)
