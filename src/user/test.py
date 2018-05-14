import subprocess
import sys
import time
import csv

INPUT_FILES = ['input-6.in', 'input-7.in']
NUM_ITERATION = 5
OUTPUT_FILE = '../../result/result.csv'

if len(sys.argv) < 3:
    print ('Usage: python test.py <SERVER_ADDRESS> <NUM_MACHINE>');
    sys,exit()

SERVER_ADDR = sys.argv[1]
NUM_MACHINE = sys.argv[2]

# with open(OUTPUT_FILE, 'w', newline='') as csv_file:
#    writer = csv.writer(csv_file, delimiter=',')
#    data = ["Input", "NUM_ITERATION", "AVERAGE_TIME", "NUM_MACHINE"]
#    writer.writerow(data)


# Iterate through every input file
for f in INPUT_FILES:
    # Set commands
    cmd = './user ' + SERVER_ADDR + ' < ' + f 
    
    print ("++++++++++++++++++++++++++++++++++++++++")
    print ("START RUNNNING ON INPUT FILE:" + f)
    print ("++++++++++++++++++++++++++++++++++++++++")

    sum_time = 0
    # Iterate through NUM_ITERATION times
    for i in range(NUM_ITERATION):
        print("Start running " + str(i) +"th time")
        # Record starting time
        start_time = time.time()
        # Run the program
        process = subprocess.Popen(cmd, shell=True)
        # Get output
        out, err = process.communicate()
        # Record ending time
        end_time = time.time()
        print("Running for " + str(end_time - start_time))
        sum_time += end_time - start_time
    average_time = sum_time / NUM_ITERATION
    print ("Running on average: " + str(average_time))    
    # Write to a csv file    
    with open(OUTPUT_FILE, 'a', newline='') as csv_file:
        writer = csv.writer(csv_file, delimiter=',')
        data = [f, NUM_ITERATION, average_time, NUM_MACHINE]
        writer.writerow(data)
    print("Finishing writing to the file")
