#!/usr/bin/env python
# This is topmose file of this excercise.
import sys
import os
os.environ['LD_LIBRARY_PATH'] = '.'
sys.path.append(".")
import time
if len(sys.argv) < 2:
    print("Importing cymoose")
    import cymoose as moose
else:
    print("Importing pymoose")
    import moose

if __name__ == "__main__":
    #a = moose.create("Neutral", "/comp", 1)
    #a = moose.create("Neutral", "/comp/comp1", 1)
    #print a 
    a = moose.Neutral("/comp")
    print a
    b = moose.Compartment('/comp/comp1')
    print b.Vm
    a = moose.Compartment('/comp/comp', 1000000)
    t = time.time()
    paths = moose.wildcardFind('/comp/##')
    for i  in paths[0:10]:
        print i, type(i)
    print("total time taken : {} ".format( time.time() - t))

