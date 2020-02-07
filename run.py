"""

    Memory Compression Measurement program runner.
    A python script that automates calling to driver program
    and data gathering and parsing.

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Jan 2020
    
"""

import os
import multiprocessing
import subprocess

#edit this part or create "run_config.py"
flags = []      #additional flags for driver
folders = []    #prior is not empty
files = []      #quits if both empty
compressions = []   #To compile
layouts = []        #To compile
                    #If both empty, compiles everything
interest = []   #Outputs to be processed
outputfile = "default.out"


driver = "./bin/driver"
threads = multiprocessing.cpu_count() + 1
path = os.path.dirname(os.path.abspath(__file__))
of = open(outputfile, "w")
os.chdir(path)

#reads a seperate file to potentially replace above run specs
if os.path.exists("run_config.py"):
    execfile("run_config.py")

p = subprocess.check_call(["make", "clean"])
if (len(compressions) + len(layouts) == 0):
    p = subprocess.check_call(["make", "-j"+str(threads), "default"]
else:
    p = subprocess.check_call(["make", "-j"+str(threads), "bootstrap", "driver"] + compressions + layouts)

output = []
index = 0
l = []
d = ""
if (len(folders) == 0):
    l = files
else:
    d = folders[0]
    l = os.listdir(d)
while True:
    l.sort()
    for f in [f for f in l if os.path.isfile(os.path.join(d, f))]:
        cmd = [driver, "-n", str(threads), "-f", os.path.join(d, f)] + flags
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        o = p.communicate()[0]
        of.write(o)
        output.append(o)
        print f

    print
    #sample data parsing part
    for j in interest:
        print j
        for i in output:
            lines = i.strip("\n").split("\n")
            l0 = lines[0].strip(",").split(",")
            l1 = lines[1].strip(",").split(",")
            for k in xrange(len(l0)):
                if (l0[k] == j):
                    print l1[k]
                    break
        print

    output = []
    index = index + 1
    if (index >= len(folders)):
        break
    d = folders[index]
    l = os.listdir(d)
    of.write("\n")
of.close()